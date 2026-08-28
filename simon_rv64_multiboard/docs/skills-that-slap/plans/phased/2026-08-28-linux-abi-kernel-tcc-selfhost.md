# Linux ABI kernel + TCC self-host — Phased Implementation Plan

> **Executors:** Use surgical-plan-execution (recommended) or subagent-driven-development.
> Complete each phase (including its gate) before the next unless marked parallel-safe.
> Track progress with `- [ ]` checkboxes.
>
> **Do not git commit** unless the user explicitly asks. Skip every Commit step.

**Goal:** На QEMU virt новое ядро грузит ELF, seed-TCC собирает TCC 0.9.26 из `stages/08-tcc`, `tcc2 -version` печатает `tcc version 0.9.26`, `cmp tcc2 tcc3` совпадает.

**Architecture:** Ядро M-mode raw binary с `0x80000000`, user U-mode, `ecall` = Linux RV64. Производитель ядра — `tcc-pnut` в госте Simon (`--from 09`). Self-host идёт во **втором** QEMU (`-bios firmware.bin`), не в Simon `do`. Ramfs — дерево путей в firmware после 2MiB pad (TCC `-oformat=binary` не кладёт `.bss` в файл, поэтому `&_end` из спеки реализуется как фиксированный `K_RAMFS=0x80200000`, ядро+BSS < 2MiB).

**Tech stack:** TCC 0.9.26 RV (синтаксис: 3 операнда, `lla`/`jal ra`, нет `ret`/`mret`/`csrw` — CSR через `.long`), QEMU `-M virt`, Python-драйвер UART/QMP как `scripts/run_m2_qemu_test.py`.

**Phasing strategy:** risk first, затем вертикальные срезы. Phase 1 доказывает, что tcc-pnut умеет линковать firmware на `0x80000000` и печатать на UART. Дальше каждая фаза — наблюдаемый UART-гейт спеки (hi → длина tcc.c → cmp).

| Phase | Name | Deliverable |
|-------|------|-------------|
| 1 | Boot | UART `k 10-k`, ядро крутится в M-mode |
| 2 | U-mode ELF | UART содержит `hi` от userspace `write(1,…)` |
| 3 | Path ramfs | UART `tcc.c <N>` равен `len(stages/08-tcc/tcc.c)` |
| 4 | Self-host | `tcc version 0.9.26` и `cmp` tcc2/tcc3 exit 0 |

**Rule:** Finish Phase *k* gate before starting Phase *k+1*.

**Spec:** `simon_rv64_multiboard/docs/superpowers/specs/2026-08-28-linux-abi-kernel-tcc-selfhost-design.md`

---

## Allowed paths

Только эти пути. Новые файлы — Create. Существующие — Modify. Артефакты `build/` не коммитить.

- `simon_rv64_multiboard/stages/08-tcc/tccelf.c`
- `simon_rv64_multiboard/stages/10-k/k.h`
- `simon_rv64_multiboard/stages/10-k/entry.s`
- `simon_rv64_multiboard/stages/10-k/uart.c`
- `simon_rv64_multiboard/stages/10-k/klib.c`
- `simon_rv64_multiboard/stages/10-k/kmain.c`
- `simon_rv64_multiboard/stages/10-k/trap.c`
- `simon_rv64_multiboard/stages/10-k/syscall.c`
- `simon_rv64_multiboard/stages/10-k/proc.c`
- `simon_rv64_multiboard/stages/10-k/elf.c`
- `simon_rv64_multiboard/stages/10-k/ramfs.c`
- `simon_rv64_multiboard/stages/10-k/kmem.c`
- `simon_rv64_multiboard/stages/10-k/k.rsp`
- `simon_rv64_multiboard/stages/10-k/user/hi.s`
- `simon_rv64_multiboard/stages/10-k/user/hi.rsp`
- `simon_rv64_multiboard/stages/10-k/user/sys.s`
- `simon_rv64_multiboard/stages/10-k/user/readlen.c`
- `simon_rv64_multiboard/stages/10-k/user/readlen.rsp`
- `simon_rv64_multiboard/stages/10-k/user/cmp.c`
- `simon_rv64_multiboard/stages/10-k/user/crt.s`
- `simon_rv64_multiboard/stages/10-k/user/libc.c`
- `simon_rv64_multiboard/stages/10-k/user/init.c`
- `simon_rv64_multiboard/stages/10-k/user/tcc-config.h`
- `simon_rv64_multiboard/stages/10-k/user/include/` (POSIX headers listed in Task 12)
- `simon_rv64_multiboard/scripts/run_k_qemu_test.py`
- `simon_rv64_multiboard/scripts/pack_kfs.py`
- `simon_rv64_multiboard/scripts/raw_to_elf.py`
- `simon_rv64_multiboard/scripts/extract_files_pl.py`
- `simon_rv64_multiboard/scripts/pack.sh` — **только** если сработал Task 14 Step 2 (удалить `#define float int` / `#define double int` в python-heredoc amalgamation). Не трогать остальной pack.sh.

Host commands (не правки репо): `bash scripts/pack.sh` только чтобы пересобрать amalgamation `pnut_tcc.c` после патча `tccelf.c`; `python3 -u scripts/run_m2_qemu_test.py --from 09` как пререквизит кэша.

**Verify after each task:**

```bash
cd /Users/hikmatillo/WebstormProjects/Aegis-V
git diff --name-only && git ls-files --others --exclude-standard
```

Ожидание: имена ⊆ Allowed paths (плюс неотслеживаемые `build/**`, их не добавлять в git).

---

## Out of scope

- hex0…mes, gcc 4.x, musl, OpenSBI/S-mode, virtio, сеть, SMP, SATP/MMU
- правки алгоритма `stages/08-tcc` кроме одного стража в `tccelf.c` (ниже)
- Simon `kernel/*.S`, `boards/*`, `scripts/pack.sh` (не редактировать; только запускать)
- `pnut_tcc_libc.c`, `#define float int`, срез `./` в `tcc_open`, force `0x82000000` как единственный режим
- коммиты без санкции пользователя

