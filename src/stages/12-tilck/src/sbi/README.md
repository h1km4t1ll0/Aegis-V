# SBI shim (M mode)

Our firmware for booting Tilck on QEMU `virt` (no OpenSBI). `/bin/tcc7b` builds it on Kronos with `sbi.rsp` into the raw image `/build/tilck/sbi.bin` (linked at `0x80000000`, libfdt `fdt.c fdt_ro.c fdt_rw.c fdt_wip.c` from dtc v1.7.2). UART `sbi built`, `snap sbi`; `kronos.py` copies it to `artifacts/sbi.bin`, and `src/scripts/tilck.py` passes it to QEMU as `-bios`.

## Memory

| Address | What | Who puts it there |
|---|---|---|
| `0x80000000` | `sbi.bin` (code, data, bss: M stack 16 KB, DTB buffer 64 KB); must stay under `0x80100000` | QEMU `-bios` |
| `0x80100000` | Tilck's own DTB copy (1 MB below the kernel, `prepare_early_mapping`) | Tilck |
| `0x80200000` | Tilck image: each `PT_LOAD` at `p_vaddr - 0xFFFFFFFF00000000` | this shim |
| `0x87F00000` | bootinfo block | host, `-device loader` |
| `0x88000000` | `tilck.elf` bytes | host, `-device loader` |
| `0x90000000` | initrd (optional); must end below `0x100000000` | host, `-device loader` |
| `0x240000000` | outbox region (`tilck.py` default: the last 1 GB of `-m 8G`) | written by Tilck's `/dev/outbox`, read by the host |
| `0x27FE00000` | QEMU's own DTB (inside the outbox region: only this shim reads it, before Tilck starts) | QEMU |

`0x80000000`–`0x80200000` goes into the DTB as `/reserved-memory/mmode_resv0@80000000` (`no-map`, as OpenSBI). A `/memreserve/` entry is not enough: Tilck's `fdt_parse_reserved_memory()` returns before reading them when there is no `/reserved-memory` node. The outbox region goes in the same way as `/reserved-memory/outbox@<addr in hex>` (`no-map`); Tilck's outbox module finds it by name (`/reserved-memory/outbox`).

bootinfo (little endian, 256 bytes): `u64 magic` = `0x32544F4F424B4C54` ("TLKBOOT2"), `u64 elf_addr`, `u64 elf_size`, `u64 initrd_addr`, `u64 initrd_size` (0: none), `u64 outbox_addr`, `u64 outbox_size` (0: no outbox), `char bootargs[200]` (NUL-terminated, empty: none). "TLKBOOT1" (no outbox fields, `bootargs[216]`) is refused: `sbi.c` and `tilck.py` change together.

## Boot

`sbi-entry.s`: clear bss, M stack in `mscratch`, `mtvec` = trap entry, `sbi_main(hartid, dtb)`. `sbi.c`:

- PMP entry 0: NAPOT over all memory, RWX.
- `medeleg` = `0xb1ff` (everything except ecall from S/M), `mideleg` = SSIP|STIP|SEIP, `mcounteren` = CY|TM|IR (Tilck lets U mode read them too), `mtimecmp` = max, `mie` = 0.
- Load the ELF, then `fdt_open_into` the QEMU DTB into the 64 KB buffer; `/chosen` gets `linux,initrd-start`/`linux,initrd-end` (u64, only if `initrd_size`) and `bootargs` (only if set); add the reserved-memory nodes (`outbox@…` only if `outbox_size`). UART `sbi: entry … initrd … size … outbox … size …`.
- `satp` = 0, `mstatus.MPP` = S, `mret` to `e_entry - 0xFFFFFFFF00000000` with `a0` = hartid, `a1` = DTB.

## Traps (in M mode)

- ecall from S: `BASE` 0x10 (spec version 0.2, impl id/version 0, `probe_extension`, `mvendorid`/`marchid`/`mimpid` CSRs); `TIME` set_timer; `SRST` (shutdown `0x5555`, cold/warm reboot `0x7777` to `sifive_test` at `0x100000`); legacy 0x00 set_timer (Tilck's `sbi_set_timer` uses it), 0x01 putchar and 0x02 getchar (16550 at `0x10000000`), 0x08 shutdown; anything else `SBI_ERR_NOT_SUPPORTED`.
- set_timer: CLINT `mtimecmp` (`0x2004000`) = value, clear `mip.STIP`, set `mie.MTIE`. Machine timer interrupt: clear `mie.MTIE`, set `mip.STIP`.
- Any other trap prints `sbi trap mepc … mtval …` and `sbi fail: mcause …` and stops.

## tcc layout and codegen notes

- tcc has no linker script. A custom `"a"` section (`.sbi.entry`) is laid out before `.data.ro` and `.text`, so `_start` is the first byte of the raw image.
- MMIO stores go through an address in a register (`mmio_w8/32/64`): tcc mob's `store()` to a constant address uses `s0` (the frame pointer) as scratch.
- The bootinfo pointer comes from an `unsigned long`: tcc mob loads a pointer constant with bit 31 set by `lui` and sign-extends it (`0xffffffff87f00000`).
