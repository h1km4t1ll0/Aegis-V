/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck_gen_headers/config_mm.h>

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>
#include <tilck/common/utils.h>

#include <tilck/kernel/process.h>
#include <tilck/kernel/process_mm.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/fs/devfs.h>
#include <tilck/kernel/syscalls.h>

#include <sys/mman.h>      // system header

char page_size_buf[PAGE_SIZE] ALIGNED_AT(PAGE_SIZE);

static void
brk_syscall_int(struct process *pi, void *new_brk)
{
   void *vaddr;

   ASSERT(!is_preemption_enabled());

   if (new_brk < pi->brk) {

      /* we have to free pages */

      for (void *p = new_brk; p < pi->brk; p += PAGE_SIZE) {
         unmap_page(pi->pdir, p, true);
      }

      pi->brk = new_brk;
      return;
   }

   vaddr = pi->brk;

   while (vaddr < new_brk) {

      if (is_mapped(pi->pdir, vaddr))
         return; // error: vaddr is already mapped!

      vaddr += PAGE_SIZE;
   }

   /* OK, everything looks good here */

   vaddr = pi->brk;

   while (vaddr < new_brk) {

      void *kernel_vaddr = kmalloc(PAGE_SIZE);

      if (!kernel_vaddr)
         break; /* we've allocated as much as possible */

      /* Aegis-V (12-tilck README): zeroed, as Linux; kmalloc'd pages keep
       * what they held before (pipe buffers, freed user pages) */
      bzero(kernel_vaddr, PAGE_SIZE);

      const ulong paddr = LIN_VA_TO_PA(kernel_vaddr);

      if (map_page(pi->pdir, vaddr, paddr, PAGING_FL_RWUS) != 0) {
         kfree2(kernel_vaddr, PAGE_SIZE);
         break;
      }

      vaddr += PAGE_SIZE;
   }

   /* We're done. */
   pi->brk = vaddr;
}

void *sys_brk(void *new_brk)
{
   struct task *ti = get_curr_task();
   struct process *pi = ti->pi;

   if (!new_brk)
      return pi->brk;

   // TODO: check if Linux accepts non-page aligned addresses.
   // If yes, what to do? how to approx? truncation, round-up/round-down?
   if ((ulong)new_brk & OFFSET_IN_PAGE_MASK)
      return pi->brk;

   if (new_brk < pi->initial_brk)
      return pi->brk;

   if ((ulong)new_brk >= MAX_BRK)
      return pi->brk;

   if (new_brk == pi->brk)
      return pi->brk;

   /*
    * Disable preemption to avoid any threads to mess-up with the address space
    * of the current process (i.e. they might call brk(), mmap() etc.)
    */

   disable_preemption();
   {
      struct user_mapping *br = pi->mi->brk_region;

      ASSERT(br != NULL);
      brk_syscall_int(pi, new_brk);

      /* Keep the brk region mapping's length in sync with the new break */
      br->len = (ulong)pi->brk - br->vaddr;
   }
   enable_preemption();
   return pi->brk;
}

static int
create_process_mmap_heap(struct process *pi)
{
   struct mappings_info *mi = pi->mi;
   struct kmalloc_heap *mmap_heap;
   void *valloc_func = &user_map_zero_page;
   void *vfree_func = &user_unmap_zero_page;

   ASSERT(mi);
   ASSERT(!mi->mmap_heap);

   if (!(mmap_heap = kzmalloc(kmalloc_get_heap_struct_size())))
      return -ENOMEM;

   mi->mmap_heap = mmap_heap;
   mi->mmap_heap_size = USER_MMAP_MIN_SZ;

   if (MMAP_NO_COW) {
      valloc_func = &user_valloc_and_map;
      vfree_func = &user_vfree_and_unmap;
   }

   bool success =
      kmalloc_create_heap(mmap_heap,
                          USER_MMAP_BEGIN,
                          mi->mmap_heap_size,
                          PAGE_SIZE,
                          PAGE_SIZE,            /* alloc block size */
                          false,                /* linear mapping */
                          NULL,                 /* metadata_nodes */
                          valloc_func,
                          vfree_func);

   if (!success) {
      kfree2(mi->mmap_heap, kmalloc_get_heap_struct_size());
      mi->mmap_heap = NULL;
      mi->mmap_heap_size = 0;
      return -ENOMEM;
   }

   return 0;
}

