/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck_gen_headers/config_mm.h>
#include <tilck/common/basic_defs.h>
#include <tilck/common/assert.h>
#include <tilck/common/printk.h>
#include <tilck/common/string_util.h>
#include <tilck/common/utils.h>
#include <tilck/kernel/hal.h>
#include <tilck/kernel/interrupts.h>
#include <tilck/kernel/sched.h>
#include <tilck/kernel/kmalloc.h>

void handle_inst_illegal_fault_int(regs_t *r, const char *fault_name);
void asm_save_fpu(void *buf);
void asm_restore_fpu(void *buf);

struct riscv_fpu_regs {
   u64 f[32];
   u32 fcsr;
};

static void handle_inst_illegal_fpu_fault(regs_t *r)
{
   const int int_num = r->int_num;
   struct task *curr = get_curr_task();

   if (is_kernel_thread(curr) || in_syscall(curr))
      goto normal_illegal_fault;

   if (r->sstatus & SR_FS)
      goto normal_illegal_fault;

   if (!riscv_cpu_features.isa_exts.f &&
       !riscv_cpu_features.isa_exts.d &&
       !riscv_cpu_features.isa_exts.q)
      goto normal_illegal_fault;

   arch_task_members_t *const arch_fields = get_task_arch_fields(curr);

#if FORK_NO_COW

   ASSERT(arch_fields->fpu_regs != NULL);

   /*
    * With the current implementation, even when the fpu_regs are
    * pre-allocated tasks cannot by default use the FPU. This approach has PROs
    * and CONs.
    *
    *    PROs:
    *       - the kernel doesn't have to save/restore unnecessarily the FPU ctx
    *       - tasks using the FPU can be distinguished from the others
    *       - it is simpler to make fpu_regs pre-allocated work this way
    *
    *    CONs:
    *       - paying the overhead of a not-strictly necessary fault, once.
    */

#else

    /*
     * We can hit this fault at MOST once in the lifetime of a task. These
     * sanity checks ensures that, in no case, we allocated the fpu_regs
     * and then, for any reason, we scheduled the task with FPU disabled.
     */

   if (arch_fields->fpu_regs)
      goto normal_illegal_fault;

   if (!allocate_fpu_regs(arch_fields)) {
      panic("Cannot allocate memory for the FPU context");
   }

#endif

   /*
    * Aegis-V (rung gmp9, 12-tilck README): load the task's new, zeroed
    * context into the FPU before the task runs with FS on. Upstream only
    * set FS: the task then started with the f registers and the fcsr
    * (rounding mode, flags) the last FPU user left in the hardware, e.g. a
    * process that had called fesetround(FE_TOWARDZERO) and exited (mpfr
    * 3.1.4's configure does that, and its tget_d, tget_flt then failed).
    * The buffer is zeroed again with preemption off: a switch after the
    * allocation saved the hardware state into it.
    */
   disable_preemption();
   bzero(arch_fields->fpu_regs, sizeof(struct riscv_fpu_regs));
   r->sstatus |= SR_FS;
   restore_fpu_regs(curr, false);
   enable_preemption();
   return;

normal_illegal_fault:
   handle_inst_illegal_fault_int(r, riscv_exception_names[int_num]);
}

void enable_cpu_features(void)
{
   get_cpu_features();
   hw_fpu_disable();
   set_fault_handler(EXC_INST_ILLEGAL, handle_inst_illegal_fpu_fault);
}

static struct riscv_fpu_regs fpu_kernel_regs;

void save_current_fpu_regs(bool in_kernel)
{
   arch_task_members_t *arch_fields;
   void *buf;

   if (UNLIKELY(in_panic()))
      return;

   arch_fields = get_task_arch_fields(get_curr_task());
   buf = in_kernel ? &fpu_kernel_regs : arch_fields->fpu_regs;

   ASSERT(buf != NULL);
   asm_save_fpu(buf);
}

void restore_fpu_regs(void *task, bool in_kernel)
{
   arch_task_members_t *arch_fields;
   void *buf;

   if (UNLIKELY(in_panic()))
      return;

   arch_fields = get_task_arch_fields((struct task *)task);
   buf = in_kernel ? &fpu_kernel_regs : arch_fields->fpu_regs;

   ASSERT(buf != NULL);
   asm_restore_fpu(buf);
}

void restore_current_fpu_regs(bool in_kernel)
{
   restore_fpu_regs(get_curr_task(), in_kernel);
}

bool allocate_fpu_regs(arch_task_members_t *arch_fields)
{
   ASSERT(arch_fields->fpu_regs == NULL);

   arch_fields->fpu_regs = kzalloc_obj(struct riscv_fpu_regs);
   arch_fields->fpu_regs_size = sizeof(struct riscv_fpu_regs);

   if (!arch_fields->fpu_regs)
      return false;

   return true;
}

/*
 * Aegis-V: called for a new task CHILD of PARENT (fork, vfork, clone). The
 * child's regs_t is a copy of the parent's, FS included, but upstream left
 * the child without an FPU context buffer: it then ran with FS on and the
 * hardware state of whatever task ran before, and its own f registers and
 * fcsr were never saved at a switch. Like Linux, the child gets a copy of
 * the parent's FPU context; the parent's live state is in the hardware
 * when it is the current task.
 */
bool arch_fork_fpu_regs(arch_task_members_t *child, struct task *parent)
{
   arch_task_members_t *pa = get_task_arch_fields(parent);

   if (!pa->fpu_regs)
      return true; /* the parent never used the FPU: FS is off */

   if (parent == get_curr_task())
      save_current_fpu_regs(false);

   if (!allocate_fpu_regs(child))
      return false;

   memcpy(child->fpu_regs, pa->fpu_regs, sizeof(struct riscv_fpu_regs));
   return true;
}

static volatile bool in_fpu_context;

void fpu_context_begin(void)
{
   disable_preemption();

   /* NOTE: nested FPU contexts are NOT allowed (unless we're in panic) */

   if (LIKELY(!in_panic())) {
      ASSERT(!in_fpu_context);
   }

   in_fpu_context = true;
   hw_fpu_enable();
   save_current_fpu_regs(true);
}

void fpu_context_end(void)
{
   ASSERT(in_fpu_context);

   restore_current_fpu_regs(true);
   hw_fpu_disable();

   in_fpu_context = false;
   enable_preemption();
}

void init_fpu_memcpy(void)
{
   /* STUB function: do nothing */
}