---

## File map

| File | Phase | Responsibility |
|------|-------|----------------|
| `stages/08-tcc/tccelf.c` | 1 | Не затирать `has_text_addr` / `oformat` у EXE |
| `stages/10-k/k.h` | 1 | Константы памяти, UART, errno, a7 |
| `stages/10-k/entry.s` | 1, +trap в 2 | Стек, BSS, `mtvec`, вызов `kmain` |
| `stages/10-k/uart.c` `klib.c` `kmain.c` | 1 | Баннер, паника |
| `stages/10-k/k.rsp` | 1 | Флаги tcc-pnut: binary @ `0x80000000` |
| `trap.c` `syscall.c` `proc.c` `elf.c` `kmem.c` | 2 | U-mode, write/exit, ELF, brk |
| `user/hi.s` `hi.rsp` | 2 | Userspace `write(1,"hi\n",3)` |
| `user/sys.s` | 3 | `ecall` helper для C без GNU asm |
| `user/cmp.c` | 4 | Байт-сравнение tcc2/tcc3 |
| `ramfs.c` | 2 | Парсер KFS1; Phase 3 добавляет fd/пути |
| `user/readlen.c` | 3 | Печать длины `/src/tcc/tcc.c` |
| `scripts/pack_kfs.py` | 2 | Архив KFS1 |
| `scripts/run_k_qemu_test.py` | 1–4 | Драйвер: Simon compile + второй QEMU |
| `scripts/extract_files_pl.py` | 1 | Достать `pnut_tcc.c` из `build/files.pl` |
| `scripts/raw_to_elf.py` | 4 | Обернуть seed tcc-pnut в ET_EXEC @ `0x82000000` |
| `user/include/*` `libc.c` `crt.s` `init.c` `tcc-config.h` | 4 | POSIX слой и self-host |

---

## Locked memory map (QEMU virt, `-m 1G`)

```
0x10000000          UART 16550
0x80000000          kernel .text (firmware load)
0x80200000          KFS1 ramfs (host pads kernel.bin to 2MiB)
0x81000000          user ELF default (-Wl,-Ttext=0x81000000)
0x82000000          seed /bin/tcc only (исторический link tcc-pnut)
0x82800000          user brk floor if load_end is below this
0xBFFFF000          kernel SP; user SP = 0xBFF00000
```

RAM QEMU: `[0x80000000, 0xC0000000)`.

---

## Phase 1: Boot on UART

**Purpose:** Снять главный риск — TCC 0.9.26 RV собирает M-mode firmware на `0x80000000`, 16550 печатает.

**Entry:** Есть `build/images/stage09.bin` и `build/stage-cache/tcc-pnut.bin` (как для `--tcc-self --from 09`). Если нет — сначала существующий `python3 -u scripts/run_m2_qemu_test.py --from 09` / `--pnut`, не часть этого плана.

**Exit:** Второй QEMU `-bios build/k-firmware.bin` печатает `k 10-k` на UART за 10 с.

**Depends on:** None (кроме кэша stage09).

### Task 1: Honor `-Wl,-Ttext=` / `-Wl,-oformat=` for EXE

**Files:**
- Modify: `simon_rv64_multiboard/stages/08-tcc/tccelf.c` (блок `elf_output_file`, сейчас безусловный force BINARY `0x82000000`)

- [ ] **Step 1: Apply the Ttext guard**

В `elf_output_file` заменить блок с комментарием `Simon loads raw images` (безусловный BINARY + `0x82000000`) на:

```c
    /* Simon default: raw @ USER_CODE_BASE. Keep it only when the user did
     * not pass -Wl,-Ttext= / -Wl,-oformat= (has_text_addr / oformat already set). */
    if (file_type == TCC_OUTPUT_EXE && !s1->has_text_addr) {
        s1->output_format = TCC_OUTPUT_FORMAT_BINARY;
        s1->text_addr = 0x82000000;
        s1->has_text_addr = 1;
    }
```

Смысл: `do tcc-pnut.bin a.c -o a.bin` без Ttext → raw @ `0x82000000`. `tcc … -Wl,-Ttext=0x80000000 -Wl,-oformat=binary` → не затирается (`has_text_addr` уже 1 из `tcc_set_linker`). `tcc … -Wl,-Ttext=0x81000000 -Wl,-oformat=elf64-little` → ELF.

- [ ] **Step 2: Rebuild amalgamation and extract `pnut_tcc.c`**

**Files:**
- Create: `simon_rv64_multiboard/scripts/extract_files_pl.py`

```python
#!/usr/bin/env python3
"""Write one files.pl record to stdout or a file."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stage_images import FILES_PL, parse_pl

def main():
    name = sys.argv[1]
    dest = sys.argv[2]
    recs = parse_pl(open(FILES_PL, "rb").read())
    for n, blob in recs:
        if n == name:
            open(dest, "wb").write(blob)
            print("wrote %s %d bytes" % (dest, len(blob)))
            return
    raise SystemExit("no record %s in %s" % (name, FILES_PL))

if __name__ == "__main__":
    main()
```

Run:

```bash
cd /Users/hikmatillo/WebstormProjects/Aegis-V/simon_rv64_multiboard
bash scripts/pack.sh
python3 scripts/extract_files_pl.py pnut_tcc.c build/pnut_tcc.c
```

Expected: `build/pnut_tcc.c` существует, `grep -n has_text_addr build/pnut_tcc.c` находит новый страж (амальгама содержит `tccelf.c`).

Пересборку `tcc-pnut.bin` в госте делает Task 3 (драйвер уже существует).

- [ ] **Step 3: Commit**

Do not commit.

### Task 2: Freestanding kernel that prints and spins

**Files:**
- Create: `simon_rv64_multiboard/stages/10-k/k.h`
- Create: `simon_rv64_multiboard/stages/10-k/entry.s`
- Create: `simon_rv64_multiboard/stages/10-k/uart.c`
- Create: `simon_rv64_multiboard/stages/10-k/klib.c`
- Create: `simon_rv64_multiboard/stages/10-k/kmain.c`
- Create: `simon_rv64_multiboard/stages/10-k/k.rsp`

