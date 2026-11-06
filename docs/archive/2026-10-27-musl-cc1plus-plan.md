# musl и cc1plus на Kronos — поэтапный план

> **Исполнитель:** одна фаза целиком, вместе с её гейтом, потом следующая.
> Коммиты только по явной просьбе в том ходе, где просят. Шагов `git commit` в этом плане нет.
> Прогресс отмечать чекбоксами `- [ ]`.

**Цель:** уже собранный гостевой `/bin/cc1` (GCC 4.6.4, форк Ekaitz, коммит `c0d20242b1d6668a0939c9ccf128d404dce9071e`) компилирует musl 1.1.24, и программа, слинкованная с объектами musl, печатает `hello from musl`. Затем этот же `/bin/cc1` компилирует фронтенд C++ из того же дерева GCC, гостевые `/bin/as` и `/bin/ld` линкуют `/bin/cc1plus`, и программа, собранная `cc1plus` и слинкованная с теми же объектами musl, печатает `hello from cc1plus`.

**Архитектура:** хост не запускает gcc, `configure`, `make`, awk, sed, flex, gperf и `genscripts.sh`. `kronos.py` только кладёт байты в ramfs. Короткий заголовок, которого нет в тарболле, пишет человек в файл стадии; при посадке он копируется как есть. Гость уже имеет `/bin/cc1`, `/bin/as`, `/bin/ld`. Новые `.c` musl и фронтенда C++ компилирует `/bin/cc1` в текст GNU as, `/bin/as` собирает объект, `/bin/ld` линкует. `tcc6` эти новые файлы не компилирует. Системные вызовы musl — обычный `ecall` с номером в `a7`. Ядро уже забирает `a7` из `regs[17]` (`src/stages/09-kronos/src/trap.c`). `tp` ядро не ставит, поэтому объекты musl компилируются с `-DSYSCALL_NO_TLS`.

**Стек:** один QEMU virt, 6G, `python3 -u src/scripts/kronos.py --tcc-self-k`. Успех фазы — подстрока UART, которую ждёт `handoff_kronos`. Точка входа программ `0x10000`, как у текущего hello.

**Стратегия фаз:** risk first. Сначала один файл musl, потом живой `write`, потом один файл фронтенда C++, потом целый `cc1plus`.

| Фаза | Имя | Что появляется |
|------|-----|----------------|
| 1 | Один объект musl | `/src/musl/strlen.o` и UART `strlen.o ok` |
| 2 | musl `write` запускается | UART `hello from musl` |
| 3 | `cc1` ест фронтенд C++ | `/src/gcc464/cp-lex.s` и UART `cp-lex.s ok` |
| 4 | `cc1plus` запускает hello | UART `hello from cc1plus` |

**Правило:** гейт фазы k зелёный, прежде чем начинать фазу k+1.

Вход плана: UART `hello from gcc` дважды, второй раз после `hello-2 start`, из `/tmp/gcc464-phase4-uart4.log`. Этот план ту лестницу не пересобирает.

Дальше (полный `libc.a`, `libstdc++`, `iostream`, `ar`, `nm`, shell, `make`, апстримный GCC 9.5, Linux, busybox) — отдельный план. Его вход: UART `hello from cc1plus`.

## Что уже известно