static inline void
mmap_err_case_free(struct process *pi, void *ptr, size_t actual_len)
{
   per_heap_kfree(pi->mi->mmap_heap,
                  ptr,
                  &actual_len,
                  KFREE_FL_ALLOW_SPLIT |
                  KFREE_FL_MULTI_STEP  |
                  KFREE_FL_NO_ACTUAL_FREE);
}

static struct user_mapping *
mmap_on_user_heap(struct process *pi,
                  size_t *actual_len_ref,
                  fs_handle handle,
                  u32 per_heap_kmalloc_flags,
                  size_t off,
                  int prot)
{
   void *res;
   struct user_mapping *um;

   while (true) {

      struct kmalloc_heap *new_heap;
      struct kmalloc_heap *h = pi->mi->mmap_heap;
      size_t heap_sz = pi->mi->mmap_heap_size;

      res = per_heap_kmalloc(h,
                             actual_len_ref,
                             per_heap_kmalloc_flags);

      if (LIKELY(res != NULL))
         break;        /* great! */

      if (heap_sz == USER_MMAP_MAX_SZ)
         return NULL; /* cannot expand the heap more than that */

      new_heap = kmalloc_heap_dup_expanded(h, heap_sz * 2);

      if (!new_heap)
         return NULL; /* no enough memory */

      pi->mi->mmap_heap_size = heap_sz * 2;
      pi->mi->mmap_heap = new_heap;
      kmalloc_destroy_heap(h);
   }

   /* NOTE: here `handle` might be NULL (zero-map case) and that's OK */
   um = process_add_user_mapping(handle, res, *actual_len_ref, off, prot);

   if (!um) {
      mmap_err_case_free(pi, res, *actual_len_ref);
      return NULL;
   }

   return um;
}

long
sys_mmap_pgoff(void *addr, size_t len, int prot,
               int flags, int fd, size_t pgoffset)
{
   size_t actual_len;
   int rc, fl;
   u32 per_heap_kmalloc_flags = KMALLOC_FL_MULTI_STEP | PAGE_SIZE;
   struct fs_handle_base *handle = NULL;
   struct user_mapping *um = NULL;
   struct task *curr = get_curr_task();
   struct process *pi = curr->pi;

   if ((flags & MAP_PRIVATE) && (flags & MAP_SHARED))
      return -EINVAL; /* non-sense parameters */

   if (!len)
      return -EINVAL;

   if (addr)
      return -EINVAL; /* addr != NULL not supported */

   if (!(prot & PROT_READ))
      return -EINVAL;

   actual_len = pow2_round_up_at(len, PAGE_SIZE);

   if (fd == -1) {

      if (!(flags & MAP_ANONYMOUS))
         return -EINVAL;

      if (flags & MAP_SHARED)
         return -EINVAL; /* MAP_SHARED not supported for anonymous mappings */

      if (!(flags & MAP_PRIVATE))
         return -EINVAL;

      if ((prot & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE))
         return -EINVAL;

      if (pgoffset != 0)
         return -EINVAL; /* pgoffset != 0 does not make sense here */

   } else {

      if (!(flags & MAP_SHARED))
         return -EINVAL;

      handle = get_fs_handle(fd);

      if (!handle)
         return -EBADF;

      fl = handle->fl_flags;

      if ((prot & (PROT_READ | PROT_WRITE)) == 0)
         return -EINVAL; /* nor read nor write prot */

      if ((prot & (PROT_READ | PROT_WRITE)) == PROT_WRITE)
         return -EINVAL; /* disallow write-only mappings */

      if (prot & PROT_WRITE) {
         if (!(fl & O_WRONLY) && (fl & O_RDWR) != O_RDWR)
            return -EACCES;
      }

      per_heap_kmalloc_flags |= KMALLOC_FL_NO_ACTUAL_ALLOC;
   }

   if (!pi->mi->mmap_heap) {
      if ((rc = create_process_mmap_heap(pi)))
         return rc;
   }

   disable_preemption();
   {
      um = mmap_on_user_heap(pi,
                             &actual_len,
                             handle,
                             per_heap_kmalloc_flags,
                             pgoffset << PAGE_SHIFT,
                             prot);
   }
   enable_preemption();

   if (!um)
      return -ENOMEM;

   ASSERT(actual_len == pow2_round_up_at(len, PAGE_SIZE));

   if (handle) {

      if ((rc = vfs_mmap(um, pi->pdir, 0))) {

         /*
          * Everything was apparently OK and the allocation in the user virtual
          * address space succeeded, but for some reason the actual mapping of
          * the device to the user vaddr failed.
          */

         disable_preemption();
         {
            mmap_err_case_free(pi, um->vaddrp, actual_len);
            process_remove_user_mapping(um);
         }
         enable_preemption();
         return rc;
      }


   } else {

      if (MMAP_NO_COW)
         bzero(um->vaddrp, actual_len);
   }

   return (long)um->vaddr;
}