- [ ] **Step 1: Write `k.h`**

```c
#ifndef K_H
#define K_H

#define K_BASE      0x80000000UL
#define K_RAMFS     0x80200000UL
#define K_USER      0x81000000UL
#define K_SEED      0x82000000UL
#define K_STACK     0xBFFFF000UL
#define UART0       0x10000000UL
#define UART_LSR    5
#define LSR_TX      0x20

#define AT_FDCWD    -100
#define ENOSYS      38
#define ENOENT      2
#define ENOTTY      25
#define ENOMEM      12
#define EBADF       9
#define EINVAL      22

#define SYS_getcwd  17
#define SYS_fcntl   25
#define SYS_ioctl   29
#define SYS_mkdirat 34
#define SYS_faccessat 48
#define SYS_openat  56
#define SYS_close   57
#define SYS_lseek   62
#define SYS_read    63
#define SYS_write   64
#define SYS_exit    93
#define SYS_uname   160
#define SYS_gettimeofday 169
#define SYS_brk     214
#define SYS_munmap  215
#define SYS_mmap    222
#define SYS_mprotect 226
#define SYS_newfstatat 79
#define SYS_fstat   80
#define SYS_clock_gettime 113

#define MCAUSE_ECALL_U 8
#define MCAUSE_ECALL_M 11

void uart_putc(int c);
void uart_puts(const char *s);
void panic(const char *s);
void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *d, int c, unsigned long n);
unsigned long strlen(const char *s);

#endif
```

Phase 1 `kprintf` может быть алиасом `uart_puts`. Расширить в Phase 3.

- [ ] **Step 2: Write `entry.s` (TCC RV asm)**

```
.global _start
.global kmain
_start:
	lla sp, kstack
	lla t0, _edata
	lla t1, _end
	beq t0, t1, 2
1:
	sb zero, t0, 0
	addi t0, t0, 1
	bltu t0, t1, 1
2:
	jal ra, kmain
3:
	jal zero, 3
.align 4
kstack_space:
	.space 65536
kstack:
```

TCC: ветки `beq t0, t1, 2` — смещение в байтах, чётное. Метки `1`/`2`/`3` как в gas numeric labels: в TCC 0.9.26 numeric local labels **могут не работать**. Использовать именованные:

```
	beq t0, t1, skip_bss
zero_bss:
	sb zero, t0, 0
	addi t0, t0, 1
	bltu t0, t1, zero_bss
skip_bss:
	jal ra, kmain
hang:
	jal zero, hang
```

Символы `_edata` / `_end` добавляет `tcc_add_linker_symbols`. Если линкер их не эмитит для BINARY — в `klib.c` завести `char _edata_anchor, _end_anchor` не надо: тогда обнулять 64KiB после `_start` не требуется, BSS пустой в Phase 1.

Если сборка падает на `_edata`: убрать BSS-цикл, оставить стек `.space 65536` в `.data` (попадёт в raw file).

- [ ] **Step 3: Write `uart.c` / `klib.c` / `kmain.c`**

```c
/* uart.c */
#include "k.h"
void uart_putc(int c)
{
    volatile unsigned char *u = (volatile unsigned char *)UART0;
    while ((u[UART_LSR] & LSR_TX) == 0)
        ;
    u[0] = (unsigned char)c;
}
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}
```

```c
/* klib.c */
#include "k.h"
void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;
    while (n--)
        *dd++ = *ss++;
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    unsigned char *dd = d;
    while (n--)
        *dd++ = (unsigned char)c;
    return d;
}
unsigned long strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n])
        n++;
    return n;
}
void panic(const char *s)
{
    uart_puts("panic: ");
    uart_puts(s);
    uart_puts("\n");
    for (;;)
        ;
}
```

```c
/* kmain.c */
#include "k.h"
void kmain(void)
{
    uart_puts("k 10-k\n");
    for (;;)
        ;
}
```

- [ ] **Step 4: Write `k.rsp`**

```
-nostdlib
-nostdinc
-Wl,-Ttext=0x80000000
-Wl,-oformat=binary
-o kernel.bin
entry.s
uart.c
klib.c
kmain.c
```

TCC `@file` читает аргументы по строкам. Без `-I`: `#include "k.h"` ищет рядом с `.c`.

- [ ] **Step 5: Commit**

Do not commit.

### Task 3: Host driver — compile kernel in Simon, boot second QEMU

**Files:**
- Create: `simon_rv64_multiboard/scripts/run_k_qemu_test.py`

- [ ] **Step 1: Write driver**

Импортировать `Qemu`, `plant_files`, `dump_named`, `CACHE`, `IMAGES`, `ROOT` из существующих скриптов (`run_m2_qemu_test.py`, `guest_ram.py`, `dump_guest_files.py`, `stage_images.py`). Не рефакторить те файлы.

Перед сборкой ядра — пересобрать `tcc-pnut.bin` с amalgamation из Task 1 (иначе Ttext всё ещё затирается старым кэшем):

1. Старт `Qemu(image=IMAGES/stage09.bin, memory="6G", smp="1", qmp_sock=ROOT/build/qemu-k-build-qmp.sock)`.
2. Дождаться `Simon says~: `.
3. `plant_files([("pnut_tcc.c", open("build/pnut_tcc.c","rb").read())], dest_base=0x84000000)`.
4. `q.cmd("do pnut.bin pnut_tcc.c -o tcc-pnut.bin", 600)`.
5. dump `tcc-pnut.bin` → `build/stage-cache/tcc-pnut.bin`.
6. `q.cmd("do tcc-pnut.bin -version", 20, expect="tcc version 0.9.26")`.

Дальше `--boot` на том же госте:

