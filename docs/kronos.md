# Kronos

Date: 2026-08-28

Kronos is a mini Linux RV64 ABI kernel used so TinyCC 0.9.26 can compile itself as a normal Unix program: paths, headers, libc, ELF. No `pnut_tcc.c` amalgam, no `#define float int`, no Simon flat ramfs, no raw `do` as the run model.

It is built **inside Simon** by `tcc-pnut`, then the **same QEMU process** resets into Kronos.

## Success

UART shows `Kronos`, then `tcc version 0.9.26`, then `cmp ok`:

```text
/bin/tcc3 ≡ /bin/tcc4
```

`/bin/tcc` is the pnut-built seed (ELF-wrapped `tcc-pnut`). `/bin/tcc2` is that seed compiling `src/stages/07-tcc`. `/bin/tcc3` is tcc2 compiling TCC. `/bin/tcc4` is tcc3 compiling TCC again. `tcc2 ≠ tcc3` is expected (different parents). The fixed point is `tcc3 ≡ tcc4`.

```bash
python3 -u src/scripts/kronos.py --tcc-self-k
```

Needs `src/stages/08-pnut/artifacts/tcc-pnut.bin` from `src/scripts/simon.py`.

## Not in this slice

Not a full Linux, not xv6, not OpenSBI/S-mode. No virtio-blk, network, SMP, or SATP. No musl. Hex0…pnut stay on Simon; they are not re-hosted on Kronos here.

## Platform

- QEMU: `qemu-system-riscv64 -M virt`. Simon image is `-bios`; after `system_reset` the host writes Kronos firmware at `0x80000000` and continues.
- Privileges: kernel M-mode, userland U-mode, `ecall` = Linux RV64 syscall.
- User ELF is linked with `-Wl,-Ttext=0x10000` (TCC 0.9.26 `lui` sign-extends bit 31).
- KFS1 ramfs at `0x80200000`.

## Layout

Sources: `src/stages/09-kronos/{src,tests}/`. Driver: `src/scripts/kronos.py`. Binary: `src/stages/09-kronos/artifacts/` (`kernel.bin`, `image.bin` firmware).