long sys_mmap(void *addr, size_t len, int prot,
              int flags, int fd, size_t offset)
{
   if (UNLIKELY(offset & (~PAGE_MASK)))
      return -EINVAL;

   return sys_mmap_pgoff(addr, len, prot, flags, fd,
                         offset >> PAGE_SHIFT);
}

/*
 * Aegis-V (12-tilck README): the mmap()ed user_mapping with the lowest start
 * that overlaps [s, e), or NULL.
 */
static struct user_mapping *
first_mmap_overlap(struct process *pi, ulong s, ulong e)
{
   struct user_mapping *pos, *best = NULL;

   list_for_each_ro(pos, &pi->mi->mappings, pi_node) {

      if (pos->type != USER_MAPPING_MMAP)
         continue;

      if (pos->vaddr < e && s < pos->vaddr + pos->len)
         if (!best || pos->vaddr < best->vaddr)
            best = pos;
   }

   return best;
}

/* Un-map [vaddr, vaddr + actual_len), which lies inside `um` */
static int
munmap_one(struct process *pi, struct user_mapping *um,
           ulong vaddr, size_t actual_len)
{
   int rc;
   u32 kfree_flags = KFREE_FL_ALLOW_SPLIT | KFREE_FL_MULTI_STEP;
   struct user_mapping *um2 = NULL;
   void *vaddrp = TO_PTR(vaddr);
   const ulong um_vend = um->vaddr + um->len;

   if (actual_len == um->len) {

      process_remove_user_mapping(um);

   } else {

      /* partial un-map */

      if (vaddr == um->vaddr) {

         /* unmap the beginning of the chunk */
         um->vaddr += actual_len;
         um->off += actual_len;
         um->len -= actual_len;

      } else if (vaddr + actual_len == um_vend) {

         /* unmap the end of the chunk */
         um->len -= actual_len;

      } else {

         /* Unmap something at the middle of the chunk */

         /* Shrink the current struct user_mapping */
         um->len = vaddr - um->vaddr;

         /* Create a new struct user_mapping for its 2nd part */
         um2 = process_add_user_mapping(
            um->h,
            (void *)(vaddr + actual_len),
            (um_vend - (vaddr + actual_len)),
            um->off + um->len + actual_len,
            um->prot
         );

         if (!um2) {

            /*
             * Oops, we're out-of-memory! No problem, revert um->page_count
             * and return -ENOMEM. Linux is allowed to do that.
             */
            um->len = um_vend - um->vaddr;
            return -ENOMEM;
         }
      }
   }

   if (um->h) {

      kfree_flags |= KFREE_FL_NO_ACTUAL_FREE;
      rc = vfs_munmap(um, vaddrp, actual_len);

      /*
       * If there's an actual user_mapping entry, it means um->h's fops MUST
       * HAVE mmap() implemented. Therefore, we MUST REQUIRE munmap() to be
       * present as well.
       */

      ASSERT(rc != -ENODEV);
      (void) rc; /* prevent the "unused variable" Werror in release */

      if (um2)
         vfs_mmap(um2, pi->pdir, VFS_MM_DONT_MMAP);
   }

   size_t freed_len = actual_len;

   per_heap_kfree(pi->mi->mmap_heap,
                  vaddrp,
                  &freed_len,
                  kfree_flags);

   ASSERT(freed_len == actual_len);
   return 0;
}