7. Plant все файлы Task 2 + `k.rsp` (имена плоские: `entry.s`, `k.h`, …).
8. `q.cmd("do tcc-pnut.bin @k.rsp", 120)`.
9. Если UART содержит `error:` / `undefined` — FAIL, напечатать хвост UART.
10. `dump_named(..., ["kernel.bin"])` в `build/k-out/`.
11. Прочитать `build/k-out/kernel.bin`. Если size < 256 — FAIL.
12. `firmware = kernel.bin.ljust(0x200000, b'\0')` записать `build/k-firmware.bin`.
13. Закрыть первый QEMU. Открыть второй: `Qemu(image=build/k-firmware.bin, memory="1G", smp="1", qmp_sock=ROOT/build/qemu-k-run-qmp.sock)`.
14. `read_more` до 10 с, искать `k 10-k` в `q.buf`.

CLI:

```python
if __name__ == "__main__":
    if "--boot" in sys.argv:
        run_boot()
    elif "--hi" in sys.argv:
        run_hi()
    elif "--readlen" in sys.argv:
        run_readlen()
    elif "--tcc-self-k" in sys.argv:
        run_tcc_self_k()
    else:
        raise SystemExit("usage: run_k_qemu_test.py --boot|--hi|--readlen|--tcc-self-k")
```

В Phase 1 реализовать только `--boot`; `--hi`/`--readlen`/`--tcc-self-k` — `raise SystemExit("not in this phase")` заглушки, заменить в следующих фазах. Не оставлять пустой `pass` без сообщения.

- [ ] **Step 2: Run `--boot` expecting FAIL before kernel sources exist**

Если Task 2 уже сделан в той же сессии — этот шаг схлопывается: сразу Step 3.

Run: `python3 -u scripts/run_k_qemu_test.py --boot`

Expected until Tasks 1–2 done: FAIL (нет `k.rsp` или старый tcc-pnut игнорирует Ttext и прыгает не туда — PC на `0x80000000` исполняет не kernel).

- [ ] **Step 3: Run `--boot` expecting PASS**

Run: `cd simon_rv64_multiboard && python3 -u scripts/run_k_qemu_test.py --boot`

Expected: stdout содержит `k 10-k`, процесс драйвера exit 0. Если баннера нет: hexdump первых 16 байт `kernel.bin` (ожидаются RV инструкции, не ELF `\x7fELF`); QEMU `-d in_asm` не добавлять в постоянный драйвер.

- [ ] **Step 4: Commit**

Do not commit.

### Task 4: Phase 1 gate

**Files:** none (verification only)

- [ ] **Run exit verification**

Run: `python3 -u scripts/run_k_qemu_test.py --boot`

Expected: UART `k 10-k`, exit 0.

- [ ] **Optional merge checkpoint**

Safe to pause. Userspace ещё нет. `git diff --name-only` ⊆ allowlist Phase 1 files + `scripts/run_k_qemu_test.py` + `extract_files_pl.py` + `tccelf.c`.

---

## Phase 2: U-mode ELF hello

**Purpose:** `ecall` Linux `write`/`exit`, загрузка static ET_EXEC, баннер ядра + `hi`.

**Entry:** Phase 1 gate зелёный; `tcc-pnut.bin` в кэше — с патчем Ttext.

**Exit:** `--hi` → UART содержит `hi` (после `k 10-k`).

**Depends on:** Phase 1.

### Task 5: Trap, syscall write/exit, ELF, one process

**Files:**
- Modify: `simon_rv64_multiboard/stages/10-k/entry.s` (mtvec, mret в U после `kmain` больше не hang-only)
- Modify: `simon_rv64_multiboard/stages/10-k/k.h` (прототипы)
- Modify: `simon_rv64_multiboard/stages/10-k/kmain.c` (`ramfs_init` + `elf_load("/bin/init")` + `enter_user`)
- Modify: `simon_rv64_multiboard/stages/10-k/k.rsp` (добавить `.c` файлы)
- Create: `simon_rv64_multiboard/stages/10-k/trap.c`
- Create: `simon_rv64_multiboard/stages/10-k/syscall.c`
- Create: `simon_rv64_multiboard/stages/10-k/proc.c`
- Create: `simon_rv64_multiboard/stages/10-k/elf.c`
- Create: `simon_rv64_multiboard/stages/10-k/kmem.c`
- Create: `simon_rv64_multiboard/stages/10-k/ramfs.c`

- [ ] **Step 1: Implement trap, syscalls, ELF, KFS parser**

```
/* mret */
.long 0x30200073
/* csrw mtvec, t0  (csrrw x0, mtvec, t0) */
.long 0x30529073
/* csrr t0, mcause */
.long 0x342022F3
/* csrr t0, mepc */
.long 0x341022F3
/* csrw mepc, t0 */
.long 0x34129073
/* csrr t0, mstatus */
.long 0x300022F3
/* csrw mstatus, t0 */
.long 0x30029073
```

mtvec: `lla t0, trap_entry` затем `csrw mtvec, t0`.

`trap_entry`: сохранить x1–x31 на `trap_stack` (ещё `.space 8192`), вызвать `trap_c`. Не сохранять x0.

`trap_c` читает mcause через аргумент: удобнее в asm положить mcause в a0, regs ptr в a1:

```
trap_entry:
	lla t6, trap_save
	sd ra, t6, 8
	/* ... sd x2..x31 ... */
	lla a1, trap_save
	.long 0x342022F3
	mv a0, t0
	jal ra, trap_c
	lla t6, trap_save
	ld ra, t6, 8
	/* restore */
	.long 0x30200073
```

`trap_c(unsigned long mcause, unsigned long *regs)`:

- если `mcause == 8` (U ecall): `a7=regs[17]`, `a0..a5=regs[10..15]`, `regs[10] = syscall(...)`, `mepc += 4`.
- иначе `panic("trap")`.

`syscall.c` Phase 2:

```c
long syscall(long a7, long a0, long a1, long a2, long a3, long a4, long a5)
{
    if (a7 == SYS_write)
        return sys_write(a0, (void *)a1, a2);
    if (a7 == SYS_exit)
        sys_exit(a0);
    if (a7 == SYS_ioctl)
        return -ENOTTY;
    return -ENOSYS;
}
```

`sys_write`: fd 1 или 2 → `uart_putc` по байтам, return count. Иначе `-EBADF`.