- musl 1.1.24 — та версия, которой Ekaitz в `commencement.scm` кормит первый GCC (`musl-boot0`, затем пересборка тем же GCC как `musl-boot`). URL: `https://musl.libc.org/releases/musl-1.1.24.tar.gz`. sha256: `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3`. В 1.1.24 есть `arch/riscv64`. В заметке релиза прямо назван починенный riscv64.
- Пересборка musl уже этим GCC у Ekaitz идёт с `CC=gcc` и `--disable-shared`. Флаг `-DSYSCALL_NO_TLS` у него стоит на сборке tcc, не на сборке gcc. Здесь он нужен по другой причине: `enter_user` не инициализирует `tp`. Это флаг компиляции объектов, не правка исходника musl.
- Фронтенд C++ GCC 4.6 — это C, не C++. Список объектов в `gcc/cp/Make-lang.in`, переменная `CXX_OBJS`: `cp/cp-lang.o`, `c-family/stub-objc.o`, `cp/call.o`, `cp/decl.o`, `cp/expr.o`, `cp/pt.o`, `cp/typeck2.o`, `cp/class.o`, `cp/decl2.o`, `cp/error.o`, `cp/lex.o`, `cp/parser.o`, `cp/ptree.o`, `cp/rtti.o`, `cp/typeck.o`, `cp/cvt.o`, `cp/except.o`, `cp/friend.o`, `cp/init.o`, `cp/method.o`, `cp/search.o`, `cp/semantics.o`, `cp/tree.o`, `cp/repo.o`, `cp/dump.o`, `cp/optimize.o`, `cp/mangle.o`, `cp/cp-objcp-common.o`, `cp/name-lookup.o`, `cp/cxx-pretty-print.o`, `cp/cp-gimplify.o`, `tree-mudflap.o`, плюс `CXX_C_OBJS` (`attribs.o`, `incpath.o`, `prefix.o` и `C_COMMON_OBJS`). `gcc/cp/cfns.h` в тарболле уже есть. `gperf` не запускать.
- `cc1plus-checksum.c` в тарболле нет, его делает `build/genchecksum`. Хостовый генератор запрещён. В стадию кладётся тот же 16-байтный массив, что уже лежит в `src/stages/12-gcc464/src/cc1-checksum.c`.
- `hello from gcc` линкуется так: `/bin/ld -o /bin/hello-gcc -e 0x10000 -Ttext 0x10000` и объекты `crt464.o`, `hello.o`, `libc464.o`. Для musl объект `libc.c` ядра в строку ld не входит.
- Код выхода `no snap tcc6 in UART` после живых маркеров — известный обрыв раннего `snap` в буфере UART. `src/scripts/lib/qemu.py` не править. Такой код 1 гейт не роняет.

## Открытость

В KFS этой работы только исходники musl 1.1.24, исходники уже закреплённого GCC и короткие заголовки из каталога стадии. Запрещены готовый `cc1plus`, готовая `libc.a`, пакет Guix, бинарь Debian, хостовый `./configure` и `make` musl или GCC.

## Allowed paths

`src/stages/*/artifacts/*` уже в gitignore, кроме `.gitkeep`.

| Путь | Фазы |
|------|------|
| `src/stages/13-musl/README.md` | 1 |
| `src/stages/13-musl/src/bits-alltypes.h` | 1 |
| `src/stages/13-musl/src/bits-syscall.h` | 1, только если в тарболле нет готового `arch/riscv64/bits/syscall.h` |
| `src/stages/13-musl/src/hello-musl.c` | 2 |
| `src/stages/13-musl/src/crt-musl.s` | 2, только если crt musl не дошёл до `main` |
| `src/stages/13-musl/src/hello-cc1plus.cpp` | 4 |
| `src/stages/12-gcc464/src/cc1plus-checksum.c` | 3 |
| `src/scripts/kronos.py` | 1–4 |
| `src/stages/09-kronos/src/kmain.c` | 1–4 |
| `src/stages/09-kronos/src/syscall.c` | 2, только номер 94, и только если trap назвал `a7=94` |
| `docs/bootstrap-to-linux.md` | 4, две строки статуса и одна строка журнала |

Вне этого списка в репозитории ничего не создавать. Исходники musl живут в gitignore-архиве и в ramfs. Если компилятор просит ещё один их заголовок, `kronos.py` дописывает одну строку посадки из архива. Новый файл в `src/stages/13-musl/src/` сверх таблицы — стоп.

Если `/bin/cc1` падает на конструкции языка, фаза останавливается. Следующий заход, не эта фаза, правит один `.c` под `src/stages/07-tcc/src/`, который назвала диагностика, и добавляет один файл `src/stages/10-tcc/tests/musl-<token>.c`. Исходники musl и `gcc/cp/*.c` не патчить.

Грязное дерево, которое уже есть до этого плана (правки tcc, ядра, удаление `src/stages/12-gcc404/`), не откатывать и не считать частью этой работы.

## Вне плана

