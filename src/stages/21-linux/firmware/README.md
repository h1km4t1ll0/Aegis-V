# Linux SBI boot firmware

Derived from the guest-built stage 12 SBI. Build with guest GCC 9.5 using
`kernel/run.sh`; no default QEMU firmware or host compilation is used.

The 256-byte bootinfo layout is shared with stage 12 (TLKBOOT2 at
0x87f00000). For this firmware, elf_addr/elf_size refer to raw Linux Image
bytes at 0x88000000. Validate the Image header, copy it to 0x80200000 and
zero the remaining image_size bytes. The source and destination ranges
must not overlap. Entrypoint is 0x80200000. Firmware and stack/DTB are
reserved below 0x80200000. initramfs starts at 0x90000000; outbox is absent.

Link entry.S and sbi.c at 0x80000000 using link.ld, -mcmodel=medany,
-msmall-data-limit=0 and no libc, PIC or stack protector. Link libfdt's
fdt.c/fdt_ro.c/fdt_rw.c/fdt_wip.c from the pinned Linux source and libgcc.
The SBI source supplies libfdt's memory/string functions.

One hart only. Services are BASE, TIME, SRST and legacy console/timer.
No HSM, remote fences or IPI support is claimed. Linux gets a0=hartid,
a1=DTB and satp=0 in S mode. The loader uses Linux's raw Image protocol,
not Tilck's ELF virtual-address conversion. Source references:

- Linux 6.6 Documentation/riscv/boot.rst
- Linux 6.6 Documentation/riscv/boot-image-header.rst
- stage 12 src/sbi/README.md (shared trap and bootinfo implementation)

Runtime validation remains pending the linux-build and linux.py --test gates.
