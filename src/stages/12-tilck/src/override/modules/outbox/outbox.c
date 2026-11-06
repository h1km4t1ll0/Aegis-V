/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Aegis-V: /dev/outbox (modelled on modules/null). Every write() appends its
 * bytes to a RAM region that the host dumps (src/scripts/tilck.py): the
 * region is the SBI shim's /reserved-memory/outbox@ADDR node, made from the
 * bootinfo block. One stream per boot: nothing is ever reset, a write past
 * the end is short, then -ENOSPC. Every close() prints
 * "outbox ADDR SIZE" (SIZE = all bytes written since boot).
 */

#include <tilck/common/basic_defs.h>
#include <tilck/common/printk.h>
#include <tilck/common/string_util.h>
#include <tilck/kernel/hal.h>
#include <tilck/kernel/sched.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/modules.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/paging.h>
#include <tilck/kernel/fs/devfs.h>
#include <tilck/kernel/fs/vfs.h>
#include <3rd_party/fdt_helper.h>
#include <libfdt.h>

/*
 * The linear mapping maps reserved memory read-only, so the region gets its
 * own RW window: above the hi vmem area, below the top of the Sv39 lower
 * half (0x4000000000).
 */
#define OUTBOX_VA          (BASE_VA + 64ul * 1024 * MB)
#define OUTBOX_MAX_SIZE    (64ul * 1024 * MB)

static ulong outbox_pa;
static size_t outbox_cap;
static size_t outbox_used;

static ssize_t outbox_write(fs_handle h, char *buf, size_t size, offt *pos)
{
   size_t off;

   if (!size)
      return 0;

   disable_preemption();
   {
      off = outbox_used;
      size = MIN(size, outbox_cap - off);
      memcpy((char *)OUTBOX_VA + off, buf, size);
      outbox_used = off + size;
   }
   enable_preemption();

   return size ? (ssize_t)size : -ENOSPC;
}

static const struct file_ops static_ops_outbox = {
   .write = outbox_write,
};

static void outbox_on_close(int minor, void *extra)
{
   printk("outbox %#lx %lu\n", outbox_pa, (ulong)outbox_used);
}

static int
outbox_create_device_file(int minor,
                          enum vfs_entry_type *type,
                          struct devfs_file_info *nfo)
{
   *type = VFS_CHAR_DEV;
   nfo->fops = &static_ops_outbox;
   nfo->destroy_extra = outbox_on_close;
   return 0;
}

static void init_outbox(void)
{
   void *fdt = fdt_get_address();
   struct driver_info *di;
   u64 addr, size;
   size_t mapped;
   int node, rc;

   node = fdt_path_offset(fdt, "/reserved-memory/outbox");

   if (node < 0)
      return; /* no outbox region in the bootinfo: no /dev/outbox */

   if (fdt_get_node_addr_size(fdt, node, 0, &addr, &size) ||
       !size || size > OUTBOX_MAX_SIZE ||
       !IS_PAGE_ALIGNED(addr) || !IS_PAGE_ALIGNED(size))
   {
      panic("outbox: bad /reserved-memory/outbox node");
   }

   mapped = map_pages(get_kernel_pdir(),
                      (void *)OUTBOX_VA,
                      (ulong)addr,
                      (size_t)size >> PAGE_SHIFT,
                      PAGING_FL_RW | PAGING_FL_BIG_PAGES_ALLOWED);

   if (mapped != ((size_t)size >> PAGE_SHIFT))
      panic("outbox: unable to map %#lx", (ulong)addr);

   outbox_pa = (ulong)addr;
   outbox_cap = (size_t)size;

   if (!(di = kzalloc_obj(struct driver_info)))
      panic("outbox: not enough memory for struct driver_info");

   di->name = "outbox";
   di->create_dev_file = outbox_create_device_file;
   register_driver(di, -1);

   if ((rc = create_dev_file("outbox", di->major, 0, NULL)) < 0)
      panic("outbox: unable to create /dev/outbox (error: %d)", rc);

   printk("outbox: %#lx, %lu MB\n", outbox_pa, (ulong)(outbox_cap / MB));
}

static struct module outbox_module = {
   .name = "outbox",
   .priority = MOD_null_prio + 1,
   .init = &init_outbox,
};

REGISTER_MODULE(&outbox_module);