- Полный набор `.o` всего `src/` musl, `libc.a`, динамический линкер, `src/complex`.
- `libstdc++`, `#include <iostream>`, `operator new`, `ar`, `nm`, `ranlib`, shell, `make`, `find`.
- Апстримный GCC 9.5, Linux, busybox.
- Повторная сборка `/bin/cc1` компилятором tcc и любой хостовый прогон генераторов GCC.
- Правка `libc.c`, `crt.s`, `trap.c`, `proc.c`, `qemu.py`, сида tcc.
- Форматирование `kmain.c` или `kronos.py` целиком. `kmain.c` остаётся CRLF.
- Вызов `gcc_as_ld` и пути `/src/as`, `/src/ld`.

После каждой фазы новые и изменённые этим планом пути входят в Allowed paths этой фазы. Чужие грязные пути не трогать.

---

## Фаза 1: Один объект musl

**Зачем:** проверить, что `/bin/cc1` ест C musl и заголовки riscv64 до того, как собирать `write`.

**Вход:** два `hello from gcc` уже есть в UART.

**Выход:** UART `strlen.o ok`.

**Зависит от:** плана `docs/2026-10-18-gcc464-plan.md`, гейт фазы 4.

### Задача 1. Пин musl 1.1.24

**Files:**
- Create: `src/stages/13-musl/README.md`
- Create: `src/stages/13-musl/artifacts/musl-1.1.24.tar.gz` (gitignore)
- Create: `src/stages/13-musl/artifacts/musl/` (gitignore, распакованный тарболл)

- [ ] **Скачать и сверить хеш**

```bash
mkdir -p src/stages/13-musl/artifacts
curl -fsSL -o src/stages/13-musl/artifacts/musl-1.1.24.tar.gz \
  https://musl.libc.org/releases/musl-1.1.24.tar.gz
shasum -a 256 src/stages/13-musl/artifacts/musl-1.1.24.tar.gz
```

Ожидание: `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3`. Иначе стоп, тарболл не тот.

```bash
mkdir -p src/stages/13-musl/artifacts/musl
tar -xzf src/stages/13-musl/artifacts/musl-1.1.24.tar.gz \
  -C src/stages/13-musl/artifacts/musl --strip-components=1
test -f src/stages/13-musl/artifacts/musl/src/string/strlen.c
test -d src/stages/13-musl/artifacts/musl/arch/riscv64
```

- [ ] **README**

В `src/stages/13-musl/README.md` записать URL, версию `1.1.24` и этот sha256. Других версий musl не писать.

### Задача 2. Заголовок `alltypes`, которого нет в тарболле

**Files:**
- Create: `src/stages/13-musl/src/bits-alltypes.h`

- [ ] **Собрать текст заголовка руками по правилу их awk**

Прочитать из распакованного дерева, не запуская awk:

- `tools/mkalltypes.awk`
- `arch/riscv64/bits/alltypes.h.in`
- `include/alltypes.h.in`

`bits-alltypes.h` — это текст, который этот awk напечатал бы на этих двух `.in`. В файле есть `typedef unsigned long size_t` (на riscv64 `_Addr` — `long`) и typedef-ы, которые `include/string.h` и `include/unistd.h` берут из `bits/alltypes.h`. Файл лежит в репозитории. `kronos.py` его только копирует. Вызова awk, sed или `./configure` в `kronos.py` нет.

Если в тарболле уже есть готовый `arch/riscv64/bits/syscall.h`, `bits-syscall.h` не создавать: сажать файл из тарболла. Если есть только `syscall.h.in`, создать `src/stages/13-musl/src/bits-syscall.h` с `#define __NR_write 64`, `#define __NR_exit 93`, `#define __NR_exit_group 94` и сажать его как `/src/musl/arch/riscv64/bits/syscall.h`. Номера 64 и 93 взять из `src/stages/09-kronos/src/syscall.c` (`SYS_write`, `SYS_exit`). 94 в ядре пока нет; это номер Linux `exit_group`. План при этом не редактировать: путь уже в Allowed paths.

### Задача 3. Посадка и `strlen.o`

**Files:**
- Modify: `src/scripts/kronos.py`
- Modify: `src/stages/09-kronos/src/kmain.c`

- [ ] **Посадка**

Новая функция рядом с посадкой gcc464. Префикс только `/src/musl/`. Кладёт:

- `src/string/strlen.c` → `/src/musl/src/string/strlen.c`
- `include/` и `arch/riscv64/` из тарболла, сохраняя относительные пути под `/src/musl/`
- `src/stages/13-musl/src/bits-alltypes.h` → `/src/musl/arch/riscv64/bits/alltypes.h`
- `src/internal/` и `crt/` ещё не класть: они нужны фазе 2

Хост ничего из этого не компилирует.

- [ ] **Вызов после второго hello**

В `gcc464()`, после запуска `/bin/hello-gcc-2`, если есть `/bin/cc1`, `/bin/as` и `/src/musl/src/string/strlen.c`, вызвать `musl464()`. `kmain.c` не переформатировать.

Аргументы гостя, дословно:

```c
av_cc1strlen[0] = "/bin/cc1";
av_cc1strlen[1] = "-quiet";
av_cc1strlen[2] = "-DSYSCALL_NO_TLS";
av_cc1strlen[3] = "-I";
av_cc1strlen[4] = "/src/musl/include";
av_cc1strlen[5] = "-I";
av_cc1strlen[6] = "/src/musl/arch/riscv64";
av_cc1strlen[7] = "-I";
av_cc1strlen[8] = "/src/musl/src/internal";
av_cc1strlen[9] = "-o";
av_cc1strlen[10] = "/src/musl/strlen.s";
av_cc1strlen[11] = "/src/musl/src/string/strlen.c";

av_asstrlen[0] = "/bin/as";
av_asstrlen[1] = "-o";
av_asstrlen[2] = "/src/musl/strlen.o";
av_asstrlen[3] = "/src/musl/strlen.s";
```

Порядок печати:

```c
uart_puts("musl464 start\n");
run(av_cc1strlen[0], av_cc1strlen);
if (!kfs_get("/src/musl/strlen.s", &len) || len < 9) {
    uart_puts("strlen.s fail\n");
    for (;;)
        ;
}
run(av_asstrlen[0], av_asstrlen);
if (!kfs_get("/src/musl/strlen.o", &len) || len < 9) {
    uart_puts("strlen.o fail\n");
    for (;;)
        ;
}
uart_puts("strlen.o ok\n");
```

Если диагностика `cc1` называет файл из `/include`, добавить в этот argv один `-nostdinc` сразу после `-quiet` и повторить прогон. Другие флаги не добавлять.

- [ ] **Маркеры**

В `handoff_kronos` после второго `"hello from gcc"` дописать, по порядку: `"musl464 start"`, `"strlen.o ok"`.

### Задача 4. Гейт фазы 1

**Files:** нет

- [ ] **Прогон**

```bash
python3 -m py_compile src/scripts/kronos.py
python3 -u src/scripts/kronos.py --tcc-self-k
```

Ожидание: прежние `hello.s ok` и два `hello from gcc`, затем `musl464 start` и `strlen.o ok`. `strlen.s` в дереве стадии нет. Код 1 только с текстом `no snap tcc6 in UART` после `strlen.o ok` — гейт зелёный.

---

## Фаза 2: hello from musl

**Зачем:** объект musl не доказательство. `write` из musl должен вывести строку через тот `ecall`, который ядро уже понимает.

**Вход:** UART `strlen.o ok`.

**Выход:** UART `hello from musl`.

**Зависит от:** фазы 1.

### Задача 5. Программа и объекты `write`

**Files:**
- Create: `src/stages/13-musl/src/hello-musl.c`
- Modify: `src/scripts/kronos.py`
- Modify: `src/stages/09-kronos/src/kmain.c`

- [ ] **Программа**

`hello-musl.c` целиком:

```c
long write(int fd, const void *buf, unsigned long n);

int main(void)
{
    static const char msg[] = "hello from musl\n";
    write(1, msg, sizeof msg - 1);
    return 0;
}
```

Строка `hello from musl` есть только здесь. В объект `write` её не подставлять.

- [ ] **Какие `.c` musl компилировать**

Посадить и скомпилировать тем же argv, что `strlen`, с заменой путей выхода:

- `src/unistd/write.c` → `/src/musl/write.s`, затем `/src/musl/write.o`
- каждый `.c`, который `cc1` попросит следующим сообщением `include file '...' not found` или `undefined`, если этот файл есть в тарболле под `src/` или `crt/`

Первый обязательный набор, до прогона, такой:

- `src/unistd/write.c`
- `src/internal/syscall.c`, если `write.c` ссылается на него
- `crt/crt1.c`

Команда `cc1` для каждого файла копирует флаги задачи 3. Меняются только `-o` и путь входного `.c`. `as` для каждого: `/bin/as -o /src/musl/<имя>.o /src/musl/<имя>.s`.

Исходник musl не переписывать. Если файла в тарболле нет — стоп, имя напечатать.

- [ ] **Линковка**

```c
av_ldmusl[0] = "/bin/ld";
av_ldmusl[1] = "-o";
av_ldmusl[2] = "/bin/hello-musl";
av_ldmusl[3] = "-e";
av_ldmusl[4] = "0x10000";
av_ldmusl[5] = "-Ttext";
av_ldmusl[6] = "0x10000";
av_ldmusl[7] = "/src/musl/crt1.o";
av_ldmusl[8] = "/src/musl/hello-musl.o";
av_ldmusl[9] = "/src/musl/write.o";
```

`/lib/libc.c` и `/src/gcc464/libc464.o` в этот argv не входят. `hello-musl.c` компилирует `/bin/cc1` теми же `-I`, затем `/bin/as`.

```c
uart_puts("hello-musl start\n");
run(av_ldmusl[0], av_ldmusl);
if (!kfs_get("/bin/hello-musl", &len)) {
    uart_puts("hello-musl fail\n");
    for (;;)
        ;
}
run("/bin/hello-musl", av_runmusl);
```

`av_runmusl[0] = "/bin/hello-musl"`.

Если ld пишет `undefined reference` на символ, который определяется `.c` в тарболле musl, добавить этот `.c` тем же способом и символ в argv ld. Один проход — один новый `.c`.

- [ ] **crt, если не дошёл до main**

Если в UART есть trap с `mepc` внутри `crt1` до строки `hello from musl`, один раз завести `src/stages/13-musl/src/crt-musl.s`:

```asm
.global _start
.global main
_start:
    ld a0, sp, 0
    addi a1, sp, 8
    jal ra, main
    li a7, 93
    ecall
```

Посадить его как `/src/musl/crt-musl.s`, собрать `/bin/as -o /src/musl/crt-musl.o /src/musl/crt-musl.s` и подставить этот объект вместо `crt1.o`. `crt/crt1.c` musl не редактировать. `proc.c` не редактировать.

- [ ] **exit_group**

Если trap показывает номер `a7`, равный 94, в `syscall.c` добавить одну ветку: `a7 == 94` вызывает тот же путь, что `SYS_exit`. Другие номера не добавлять, пока trap не напечатал именно их.

### Задача 6. Гейт фазы 2

**Files:**
- Modify: `src/scripts/kronos.py` (маркеры)

- [ ] **Маркеры и прогон**

В `handoff_kronos` после `strlen.o ok`: `"hello-musl start"`, `"hello from musl"`.

```bash
python3 -m py_compile src/scripts/kronos.py
python3 -u src/scripts/kronos.py --tcc-self-k
```

Ожидание: `strlen.o ok`, затем `hello-musl start`, затем строка `hello from musl`. Строка выходит из `/bin/hello-musl`, не из заранее положенного бинаря. Код 1 только на `no snap tcc6` после этой строки — гейт зелёный.

---

## Фаза 3: cc1 компилирует один файл фронтенда C++

**Зачем:** `cp/*.c` тяжелее musl. Один файл показывает, хватает ли уже собранному `cc1` заголовков GCC, до линковки всего `cc1plus`.

**Вход:** UART `hello from musl`.

**Выход:** UART `cp-lex.s ok`.

**Зависит от:** фазы 2.

### Задача 7. `cp/lex.c` → `.s`

**Files:**
- Create: `src/stages/12-gcc464/src/cc1plus-checksum.c`
- Modify: `src/scripts/kronos.py`
- Modify: `src/stages/09-kronos/src/kmain.c`

- [ ] **Контрольная сумма**