`sys_exit`: `uart_puts("exit ")`, печать кода (цифры), `\n`, затем бесконечный цикл (не ребут QEMU).

`kmem.c`: bump `user_brk` от `load_end`. `sys_brk` в Phase 2 возвращает `-ENOSYS`; `hi.s` brk не вызывает.

`elf.c`: разобрать `Elf64_Ehdr` + `Elf64_Phdr` (скопировать поля из `stages/08-tcc/elf.h` **в** `elf.c` как локальные struct, не `#include` весь `elf.h` из 08-tcc). Только `ET_EXEC`, `EM_RISCV=243`, `PT_LOAD`. Копировать `p_filesz` в `p_vaddr`, zero `p_memsz-p_filesz`. Вернуть `e_entry`. Не поддерживать PT_INTERP.

`proc.c`: один `struct proc { int exit_code; }`. `enter_user(entry, sp)`: asm выставляет `mstatus.MPP=00` (обнулить биты 11:12), `mepc=entry`, `sp=user_sp`, `mret`.

User stack (класть в `enter_user` до mret), Linux layout, 16-byte align:

```
argc = 1
argv[0] = pointer to "init"
NULL
NULL   /* envp */
AT_NULL = 0, 0
```

Строка `"init\0"` на стеке выше.

`kmain` Phase 2: баннер `k 10-k\n`, `kfs_init((void *)K_RAMFS)`, `blob = kfs_get("/bin/init", &len)`, `entry = elf_load(blob, len)`, `enter_user(entry, 0xBFF00000UL)`.

`kfs_init` / `kfs_get` в Phase 2 живут в `kmain.c` как минимальный парсер KFS1 (вынести в `ramfs.c` в Phase 3, здесь не дублировать потом — сразу Create `ramfs.c` с парсером, Phase 3 только добавит дерево/open).

KFS1 on-disk:

```
magic[4] = "KFS1"
u32le nent
repeat nent:
  u16le path_len
  path bytes (без NUL)
  u32le data_len
  data bytes
```

`kfs_get`: линейный поиск, memcmp path.

- [ ] **Step 2: Compile kernel with new objects**

`k.rsp` files: `entry.s uart.c klib.c kmain.c trap.c syscall.c proc.c elf.c kmem.c ramfs.c`

- [ ] **Step 3: Commit**

Do not commit.

### Task 6: Userspace `hi`

**Files:**
- Create: `simon_rv64_multiboard/stages/10-k/user/hi.s`
- Create: `simon_rv64_multiboard/stages/10-k/user/hi.rsp`

- [ ] **Step 1: Write `hi.s`**

```
.global _start
_start:
	addi a0, zero, 1
	lla a1, msg
	addi a2, zero, 3
	addi a7, zero, '@'
	ecall
	addi a0, zero, 0
	addi a7, zero, ']'
	ecall
msg:
	.ascii "hi\n"
```

`'@'` = 64 (SYS_write), `']'` = 93 (SYS_exit).

`hi.rsp`:

```
-nostdlib
-nostdinc
-Wl,-Ttext=0x81000000
-Wl,-oformat=elf64-little
-o init.elf
hi.s
```

- [ ] **Step 2: Commit**

Do not commit.

### Task 7: `pack_kfs.py` and `--hi`

**Files:**
- Create: `simon_rv64_multiboard/scripts/pack_kfs.py`
- Modify: `simon_rv64_multiboard/scripts/run_k_qemu_test.py`

- [ ] **Step 1: Write pack_kfs.py**

```python
#!/usr/bin/env python3
"""Pack (path, bytes) into KFS1."""
import struct
import sys

def pack(entries):
    body = b""
    n = 0
    for path, data in entries:
        pb = path.encode("ascii")
        body += struct.pack("<H", len(pb)) + pb
        body += struct.pack("<I", len(data)) + data
        n += 1
    return b"KFS1" + struct.pack("<I", n) + body

def main():
    # args: -o out.kfs path1=file1 path2=file2
    out = sys.argv[2]
    ents = []
    for spec in sys.argv[3:]:
        path, fn = spec.split("=", 1)
        ents.append((path, open(fn, "rb").read()))
    open(out, "wb").write(pack(ents))

if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Extend `--hi`**

После сборки `kernel.bin` (как `--boot`):

1. Plant `hi.s` `hi.rsp`, `do tcc-pnut.bin @hi.rsp`, dump `init.elf`.
2. Head `init.elf` must be `\x7fELF`. Иначе FAIL.
3. `python3 scripts/pack_kfs.py -o build/k-out/ramfs.bin /bin/init=build/k-out/init.elf`
4. `firmware = kernel.ljust(0x200000,b'\0') + ramfs`
5. Второй QEMU, ждать `hi` в UART (и `k 10-k`).

- [ ] **Step 3: Run**

Run: `python3 -u scripts/run_k_qemu_test.py --hi`

Expected: FAIL until Tasks 5–6 done; затем UART `k 10-k` и `hi`, exit 0.

- [ ] **Step 4: Commit**

Do not commit.

### Task 8: Phase 2 gate

**Files:** none

- [ ] **Run:** `python3 -u scripts/run_k_qemu_test.py --hi`

Expected: `hi` на UART, exit 0. Init `exit 0` строка опциональна.

- [ ] **Optional merge checkpoint**

Можно остановиться: ядро запускает ELF.

---

## Phase 3: Path ramfs + read(tcc.c)

**Purpose:** `openat`/`read`/`close` по абсолютным путям; длина `/src/tcc/tcc.c` = хост.

**Entry:** Phase 2 gate зелёный.

**Exit:** `--readlen` печатает `tcc.c <N>` где N = `os.path.getsize(stages/08-tcc/tcc.c)`.

**Depends on:** Phase 2.

### Task 9: ramfs tree + file syscalls

**Files:**
- Modify: `simon_rv64_multiboard/stages/10-k/ramfs.c`
- Modify: `simon_rv64_multiboard/stages/10-k/syscall.c`
- Modify: `simon_rv64_multiboard/stages/10-k/k.h`
- Modify: `simon_rv64_multiboard/stages/10-k/proc.c` (таблица fd, cwd)

- [ ] **Step 1: ramfs API**

```c
void ramfs_init(void *blob);
int ramfs_open(const char *path); /* returns fd >= 3, or -ENOENT */
long ramfs_read(int fd, void *buf, unsigned long n);
long ramfs_lseek(int fd, long off, int whence);
int ramfs_close(int fd);
int ramfs_stat_size(const char *path, unsigned long *sz);
```

Нормализация: если path[0]=='.' && path[1]=='/' — сдвинуть. Абсолютный `/…`. Относительный: prefix `cwd` (`/` в Phase 3). Каталог `sys/` — обычный префикс пути `/include/sys/time.h`, не плоский `sys_time.h`.

fd 0/1/2: stdin=EOF на read, stdout/stderr=UART на write. open начинается с fd 3. Максимум 32 fd.

`lseek` whence 0/1/2 (SEEK_SET/CUR/END).

- [ ] **Step 2: syscall wiring**

Реализовать как Linux, не `return 0`:

| a7 | поведение |
|----|-----------|
| 56 openat | игнорировать dirfd если path[0]=='/' ; иначе cwd |
| 57 close | ramfs_close |
| 62 lseek | ramfs_lseek |
| 63 read | fd 0 → 0; fd 1/2 → -EBADF; else ramfs_read |
| 64 write | как Phase 2 для 1/2; иначе запись в ramfs (Phase 4; здесь `-EBADF` если не 1/2) |
| 17 getcwd | скопировать `"/"` + NUL, return strlen+1 |
| остальные из спеки | пока `-ENOSYS`, кроме ioctl `-ENOTTY` |

- [ ] **Step 3: Commit**

Do not commit.

### Task 10: `readlen` user + pack `tcc.c`

**Files:**
- Create: `simon_rv64_multiboard/stages/10-k/user/sys.s`
- Create: `simon_rv64_multiboard/stages/10-k/user/readlen.c`
- Create: `simon_rv64_multiboard/stages/10-k/user/readlen.rsp`
- Modify: `simon_rv64_multiboard/scripts/run_k_qemu_test.py`

- [ ] **Step 1: `sys.s` + `readlen.c`**

`sys.s`:

```
.global ecall3
ecall3:
	mv a7, a0
	mv a0, a1
	mv a1, a2
	mv a2, a3
	ecall
	jalr zero, ra, 0