/*
 * Aegis-V (12-tilck README): as Linux, the range may cover the end of one
 * mmap() and the start of the next (GCC's ggc-page unmaps adjacent free pages
 * together, whichever mmap() they came from) and holes. Upstream took only
 * the mapping containing `vaddrp`: its "middle" case then shrank that mapping
 * and freed the whole range in the mmap heap, so a later mmap() handed out
 * pages still mapped (and used) by the first mapping. Each mapping that
 * overlaps the range is now un-mapped for its own part; the holes are skipped.
 */
static int munmap_int(struct process *pi, void *vaddrp, size_t len)
{
   int rc;
   struct user_mapping *um;
   ulong s = (ulong) vaddrp;
   const ulong e = s + pow2_round_up_at(len, PAGE_SIZE);

   ASSERT(!is_preemption_enabled());

   if (!first_mmap_overlap(pi, s, e)) {

      /*
       * We just don't have any user_mappings containing [vaddrp, vaddrp+len).
       * Just ignore that and return 0 [linux behavior].
       */

      printk("[%d] Un-map unknown chunk at [%p, %p)\n",
             pi->pid, TO_PTR(s), TO_PTR(e));
      return 0;
   }

   while ((um = first_mmap_overlap(pi, s, e))) {

      const ulong a = MAX(s, um->vaddr);
      const ulong b = MIN(e, um->vaddr + um->len);

      if ((rc = munmap_one(pi, um, a, b - a)))
         return rc;

      s = b;
   }

   return 0;
}

int sys_munmap(void *vaddrp, size_t len)
{
   int rc;
   ulong vaddr = (ulong) vaddrp;
   struct task *curr = get_curr_task();
   struct process *pi = curr->pi;

   if (!len || !pi->mi->mmap_heap)
      return -EINVAL;

   if (!IN_RANGE(vaddr,
                 USER_MMAP_BEGIN,
                 USER_MMAP_BEGIN + pi->mi->mmap_heap_size))
   {
      return -EINVAL;
   }

   disable_preemption();
   {
      rc = munmap_int(pi, vaddrp, len);
   }
   enable_preemption();
   return rc;
}

/*
 * Aegis-V (12-tilck README): mremap() of an mmap()ed range shrinks it in
 * place (its tail is un-mapped, as Linux does) and does not grow it (-ENOMEM,
 * nothing is reserved after a mapping); upstream: -ENOSYS. musl 1.1.24's
 * realloc() of a block it got from mmap() calls mremap() and, when that
 * fails, copies the OLD size into a new malloc() of the new size, also when
 * shrinking: it wrote past the new block over the rest of the heap (GCC's
 * cc1 shrinks big tables with xrealloc()).
 */
long sys_mremap(void *old_addr, size_t old_len, size_t new_len, int flags,
                void *new_addr)
{
   const ulong vaddr = (ulong) old_addr;
   const size_t old_sz = pow2_round_up_at(old_len, PAGE_SIZE);
   const size_t new_sz = pow2_round_up_at(new_len, PAGE_SIZE);

   (void) new_addr;

   if ((vaddr & (PAGE_SIZE - 1)) || !new_len)
      return -EINVAL;

   if (flags & ~1) /* only MREMAP_MAYMOVE (1): no MREMAP_FIXED */
      return -EINVAL;

   if (new_sz > old_sz)
      return -ENOMEM;

   if (new_sz < old_sz) {

      int rc = sys_munmap(TO_PTR(vaddr + new_sz), old_sz - new_sz);

      if (rc)
         return rc;
   }

   return (long) vaddr;
}