`cc1plus-checksum.c` целиком, как соседний `cc1-checksum.c`:

```c
#include "config.h"
#include "system.h"
EXPORTED_CONST unsigned char executable_checksum[16] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };
```

Посадить как `/src/gcc464/cc1plus-checksum.c`. `genchecksum` не запускать.

- [ ] **Компиляция `cp/lex.c`**

Посадить `gcc/cp/lex.c` как `/src/gcc464/cp/lex.c` и `gcc/cp/cfns.h` как `/src/gcc464/cp/cfns.h`, оба из уже проверенного тарболла GCC. Другие `cp/*.c` в этой фазе не сажать.

`/bin/cc1` запускать с теми же `-I`, с какими уже собирался сам `cc1` (`-I /src/gcc464` и подкаталоги, которые есть в его rsp), плюс `-I /src/gcc464/cp`. Выход: `/src/gcc464/cp-lex.s`. Вход: `/src/gcc464/cp/lex.c`.

```c
uart_puts("cp-lex start\n");
run(av_cc1cplex[0], av_cc1cplex);
if (!kfs_get("/src/gcc464/cp-lex.s", &len) || len < 9) {
    uart_puts("cp-lex.s fail\n");
    for (;;)
        ;
}
uart_puts("cp-lex.s ok\n");
```

Файл должен содержать `addi` или `ret`. Иначе `cp-lex.s fail` и висеть. `as` в этой фазе не вызывать.

Если диагностика называет отсутствующий заголовок из дерева GCC, `kronos.py` добавляет одну посадку этого пути из тарболла. Заголовок в `src/stages/12-gcc464/src/` не копировать, пока это не `cc1plus-checksum.c`.

### Задача 8. Гейт фазы 3

**Files:**
- Modify: `src/scripts/kronos.py`

- [ ] **Маркеры и прогон**

После `hello from musl`: `"cp-lex start"`, `"cp-lex.s ok"`.

```bash
python3 -m py_compile src/scripts/kronos.py
python3 -u src/scripts/kronos.py --tcc-self-k
```

Ожидание: `hello from musl`, затем `cp-lex.s ok`. `cp-lex.s` в дереве стадии нет.

---

## Фаза 4: cc1plus и hello from cc1plus

**Зачем:** один `.s` — ещё не компилятор C++. `cc1plus` должен сам собрать программу, а musl `write` — вывести её строку.

**Вход:** UART `cp-lex.s ok`.

**Выход:** UART `hello from cc1plus`.

**Зависит от:** фаз 2 и 3.

### Задача 9. Собрать `/bin/cc1plus` компилятором `/bin/cc1`

**Files:**
- Modify: `src/scripts/kronos.py`
- Modify: `src/stages/09-kronos/src/kmain.c`

- [ ] **Список файлов**

Взять список `.c`, из которого уже линкуется `/bin/cc1`, и добавить `.c`, соответствующие `CXX_OBJS` из раздела «Что уже известно». `tree-mudflap.c` в этот список входит: из линковки `cc1` его не выкидывали молча в этом плане, и здесь тоже не выкидывать. Если какого-то `.c` нет в тарболле — стоп, имя напечатать, файл не подменять пустышкой.

Каждый новый `.c` (которого ещё нет среди объектов `cc1`) гость компилирует так: `/bin/cc1` теми же include, что в задаче 7, в `/src/gcc464/cc1plus-<имя>.s`, затем `/bin/as -o /src/gcc464/cc1plus-<имя>.o`. Объекты, уже входящие в `/bin/cc1`, компилируются заново тем же `/bin/cc1`, а не остаются внутри старого ELF: ld получает `.o`, не готовый `/bin/cc1`.

`cc1plus-checksum.c` компилируется тем же способом. `tcc6` на эти файлы не вызывать.

- [ ] **Линковка**

```c
av_ldcc1plus[0] = "/bin/ld";
av_ldcc1plus[1] = "-o";
av_ldcc1plus[2] = "/bin/cc1plus";
av_ldcc1plus[3] = "-e";
av_ldcc1plus[4] = "0x10000";
av_ldcc1plus[5] = "-Ttext";
av_ldcc1plus[6] = "0x10000";
```