```

`readlen.c`: `openat` через `ecall3(56, -100, path, 0)`, цикл `ecall3(63, fd, buf, 4096)` суммируя байты, `putnum` через `ecall3(64, 1, digit, 1)`, строка `tcc.c <N>\n`, `ecall3(93, 0, 0, 0)`.

`readlen.rsp`:

```
-nostdlib
-nostdinc
-Wl,-Ttext=0x81000000
-Wl,-oformat=elf64-little
-o init.elf
sys.s
readlen.c
```

- [ ] **Step 2: `--readlen`**

KFS entries:

- `/bin/init` = dumped `init.elf` (readlen)
- `/src/tcc/tcc.c` = `stages/08-tcc/tcc.c` с хоста (сырой файл)

Run: `python3 -u scripts/run_k_qemu_test.py --readlen`

Expected: UART содержит `tcc.c 12345` где 12345 = `os.path.getsize(...)`. Драйвер сам считает size и `assert marker in uart`.

- [ ] **Step 3: Commit**

Do not commit.

### Task 11: Phase 3 gate

**Files:** none

- [ ] **Run:** `python3 -u scripts/run_k_qemu_test.py --readlen`

Expected: длина совпала, exit 0.

- [ ] **Optional merge checkpoint**

Файловый слой готов; TCC ещё не гоняется.

---

## Phase 4: libc + TCC self-host

**Purpose:** POSIX-слой, seed `/bin/tcc`, сборка из дерева `08-tcc`, фиксация `tcc2==tcc3`.

**Entry:** Phase 3 gate зелёный.

**Exit:** `--tcc-self-k` → UART содержит `tcc version 0.9.26` и `cmp ok` (exit init 0).

**Depends on:** Phase 3.

### Task 12: Headers, crt, libc

**Files:**
- Create: `simon_rv64_multiboard/stages/10-k/user/crt.s`
- Create: `simon_rv64_multiboard/stages/10-k/user/libc.c`
- Create: `simon_rv64_multiboard/stages/10-k/user/tcc-config.h`
- Create headers under `simon_rv64_multiboard/stages/10-k/user/include/`:

`stddef.h` `stdint.h` `stdarg.h` `stdio.h` `stdlib.h` `string.h` `errno.h` `unistd.h` `fcntl.h` `setjmp.h` `time.h` `math.h` `assert.h` `limits.h` `ctype.h` `inttypes.h` `sys/types.h` `sys/stat.h` `sys/time.h`

Не копировать `stages/09-pnut/tcc-inc/` как есть: там нет настоящего `sys/` и `printf` завязан на stubs. Новые заголовки: прототипы под `libc.c`.

- [ ] **Step 1: crt.s**

```
.global _start
.global main
.global exit
_start:
	ld a0, sp, 0
	addi a1, sp, 8
	jal ra, main
	jal ra, exit