Дальше все `.o` этой задачи, в том же порядке, в каком `cc1` получает свои `.c`, затем объекты `CXX_OBJS`, затем `cc1plus-checksum.o`. Перед запуском `/bin/cc1plus` вызвать тот же `cc1_protect()`, которым уже прикрыт `/bin/cc1`.

```c
uart_puts("cc1plus start\n");
if (!kfs_get("/bin/cc1plus", &len) || len < 9) {
    uart_puts("cc1plus fail\n");
    for (;;)
        ;
}
uart_puts("cc1plus linked\n");
```

Таймаут `handoff_kronos` для прогона, в чьём списке маркеров есть `cc1plus linked`, поставить `21600`. Прогоны, которые падают раньше этого маркера, по-прежнему обрывать на первой строке `fail`, не ждать 21600.

### Задача 10. Программа на C++

**Files:**
- Create: `src/stages/13-musl/src/hello-cc1plus.cpp`
- Modify: `src/stages/09-kronos/src/kmain.c`
- Modify: `src/scripts/kronos.py`

- [ ] **Исходник**

`hello-cc1plus.cpp` целиком:

```cpp
extern "C" long write(int fd, const void *buf, unsigned long n);

int main(void)
{
    static const char msg[] = "hello from cc1plus\n";
    write(1, msg, sizeof msg - 1);
    return 0;
}
```

`#include <iostream>` не добавлять. `libstdc++` не сажать.

- [ ] **Сборка и запуск**

Посадить исходник как `/src/musl/hello-cc1plus.cpp`. Компилирует его `/bin/cc1plus`, не `/bin/cc1` и не `tcc6`:

```c
av_cc1plushello[0] = "/bin/cc1plus";
av_cc1plushello[1] = "-quiet";
av_cc1plushello[2] = "-nostdinc";
av_cc1plushello[3] = "-I";
av_cc1plushello[4] = "/src/musl/include";
av_cc1plushello[5] = "-I";
av_cc1plushello[6] = "/src/musl/arch/riscv64";
av_cc1plushello[7] = "-o";
av_cc1plushello[8] = "/src/musl/hello-cc1plus.s";
av_cc1plushello[9] = "/src/musl/hello-cc1plus.cpp";
```

Дальше `/bin/as -o /src/musl/hello-cc1plus.o /src/musl/hello-cc1plus.s` и `/bin/ld` с теми же `-e 0x10000` и `-Ttext 0x10000`, объекты: тот же crt, что победил в фазе 2, `hello-cc1plus.o`, `write.o` и те объекты musl, без которых фаза 2 не линковалась. `/lib/libc.c` не входит. Запуск `/bin/hello-cc1plus`.

`.s` должен содержать и `hello from cc1plus`, и `addi` или `ret`. Иначе `hello-cc1plus fail` и висеть. Файл `.s` в репозиторий не класть.

### Задача 11. Гейт фазы 4

**Files:**
- Modify: `src/scripts/kronos.py`
- Modify: `docs/bootstrap-to-linux.md`

- [ ] **Маркеры**

После `cp-lex.s ok`, по порядку:

```python
"cc1plus start",
"cc1plus linked",
"hello-cc1plus start",
"hello from cc1plus",
```

- [ ] **Прогон**

```bash
python3 -m py_compile src/scripts/kronos.py
python3 -u src/scripts/kronos.py --tcc-self-k
```

Ожидание: `hello from musl`, затем `cp-lex.s ok`, затем `cc1plus linked`, затем `hello from cc1plus`. Вторая строка — из `/bin/hello-cc1plus`, собранного `/bin/cc1plus`. Первую строку `hello from musl` гейтом этой фазы не считать. Код 1 только на `no snap tcc6` после `hello from cc1plus` — гейт зелёный.

- [ ] **Живой документ**

В `docs/bootstrap-to-linux.md` заменить статус строки «GCC 4.6.4» на `готово`, статус строки «musl, собранный этим gcc, затем пересборка 4.6.4 с C++» на `частично: hello from musl и hello from cc1plus`. В журнал одну строку с датой прогона и этими двумя маркерами. Другие строки файла не менять.

На этом план закрыт.