```

`exit` в `libc.c` делает ecall 93.

- [ ] **Step 2: libc.c (обязательный минимум спеки)**

Реализовать, не `return 0` для I/O:

- `write`/`read`/`open`/`close`/`lseek`/`brk`/`exit` через `ecall` (`open` → openat AT_FDCWD)
- `malloc`/`free`: bump от `brk` (free — no-op или простой last-fit; tcc живёт)
- `memcpy` `memset` `memmove` `strlen` `strcmp` `strncmp` `strcpy` `strchr` `strstr` `memcpy`
- `strtol` `strtoul` (база 0/10/16)
- `printf` `vsnprintf` `snprintf` `fprintf`: `%s` `%d` `%x` `%c` `%%`; не класть format string как тело
- `FILE` = struct `{int fd;}`; `stdin` fd0 `stdout` fd1 `stderr` fd2
- `fopen`/`fread`/`fwrite`/`fclose`/`fseek`/`ftell`/`fgetc`/`fputc`
- `qsort` (любой корректный, хоть bubble)
- `setjmp`/`longjmp`: если asm слишком тяжёлый — `longjmp` → `exit(1)` и комментарий в `libc.c` что tcc error path умрёт процессом; **не** подменять `fprintf` на `puts(fmt)`

`mmap`: libc может вызывать SYS_mmap. Ядро Phase 4: анонимный bump в `kmem.c` (`MAP_ANONYMOUS=0x20`), иначе `-ENOSYS`. `mprotect`/`munmap` → 0.

Добить syscall таблицу спеки: `uname` (заполнить `sysname="Linux"`, `machine="riscv64"`), `getcwd`, `mkdirat` (создать пустой dir в ramfs), `newfstatat`/`fstat` (заполнить `st_size`, `st_mode`), `fcntl` F_GETFL → 0, `clock_gettime`/`gettimeofday` (монотонный tick++), `faccessat` 0 если ramfs_open успешен.

`ramfs` Phase 4: `open` с `O_CREAT|O_TRUNC` выделяет буфер из kernel bump (1MiB cap на файл) и позволяет `write` в fd. Нужно чтобы tcc создал `/bin/tcc2`.

- [ ] **Step 3: tcc-config.h overlay**

Не менять `stages/08-tcc/config.h` на диске. Overlay в KFS как `/src/tcc/config.h`:

```c
#define BOOTSTRAP 1
#define HAVE_LONG_LONG 1
#define TCC_TARGET_RISCV64 1
#define inline
#define TCC_VERSION "0.9.26"
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCCDIR "/"
#define CONFIG_SYSROOT "/"
#define CONFIG_TCC_CRTPREFIX "/lib"
#define CONFIG_TCC_SYSINCLUDEPATHS "/include"
#define CONFIG_TCC_LIBPATHS "/lib"
#define TCC_LIBGCC "libc.a"
#define TCC_LIBTCC1 "libtcc1.a"
```

Остальные `/src/tcc/*` — **байты с хоста** из `stages/08-tcc/{tcc.c,libtcc.c,...}` список из спеки. Не подкладывать Simon-патчи: в образ идёт дерево как в git, кроме `config.h` overlay.

- [ ] **Step 4: Commit**

Do not commit.

### Task 13: Seed ELF wrap + init self-host

**Files:**
- Create: `simon_rv64_multiboard/scripts/raw_to_elf.py`
- Create: `simon_rv64_multiboard/stages/10-k/user/init.c`
- Create: `simon_rv64_multiboard/stages/10-k/user/cmp.c`
- Modify: `simon_rv64_multiboard/stages/10-k/kmain.c`
- Modify: `simon_rv64_multiboard/stages/10-k/syscall.c` (`sys_exit` возвращает в `kmain`, не hang; hang только если шагов больше нет)
- Modify: `simon_rv64_multiboard/stages/10-k/proc.c`
- Modify: `simon_rv64_multiboard/stages/10-k/trap.c`
- Modify: `simon_rv64_multiboard/scripts/run_k_qemu_test.py`
- Modify: `simon_rv64_multiboard/scripts/pack_kfs.py`

- [ ] **Step 1: raw_to_elf.py**

Обернуть raw RISC-V (линк `0x82000000`, entry = load address) в ELF64 ET_EXEC, PT_LOAD `p_vaddr=p_paddr=0x82000000`, `p_filesz=p_memsz=len(raw)`, `e_entry=0x82000000`. Машинный `EM_RISCV=243`, `ELFCLASS64`, little. Без секций (shnum=0 допустимо).

Проверка на хосте: `python3 scripts/raw_to_elf.py build/stage-cache/tcc-pnut.bin build/k-out/tcc-seed.elf && python3 -c "assert open('build/k-out/tcc-seed.elf','rb').read(4)==b'\\x7fELF'"`.

- [ ] **Step 2: `init.c`, `cmp.c`, sequential `run()` in `kmain`**

Без fork: `sys_exit` возвращает в `kmain` (не hang), пока есть следующий шаг. Hang только после последнего шага.

`init.c` печатает `init\n` и делает `return 0`:

```c
int main(void)
{
    write(1, "init\n", 5);
    return 0;
}
```

Слинковать с `crt.s` и `libc.c` (`-Wl,-Ttext=0x81000000 -Wl,-oformat=elf64-little`).

`cmp.c`: `fopen` двух путей из `argv`, `fread` в `malloc`, сравнить длину и байты, `write(1, "cmp ok\n", 7)` или `write(1, "cmp fail\n", 9)`.

`kmain.c` после `ramfs_init` вызывает `run()` по очереди. Compound literals TCC 0.9.26 может не есть — только `static char *argv_…[]`.

```c
static char *av_init[] = { "/bin/init", 0 };
static char *av_ver[] = { "/bin/tcc", "-version", 0 };
static char *av_tcc2[] = {
    "/bin/tcc", "-o", "/bin/tcc2", "-DONE_SOURCE", "-DTCC_TARGET_RISCV64",
    "-I", "/include", "-nostdlib",
    "-Wl,-Ttext=0x81000000", "-Wl,-oformat=elf64-little",
    "/lib/crt.s", "/lib/libc.c", "/src/tcc/tcc.c", 0
};
static char *av_ver2[] = { "/bin/tcc2", "-version", 0 };
static char *av_tcc3[] = {
    "/bin/tcc2", "-o", "/bin/tcc3", "-DONE_SOURCE", "-DTCC_TARGET_RISCV64",
    "-I", "/include", "-nostdlib",
    "-Wl,-Ttext=0x81000000", "-Wl,-oformat=elf64-little",
    "/lib/crt.s", "/lib/libc.c", "/src/tcc/tcc.c", 0
};
static char *av_cmp[] = { "/bin/cmp", "/bin/tcc2", "/bin/tcc3", 0 };

void kmain(void)
{
    uart_puts("k 10-k\n");
    ramfs_init((void *)K_RAMFS);
    run(av_init[0], av_init);
    run(av_ver[0], av_ver);
    run(av_tcc2[0], av_tcc2);
    run(av_ver2[0], av_ver2);
    run(av_tcc3[0], av_tcc3);
    run(av_cmp[0], av_cmp);
    for (;;)
        ;
}
```

`run(path, argv)`: найти файл в ramfs, `elf_load`, сложить Linux argv на user stack, `enter_user`; по `SYS_exit` вернуться в `run`.

- [ ] **Step 3: Wire `sys_exit` to return from `enter_user`**

В `proc.c` / `trap.c`: `enter_user` сохраняет kernel `ra`/`sp`; `sys_exit` восстанавливает их вместо hang. После последнего `run` в `kmain` — hang.

- [ ] **Step 4: Commit**

Do not commit.

### Task 14: Seed compile sanity (no `08-tcc` patches)

**Files:**
- Modify: `simon_rv64_multiboard/scripts/run_k_qemu_test.py` only (порядок шагов)

- [ ] **Step 1: First `--tcc-self-k` attempt**

KFS минимум:

- `/bin/tcc` = `tcc-seed.elf` (wrapped tcc-pnut; CRT seed уже Linux a7)
- `/lib/crt.s` `/lib/libc.c`
- `/include/**` headers
- `/src/tcc/` files from spec list + overlay `config.h`
- `/bin/cmp` ELF
- `/bin/init` optional banner

Seed tcc-pnut линковался с `tcc_crt.s` (write 64, openat 56). На новом ядре это должно работать, **если** argv layout Linux (ядро кладёт argc/argv). Seed также печатает `tcc-crt\n` — драйвер это игнорирует.

Команда tcc2 как в спеке плюс явные crt/Ttext/oformat:

```
/bin/tcc -o /bin/tcc2 -DONE_SOURCE -DTCC_TARGET_RISCV64 -I /include
  -nostdlib -Wl,-Ttext=0x81000000 -Wl,-oformat=elf64-little
  /lib/crt.s /lib/libc.c /src/tcc/tcc.c
```

- [ ] **Step 2: If UART contains `duplicate case` or `error:`**

Не трогать `stages/08-tcc` исходники компилятора. Чинить **seed**:

1. В amalgamation `pnut_tcc.c` на хосте (после `pack.sh`) **не** добавлять новый `#define float int` — он уже там от pnut pack. Цель — получить tcc, собранный **не pnut**, а уже новым ядром… циклично.

Единственный разрешённый seed-fix без правки tccgen: повторно собрать `tcc-pnut` **из** `pnut_tcc.c`, где в `scripts/pack.sh` python-блоке **уже** есть `#define float int`. Чтобы убрать искажение, executor **один раз** правит **генерацию amalgamation** — это файл `scripts/pack.sh` (python heredoc: удалить `#define float int` и `#define double int`).

Если этот план запретил edit `pack.sh`, seed-fix **разрешён явно здесь**:

**Waiver-on-fail Files add:** Modify `simon_rv64_multiboard/scripts/pack.sh` только строки `#define float int` / `#define double int` (удалить их). Затем Task 1 Steps 3–4 (пересобрать tcc-pnut). Повторить `--tcc-self-k`.

Если pnut **не** компилирует tcc без `float int` — **стоп**, отчёт пользователю, не патчить `tcc.c`/`tccgen.c`.

- [ ] **Step 3: Commit**

Do not commit.

### Task 15: `--tcc-self-k` driver + cmp

**Files:**
- Modify: `simon_rv64_multiboard/scripts/run_k_qemu_test.py`

- [ ] **Step 1: Implement `run_tcc_self_k`**

Таймаут второго QEMU: 900 с (как Simon self-host). Искать в UART:

1. `k 10-k`
2. `tcc version 0.9.26` (достаточно одного от tcc2; seed тоже ок)
3. `cmp ok`

Exit 0 только если все три. `cmp fail` → FAIL.

- [ ] **Step 2: Run**

Run: `python3 -u scripts/run_k_qemu_test.py --tcc-self-k`

Expected: PASS.

- [ ] **Step 3: Commit**

Do not commit.

### Task 16: Phase 4 gate (spec complete)

**Files:** none

- [ ] **Run:** `python3 -u scripts/run_k_qemu_test.py --tcc-self-k`

Expected: `tcc version 0.9.26`, `cmp ok`, exit 0.

- [ ] **Optional merge checkpoint**

Спека закрыта. hex0/gcc не гонять.

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| QEMU virt, M-mode, `0x80000000` | 2–3 |
| tcc-pnut производит kernel, Ttext не ломает Simon default | 1 |
| UART баннер | 3–4 |
| ELF ET_EXEC + write hi | 5–8 |
| KFS дерево путей, open/read tcc.c length | 9–11 |
| Linux a7 таблица | 5, 9, 12 |
| ioctl ENOTTY, unknown ENOSYS | 5, 9 |
| mmap anonymous bump | 12 |
| libc printf/malloc/FILE | 12 |
| crt argc/argv | 12, 5 stack |
| Команда tcc без amalgamation/float int | 13–15 |
| overlay config.h, не Simon tcc_open hacks | 12 |
| tcc2 version + cmp tcc3 | 15–16 |
| exit не ребутит QEMU | 5 |
| Seed fix not 08-tcc algorithm | 14 |
| Нет musl/gcc/hex0/OpenSBI | out of scope |

---

## Notes for executor

- Не коммитить.
- Не запускать `ruff`/`mypy` по всему дереву; Python без новых линтеров.
- Не править `kernel/simon.S`.
- Asm ядра и user stub — только инструкции из `riscv64-asm.c` + `.long` + `.space` + `.ascii` + `.align` + `.global`.
- После каждого таска: `git diff --name-only` ⊆ Allowed paths.
- Проверка типов: C без CI; драйвер — `python3 -m py_compile scripts/run_k_qemu_test.py scripts/pack_kfs.py scripts/raw_to_elf.py scripts/extract_files_pl.py`.
