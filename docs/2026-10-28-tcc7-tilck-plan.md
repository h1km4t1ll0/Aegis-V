# tcc7 и сборка Tilck на Kronos — поэтапный план

> **Исполнитель:** одна фаза целиком вместе с её гейтом, потом следующая. Ревью по фазам нет, в конце один отчёт о результатах.
> **Коммиты** — шаги плана, но каждый выполняется только после явного «да» пользователя в чате.
> **Всё проверяется на QEMU.** Гейт фазы — строки в UART гостя или файл, выгруженный из гостя, а не сборка на хосте.
> Прогресс отмечать чекбоксами `- [ ]`.

**Цель:** внутри QEMU гость собирает tcc7 (upstream tcc mob) и этим tcc7 компилирует и линкует ядро Tilck в `tilck.elf`.

**Архитектура:** Kronos остаётся единственным ядром в этом плане. Хост (`src/scripts/kronos.py`) только кладёт байты в KFS: файлы закреплённых архивов и файлы стадий. Хост ничего не компилирует, не генерирует и не патчит. Гость в `kmain.c` по шагам запускает tcc6 → tcc7 → tcc7b → tcc7c, тесты tcc7, наши утилиты (`headers-install`, `gen-tilck-config`) и сборку Tilck по `tilck.rsp`. Готовые файлы гость объявляет строкой `snap TAG ADDR SIZE`, хост их забирает.

**Стек:** QEMU `virt`, 6G, один процесс; `python3 -u src/scripts/kronos.py --tcc-self-k`. Дизайн: `docs/2026-10-28-gcc464-ladder-design.md`.

**Стратегия фаз:** risk first. Главное неизвестное — соберёт ли tcc6 upstream tcc mob. Это проверяется во второй фазе, до любой работы с Tilck.

| Фаза | Имя | Что появляется |
|------|-----|----------------|
| 1 | Снимок и чистка | ручная сборка gcc464 и musl удалена, лестница до `hello from tcc6` зелёная |
| 2 | tcc7 из tcc mob | UART `tcc77 ok` (tcc7b ≡ tcc7c) и `hello from tcc7` |
| 3 | Патчи tcc7 под Tilck | UART `kconst ok`, `priv ok`, `macro ok`, `penum ok`, `predefs ok`; `tcc77 ok` по-прежнему |
| 4 | Tilck компилируется и линкуется | `tilck.elf` выгружен из гостя, проверка ELF на хосте зелёная |

**Правило:** гейт фазы k зелёный, прежде чем начинать фазу k+1.

Загрузка Tilck (SBI-прослойка, initrd, `/dev/outbox`, тесты `fork`/`pipe`/`exec`/`dirs`/`mem`, подъём `LINEAR_MAPPING_MB`) — следующий план. Его вход: `tilck.elf` из фазы 4 этого плана.

## Закреплённые исходники

| Что | Пин | Где лежит архив (gitignore) |
|---|---|---|
| tcc mob | `https://repo.or.cz/tinycc.git` `e23dabed154a8495f3ac1ff3cc9d04fa1c207893` | `build/sources/tcc-mob-e23dabed.tar` |
| Tilck | `https://github.com/vvaltchev/tilck.git` `01dacaa51a074474a4e5491fc80addfe98956500` | `build/sources/tilck-01dacaa5.tar` |
| dtc (libfdt) | `https://git.kernel.org/pub/scm/utils/dtc/dtc.git`, последний тег `v1.7.*` на момент задачи 15 | `build/sources/dtc-<тег>.tar` |
| Linux uapi | `https://github.com/torvalds/linux.git` тег `v6.6` (`ffc253263a1375a65fa6c9f62a893e9767fbebfa`) | `build/sources/linux-uapi-v6.6.tar` |

Каждый архив делается через `git archive <коммит>`. Его sha256 записывается в README стадии, а `kronos.py` проверяет sha256 перед посадкой. Посадка — это чтение файлов из tar и запись их байтов в KFS. Ничего не меняется.

## Allowed paths

| Путь | Фазы |
|------|------|
| `src/scripts/kronos.py` | 1–4 |
| `src/scripts/lib/phases.py` | 1, 2, 4 (только словарь `STAGE_DIR`) |
| `src/stages/09-kronos/src/kmain.c` | 1–4 (CRLF сохраняется) |
| `src/stages/12-gcc464/` | 1 (удаление) |
| `src/stages/13-musl/` | 1 (удаление) |
| `docs/2026-10-18-gcc464-plan.md`, `docs/2026-10-27-musl-cc1plus-plan.md` → `docs/archive/` | 1 |
| `src/stages/11-tcc7/README.md` | 2, 3 |
| `src/stages/11-tcc7/src/config.h` | 2 |
| `src/stages/11-tcc7/src/override/*` (наши изменённые копии файлов mob) | 3; 4 — только `tccelf.c`, если линкер tcc не даёт нужный символ (задача 18) |
| `src/stages/11-tcc7/tests/*` | 2, 3; 4 — только тест к правке `tccelf.c` |
| `src/stages/12-tilck/README.md` | 4 |
| `src/stages/12-tilck/src/*` | 4 |
| `src/stages/12-tilck/tests/*` | 4 |
| `docs/2026-10-28-tcc7-tilck-plan.md` (только чекбоксы) | 1–4 |

Остальное в репозитории не трогать. В частности, не трогать `src/stages/07-tcc/`, `src/stages/10-tcc/`, ядро Kronos (кроме `kmain.c`), `lib/qemu.py`, `docs/bootstrap-to-linux.md` и README в корне. Их правка — отдельная работа после этого плана.

После каждой фазы: `git status --short` показывает только пути из Allowed paths этой и прошлых фаз.

## Вне плана

- Загрузка Tilck, SBI, initrd, `/dev/outbox`, подъём `LINEAR_MAPPING_MB` — следующий план.
- kaem, make, bash, musl и дальше по лестнице.
- `run_tcc_self_k` берёт `/bin/tcc` из `tcc6.elf` прошлого прогона, а не строит цепочку от tcc-pnut. Для итогового прогона «с нуля» это надо поправить, но не в этом плане.
- Отладочная строка `tcc-main` в tcc6 (`src/stages/07-tcc/src/tcc.c`). tcc7 — это mob, в нём этой строки нет.
- Переписывание `docs/bootstrap-to-linux.md`, `docs/2026-08-29-compiler-ladder.md` и README.

## Как читать `kmain.c`

Файл в CRLF. Новые шаги идут по образцу `hello6`: массив `static char *av_X[N]`, заполнение в `setup_args()`, вызов `run(av_X[0], av_X)` в `kmain()`, маркеры через `uart_puts`, выгрузка через `snap_file(path, tag)`.

После каждой правки `kmain.c` проверить, что в файле нет голых LF:

```bash
python3 -I -c "import sys; d=open(sys.argv[1],'rb').read(); n=d.replace(b'\r\n',b'').count(b'\n'); print('bare LF:', n); sys.exit(1 if n else 0)" src/stages/09-kronos/src/kmain.c
```

Ожидание: `bare LF: 0`, код 0.

---

## Фаза 1: Снимок и чистка

**Зачем:** записать в историю ручную сборку 4.6.4 и убрать её из рабочего дерева, чтобы следующие фазы не жили рядом с заглушками и хостовыми генераторами.

**Вход:** текущее рабочее дерево ветки `feature/simon-tcc-bootstrap`.

**Выход:** `--tcc-self-k` доходит до `hello from tcc6`; в `kronos.py` и `kmain.c` нет кода gcc и musl.

**Зависит от:** нет.

### Задача 1. Снимок текущего состояния

**Files:** все уже изменённые и новые файлы дерева, кроме `.DS_Store`, `.cursor/`, `.agents/`.

- [ ] **Показать пользователю, что войдёт в коммит**

```bash
git status --short | grep -v -E '\.DS_Store$|^\?\? \.cursor/|^\?\? \.agents/'
```

- [ ] **После «да» пользователя — коммит**

```bash
git add -A -- . ':!**/.DS_Store' ':!.cursor' ':!.agents'
git commit -m "wip(gcc464): snapshot of hand-wired GCC 4.6.4 build before the Tilck ladder

The hand-wired cc1 prints hello from gcc, but uses stub MPFR/MPC and
host-generated options.c and gengtype-lex.c. Kept in history only.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Ожидание: `git status --short` показывает только `.DS_Store`, `.cursor/`, `.agents/`.

### Задача 2. Убрать gcc и musl из `kronos.py`

**Files:**
- Modify: `src/scripts/kronos.py`

- [x] **Удалить функции** `_GCC464_SKIP`, `_gcc464_find`, `_gcc464_put`, `_gcc464_walk`, `_gcc464_rsp`, `_gcc464_cc1_parts`, `_gcc464_make_assign`, `_gcc464_cc1_sources`, `_plant_binutils464`, `collect_kfs_gcc464`, `collect_kfs_musl`, `dump_cc1_trap` и константы, которые используются только ими (найти: `grep -n 'GCC464\|MUSL\|BINUTILS\|gcc464\|musl' src/scripts/kronos.py`).

- [x] **В `run_tcc_self_k`** удалить строки `ents.extend(collect_kfs_gcc464())` и `ents.extend(collect_kfs_musl())`, а из кортежа маркеров `handoff_kronos` — все строки после `"hello from tcc6"`. Таймаут `5400` заменить на `1800`.

- [x] **Проверить**

```bash
grep -n -i -E 'gcc|musl|binutils|cc1' src/scripts/kronos.py
python3 -I -m py_compile src/scripts/kronos.py && echo compiled
```

Ожидание: grep ничего не находит; `compiled`.

### Задача 3. Убрать gcc и musl из `kmain.c`

**Files:**
- Modify: `src/stages/09-kronos/src/kmain.c`

- [x] **В `kmain()`** удалить ветку `if (kfs_get("/src/skip-tcc", &len)) { … }` целиком и всё после блока `hello6` до финального `for (;;) ;`: вызов `gcc464();` и все блоки `/src/gcc/…`.

- [x] **Удалить функции**, которые после этого не вызываются: `gcc464`, `musl464`, `gcc464_step`, `gcc464_go`, `gcc464_cc1_parts`, `self_compile_all`, `gcc_as_ld`, `md_gen`, `fix_branch_skip`, `blob_has` (если больше не используется), а также массивы `av_*`, которые использовались только ими, и их заполнение в `setup_args()`.

Способ: после удаления блоков в `kmain()` по очереди искать каждое имя (`grep -n 'имя' src/stages/09-kronos/src/kmain.c`). Если остались только определение и заполнение, удалить и то и другое.

- [x] **Проверить**

```bash
grep -n -i -E 'gcc|musl|cc1|binutils|/src/as|/src/ld' src/stages/09-kronos/src/kmain.c
python3 -I -c "import sys; d=open(sys.argv[1],'rb').read(); n=d.replace(b'\r\n',b'').count(b'\n'); print('bare LF:', n); sys.exit(1 if n else 0)" src/stages/09-kronos/src/kmain.c
```

Ожидание: grep ничего не находит; `bare LF: 0`.

### Задача 4. Удалить стадии и убрать старые планы в архив

**Files:**
- Delete: `src/stages/12-gcc464/`, `src/stages/13-musl/`
- Move: `docs/2026-10-18-gcc464-plan.md`, `docs/2026-10-27-musl-cc1plus-plan.md` → `docs/archive/`
- Modify: `src/scripts/lib/phases.py` (строка `"12": "12-gcc464",` удаляется)

- [x] **Сохранить закреплённые архивы** (они в gitignore и понадобятся следующим планам)

```bash
mkdir -p build/sources
mv src/stages/12-gcc464/artifacts/gcc-4.6.4-c0d20242.tar build/sources/
mv src/stages/12-gcc464/artifacts/binutils-2.30.tar.bz2 build/sources/
mv src/stages/13-musl/artifacts/musl-1.1.24.tar.gz build/sources/
shasum -a 256 build/sources/*
```

Ожидание: `fb8689273a59812638f7d38a6187ebd32fd0487c0a90100d12a86e6251bbbac0` (gcc), `efeade848067e9a03f1918b1da0d37aaffa0b0127a06b5e9236229851d9d0c09` (binutils), `1370c9a812b2cf2a7d92802510cca0058cc37e66a7bedd70051f0a34015022a3` (musl).

- [x] **Удалить и перенести**

```bash
rm -rf src/stages/12-gcc464 src/stages/13-musl
mkdir -p docs/archive
git mv docs/2026-10-18-gcc464-plan.md docs/archive/ 2>/dev/null || mv docs/2026-10-18-gcc464-plan.md docs/archive/
git mv docs/2026-10-27-musl-cc1plus-plan.md docs/archive/ 2>/dev/null || mv docs/2026-10-27-musl-cc1plus-plan.md docs/archive/
```

- [x] **Убрать `"12": "12-gcc464",` из `STAGE_DIR`** в `src/scripts/lib/phases.py`.

### Задача 5. Гейт фазы 1

**Files:** нет (только проверка).

- [x] **Прогон на QEMU**

```bash
python3 -u src/scripts/kronos.py --tcc-self-k 2>&1 | tee /tmp/p1-gate.log
```

Ожидание: в логе по порядку `Kronos`, `cmp ok`, `tcc56 ok`, `hello6 built`, `hello from tcc6`. Код выхода 0, или 1 только с последней строкой `no snap tcc6 in UART` (известный обрыв буфера UART; гейт он не роняет).

- [x] **Дерево**

```bash
git status --short | grep -v -E '\.DS_Store$|^\?\? \.cursor/|^\?\? \.agents/'
```

Ожидание: только `kronos.py`, `phases.py`, `kmain.c`, удаления в `12-gcc464/` и `13-musl/`, перенос двух планов, этот план.

- [ ] **Коммит после «да» пользователя:** `refactor: drop the hand-wired gcc464 and musl rungs`.

---

## Фаза 2: tcc7 из tcc mob

**Зачем:** проверить главное неизвестное — что tcc6 собирает upstream tcc mob и что результат сходится сам с собой.

**Вход:** гейт фазы 1 зелёный.

**Выход:** UART `tcc77 ok` (tcc7b ≡ tcc7c) и `hello from tcc7`; `tcc7b.elf` выгружен в `src/stages/11-tcc7/artifacts/`.

**Зависит от:** фаза 1.

### Задача 6. Пин tcc mob

**Files:**
- Create: `src/stages/11-tcc7/README.md`
- Create: `build/sources/tcc-mob-e23dabed.tar` (gitignore)

- [x] **Архив**

```bash
mkdir -p /tmp/pin-tcc && git clone -q --bare https://repo.or.cz/tinycc.git /tmp/pin-tcc/tinycc.git
git --git-dir=/tmp/pin-tcc/tinycc.git rev-parse e23dabed154a8495f3ac1ff3cc9d04fa1c207893
git --git-dir=/tmp/pin-tcc/tinycc.git archive --format=tar e23dabed154a8495f3ac1ff3cc9d04fa1c207893 -o build/sources/tcc-mob-e23dabed.tar
shasum -a 256 build/sources/tcc-mob-e23dabed.tar
```

Ожидание: `rev-parse` печатает тот же хеш; sha256 записать в README.

- [x] **README**

```markdown
# 11 — tcc7 (upstream tcc mob)

Kronos `/bin/tcc6` compiles upstream TinyCC mob. The result `/bin/tcc7` compiles itself twice; `/bin/tcc7b` ≡ `/bin/tcc7c`.

**Source:** https://repo.or.cz/tinycc.git `e23dabed154a8495f3ac1ff3cc9d04fa1c207893` (2026-10-16). LGPL-2.1.
**Archive:** `build/sources/tcc-mob-e23dabed.tar` (gitignored), sha256 `<из шага выше>`.
**Config:** `src/config.h`. `CONFIG_TCC_PREDEFS=0`: `tccdefs.h` is read at run time from `/lib/tcc7/include`, so the host `c2str` generator is not used.
**Our changes:** whole-file copies in `src/override/`, listed below with the reason. None yet.

Success UART: `tcc77 ok`, `hello from tcc7`.
```

### Задача 7. `config.h` для tcc7

**Files:**
- Create: `src/stages/11-tcc7/src/config.h`

- [x] **Файл**

```c
#define TCC_VERSION "0.9.28rc"
#define TCC_TARGET_RISCV64 1
#define CONFIG_TCC_PREDEFS 0
#define CONFIG_TCC_STATIC 1
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCCDIR "/lib/tcc7"
#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include:/include"
#define CONFIG_TCC_LIBPATHS "/lib"
#define CONFIG_TCC_CRTPREFIX "/lib"
#define CONFIG_TCC_ELFINTERP "/lib/ld-musl-riscv64.so.1"
#define TCC_LIBTCC1 "libtcc1.a"
```

### Задача 8. Посадка tcc mob в KFS

**Files:**
- Modify: `src/scripts/kronos.py`

- [x] **Константы и функция** (рядом с `collect_kfs_tcc_full`)

```python
SOURCES = os.path.join(ROOT, "build", "sources")
TCC7_DIR = os.path.join(ROOT, "src", "stages", "11-tcc7")
TCC7_TAR = os.path.join(SOURCES, "tcc-mob-e23dabed.tar")
TCC7_SHA256 = "<sha256 из задачи 6>"
TCC7_OUT = artifacts("11-tcc7")


def _tar_files(path, sha256):
    """Bytes of every regular file in a pinned tar, keyed by member path."""
    data = open(path, "rb").read()
    got = hashlib.sha256(data).hexdigest()
    if got != sha256:
        raise SystemExit("%s sha256 %s, want %s" % (path, got, sha256))
    out = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as tf:
        for m in tf.getmembers():
            if m.isfile():
                out[m.name] = tf.extractfile(m).read()
    return out


def collect_kfs_tcc7():
    """Plant tcc mob: sources under /src/tcc7, headers under /lib/tcc7/include."""
    files = _tar_files(TCC7_TAR, TCC7_SHA256)
    over = os.path.join(TCC7_DIR, "src", "override")
    if os.path.isdir(over):
        for root, _dirs, names in os.walk(over):
            for fn in names:
                full = os.path.join(root, fn)
                files[os.path.relpath(full, over).replace("\\", "/")] = open(full, "rb").read()
    ents = []
    for name in sorted(files):
        if "/" not in name and name.endswith((".c", ".h", ".def")):
            ents.append(("/src/tcc7/" + name, files[name]))
        elif name.startswith("lib/"):
            ents.append(("/src/tcc7/" + name, files[name]))
        elif name.startswith("include/"):
            ents.append(("/lib/tcc7/" + name, files[name]))
    ents.append(("/src/tcc7/config.h", open(os.path.join(TCC7_DIR, "src", "config.h"), "rb").read()))
    ents.append(("/src/hello7/hello.c", open(os.path.join(TCC7_DIR, "tests", "hello7.c"), "rb").read()))
    print("[tcc7] KFS %d files" % len(ents), flush=True)
    return ents
```

В импорты добавить `io` и `tarfile`, если их там нет.

- [x] **Тест-программа** `src/stages/11-tcc7/tests/hello7.c` (Files: Create)

```c
#include <stdio.h>

int main(void)
{
    puts("hello from tcc7");
    return 0;
}
```

- [x] **В `run_tcc_self_k`** после строки с `/src/hello/hello.c` добавить `ents.extend(collect_kfs_tcc7())`. В кортеж маркеров после `"hello from tcc6"` добавить `"tcc7 built", "tcc7b built", "tcc7c built", "tcc77 ok", "hello from tcc7"`. После `dump_snap(... "hello6" ...)` добавить:

```python
        dump_snap(q, q.buf, "tcc7b", os.path.join(TCC7_OUT, "tcc7b.elf"))
```

- [x] **`phases.py`:** в `STAGE_DIR` добавить `"11": "11-tcc7",`.

### Задача 9. Шаги tcc7 в `kmain.c`

**Files:**
- Modify: `src/stages/09-kronos/src/kmain.c`

Правило линковки: tcc6 линкует как сейчас (`/lib/crt.s /lib/libc.c`). Код, который собирает tcc7, дополнительно линкуется с рантаймом mob для riscv64, `/src/tcc7/lib/lib-arm64.c` (так в mob устроен `RISCV64_O`, в нём операции с `long double`).

- [x] **Массивы** (рядом с `av_hello6`)

```c
static char *av_tcc7[16];
static char *av_tcc7b[17];
static char *av_tcc7c[17];
static char *av_cmp77[4];
static char *av_hello7[13];
static char *av_runhello7[2];
```

- [x] **Заполнение в `setup_args()`** (после `av_runhello`)

```c
    av_tcc7[0] = "/bin/tcc6";
    av_tcc7[1] = "-o";
    av_tcc7[2] = "/bin/tcc7";
    av_tcc7[3] = "-DONE_SOURCE=1";
    av_tcc7[4] = "-I";
    av_tcc7[5] = "/src/tcc7";
    av_tcc7[6] = "-I";
    av_tcc7[7] = "/include";
    av_tcc7[8] = "-nostdlib";
    av_tcc7[9] = "-Wl,-Ttext=0x10000";
    av_tcc7[10] = "-Wl,-oformat=elf64-little";
    av_tcc7[11] = "/lib/crt.s";
    av_tcc7[12] = "/lib/libc.c";
    av_tcc7[13] = "/src/tcc7/tcc.c";
    av_tcc7[14] = 0;
    av_tcc7b[0] = "/bin/tcc7";
    av_tcc7b[1] = "-o";
    av_tcc7b[2] = "/bin/tcc7b";
    av_tcc7b[3] = "-DONE_SOURCE=1";
    av_tcc7b[4] = "-I";
    av_tcc7b[5] = "/src/tcc7";
    av_tcc7b[6] = "-I";
    av_tcc7b[7] = "/include";
    av_tcc7b[8] = "-nostdlib";
    av_tcc7b[9] = "-Wl,-Ttext=0x10000";
    av_tcc7b[10] = "-Wl,-oformat=elf64-little";
    av_tcc7b[11] = "/lib/crt.s";
    av_tcc7b[12] = "/lib/libc.c";
    av_tcc7b[13] = "/src/tcc7/lib/lib-arm64.c";
    av_tcc7b[14] = "/src/tcc7/tcc.c";
    av_tcc7b[15] = 0;
    av_tcc7c[0] = "/bin/tcc7b";
    av_tcc7c[1] = "-o";
    av_tcc7c[2] = "/bin/tcc7c";
    av_tcc7c[3] = "-DONE_SOURCE=1";
    av_tcc7c[4] = "-I";
    av_tcc7c[5] = "/src/tcc7";
    av_tcc7c[6] = "-I";
    av_tcc7c[7] = "/include";
    av_tcc7c[8] = "-nostdlib";
    av_tcc7c[9] = "-Wl,-Ttext=0x10000";
    av_tcc7c[10] = "-Wl,-oformat=elf64-little";
    av_tcc7c[11] = "/lib/crt.s";
    av_tcc7c[12] = "/lib/libc.c";
    av_tcc7c[13] = "/src/tcc7/lib/lib-arm64.c";
    av_tcc7c[14] = "/src/tcc7/tcc.c";
    av_tcc7c[15] = 0;
    av_cmp77[0] = "/bin/cmp";
    av_cmp77[1] = "/bin/tcc7b";
    av_cmp77[2] = "/bin/tcc7c";
    av_cmp77[3] = 0;
    av_hello7[0] = "/bin/tcc7b";
    av_hello7[1] = "-o";
    av_hello7[2] = "/bin/hello7";
    av_hello7[3] = "-I";
    av_hello7[4] = "/include";
    av_hello7[5] = "-nostdlib";
    av_hello7[6] = "-Wl,-Ttext=0x10000";
    av_hello7[7] = "-Wl,-oformat=elf64-little";
    av_hello7[8] = "/lib/crt.s";
    av_hello7[9] = "/lib/libc.c";
    av_hello7[10] = "/src/tcc7/lib/lib-arm64.c";
    av_hello7[11] = "/src/hello7/hello.c";
    av_hello7[12] = 0;
    av_runhello7[0] = "/bin/hello7";
    av_runhello7[1] = 0;
```

- [x] **Шаг в `kmain()`** сразу после блока `hello6` (внутри `if (kfs_get("/src/tcc-full/tcc.c", &len))`)

```c
        if (kfs_get("/src/tcc7/tcc.c", &len)) {
            run(av_tcc7[0], av_tcc7);
            if (!kfs_get("/bin/tcc7", &len)) {
                uart_puts("tcc7 fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7 built\n");
            run(av_tcc7b[0], av_tcc7b);
            if (!kfs_get("/bin/tcc7b", &len)) {
                uart_puts("tcc7b fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7b built\n");
            run(av_tcc7c[0], av_tcc7c);
            if (!kfs_get("/bin/tcc7c", &len)) {
                uart_puts("tcc7c fail\n");
                for (;;)
                    ;
            }
            uart_puts("tcc7c built\n");
            run(av_cmp77[0], av_cmp77);
            if (proc_last_exit() == 0)
                uart_puts("tcc77 ok\n");
            snap_file("/bin/tcc7b", "tcc7b");
            run(av_hello7[0], av_hello7);
            run(av_runhello7[0], av_runhello7);
        }
```

- [x] **Проверить CRLF** (команда из раздела «Как читать `kmain.c`»).

### Задача 10. Гейт фазы 2

**Files:** нет.

- [x] **Прогон на QEMU**

```bash
python3 -u src/scripts/kronos.py --tcc-self-k 2>&1 | tee /tmp/p2-gate.log
grep -E 'hello from tcc6|tcc7 built|tcc7b built|tcc7c built|tcc77 ok|hello from tcc7' /tmp/p2-gate.log
ls -la src/stages/11-tcc7/artifacts/tcc7b.elf
```

Ожидание: все шесть строк по порядку; `tcc7b.elf` существует.

- [x] **Если tcc6 падает на исходнике mob** (`tcc7 fail` или ошибка компилятора в UART): фаза останавливается. Ошибка — это пробел tcc6. Отдельный заход, не эта фаза, исправляет один файл в `src/stages/07-tcc/src/`, который назвала диагностика, и добавляет воспроизводящий тест `src/stages/10-tcc/tests/mob-<token>.c`. Исходники mob ради tcc6 не патчить.

- [ ] **Если tcc7b не линкуется из-за неопределённого символа** с префиксом `__` (рантайм mob): добавить в строки `av_tcc7b`, `av_tcc7c`, `av_hello7` файл из `/src/tcc7/lib/`, который определяет этот символ (`grep -l 'символ' lib/*.c lib/*.S` в архиве mob), и повторить гейт.

- [ ] **Коммит после «да» пользователя:** `feat(tcc7): guest tcc6 builds upstream tcc mob, fixed point tcc7b ≡ tcc7c`.

---

## Фаза 3: Патчи tcc7 под Tilck

**Зачем:** закрыть пробелы mob, которые нашла проверка компиляции Tilck. Каждый пробел — тест, который сначала падает, потом проходит на госте.

**Вход:** гейт фазы 2 зелёный.

**Выход:** UART `kconst ok`, `priv ok`, `macro ok`, `penum ok`, `predefs ok`, и `tcc77 ok` с изменёнными исходниками.

**Зависит от:** фаза 2.

Изменённые файлы mob кладутся целиком в `src/stages/11-tcc7/src/override/` с тем же относительным путём (`riscv64-asm.c`, `riscv64-tok.h`, `tccasm.c`, `tcctok.h`, `tccgen.c`, `include/tccdefs.h`). `collect_kfs_tcc7` уже подставляет их вместо файлов архива (задача 8). Причина каждого изменения дописывается в README стадии.

### Задача 11. Тесты пробелов

**Files:**
- Create: `src/stages/11-tcc7/tests/kconst.c`, `priv.c`, `macro.S`, `macro-main.c`, `penum.c`, `predefs.c`
- Modify: `src/scripts/kronos.py` (посадка тестов в `/src/t7/`), `src/stages/09-kronos/src/kmain.c` (сборка и запуск)

- [x] **`kconst.c`** — ограничение `K` (5-битная беззнаковая константа)

```c
#include <stdio.h>

int main(void)
{
    unsigned long v;
    __asm__ volatile ("addi %0, zero, %1" : "=r" (v) : "K" (17));
    if (v != 17)
        return 1;
    puts("kconst ok");
    return 0;
}
```

- [x] **`priv.c`** — привилегированные инструкции должны ассемблироваться; проверяем кодировку, а не исполнение

```c
#include <stdio.h>

extern unsigned int priv_words[4];
__asm__ (".global priv_words\n"
         "priv_words:\n"
         "sret\n"
         "mret\n"
         "sfence.vma\n"
         "sfence.vma a0, zero\n");

int main(void)
{
    if (priv_words[0] != 0x10200073u)
        return 1;
    if (priv_words[1] != 0x30200073u)
        return 2;
    if (priv_words[2] != 0x12000073u)
        return 3;
    if (priv_words[3] != 0x12050073u)
        return 4;
    puts("priv ok");
    return 0;
}
```

- [x] **`macro.S`** и **`macro-main.c`** — `.macro` без параметров, как в `asm_defs.h` Tilck

```asm
.macro load_42
   li a0, 40
   addi a0, a0, 2
.endm

.global macro_val
macro_val:
   load_42
   ret
```

```c
#include <stdio.h>

long macro_val(void);

int main(void)
{
    if (macro_val() != 42)
        return 1;
    puts("macro ok");
    return 0;
}
```

- [x] **`penum.c`** — `packed` у `enum`

```c
#include <stdio.h>

enum small { s_a = 0, s_b = 5 } __attribute__((packed));
enum wide { w_a = 0, w_b = 70000 } __attribute__((packed));

int main(void)
{
    if (sizeof(enum small) != 1)
        return 1;
    if (sizeof(enum wide) != 4)
        return 2;
    puts("penum ok");
    return 0;
}
```

- [x] **`predefs.c`** — макросы, которые GCC определяет сам

```c
#include <stdio.h>

#if !defined(__riscv_xlen) || __riscv_xlen != 64
#error __riscv_xlen
#endif
#if !defined(__ATOMIC_RELAXED) || !defined(__ATOMIC_SEQ_CST)
#error __ATOMIC_*
#endif
#if __ATOMIC_RELAXED != 0 || __ATOMIC_CONSUME != 1 || __ATOMIC_ACQUIRE != 2 \
    || __ATOMIC_RELEASE != 3 || __ATOMIC_ACQ_REL != 4 || __ATOMIC_SEQ_CST != 5
#error __ATOMIC_ values
#endif
#if __GCC_ATOMIC_INT_LOCK_FREE != 2 || __GCC_ATOMIC_LONG_LOCK_FREE != 2 \
    || __GCC_ATOMIC_POINTER_LOCK_FREE != 2 || __GCC_ATOMIC_BOOL_LOCK_FREE != 2 \
    || __GCC_ATOMIC_CHAR_LOCK_FREE != 2 || __GCC_ATOMIC_SHORT_LOCK_FREE != 2
#error __GCC_ATOMIC_*_LOCK_FREE
#endif

int main(void)
{
    puts("predefs ok");
    return 0;
}
```

- [x] **Посадка:** в `collect_kfs_tcc7` добавить все файлы `src/stages/11-tcc7/tests/` под `/src/t7/` (кроме `hello7.c`, он уже в `/src/hello7/`).

- [x] **Сборка и запуск в `kmain.c`:** после `run(av_runhello7…)` для каждого теста `T` из `kconst`, `priv`, `macro`, `penum`, `predefs` — сборка `/bin/tcc7b -o /bin/t7-T -I /include -nostdlib -Wl,-Ttext=0x10000 -Wl,-oformat=elf64-little /lib/crt.s /lib/libc.c /src/tcc7/lib/lib-arm64.c /src/t7/T.c` (для `macro` — ещё `/src/t7/macro.S` перед `macro-main.c`, вместо `T.c`) и запуск `/bin/t7-T`. Массивы `av_t7cc_T[14]` и `av_t7run_T[2]` по образцу `av_hello7` и `av_runhello7`. Чтобы сбой одного теста не останавливал остальные, после каждой сборки проверять `kfs_get("/bin/t7-T", &len)`; если файла нет, печатать `T build fail`.

- [x] **Убедиться, что тесты падают:** прогон `--tcc-self-k` до патчей. Ожидание: ни одной строки `T ok`; в UART ошибки tcc7b (`unknown constraint 'K'`, `known instruction expected` для `sret`, `.macro`, `#error`) или выход теста с ненулевым кодом.

### Задача 12. Патчи

**Files:**
- Create: `src/stages/11-tcc7/src/override/include/tccdefs.h`, `override/riscv64-asm.c`, `override/riscv64-tok.h`, `override/tccasm.c`, `override/tcctok.h`, `override/tccgen.c` (копии файлов архива mob с изменениями ниже)
- Modify: `src/stages/11-tcc7/README.md`

Каждую копию начинать с файла из архива (`tar -xOf build/sources/tcc-mob-e23dabed.tar <путь> > src/stages/11-tcc7/src/override/<путь>`) и менять только то, что описано.

- [x] **`include/tccdefs.h` — предопределения.** В ветку `#elif defined __riscv` (около строки 239) добавить:

```c
    #ifndef __riscv_xlen
    #define __riscv_xlen 64
    #endif
```

А в общую часть файла, вне веток архитектур:

```c
#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED 0
#define __ATOMIC_CONSUME 1
#define __ATOMIC_ACQUIRE 2
#define __ATOMIC_RELEASE 3
#define __ATOMIC_ACQ_REL 4
#define __ATOMIC_SEQ_CST 5
#endif
#define __GCC_ATOMIC_BOOL_LOCK_FREE 2
#define __GCC_ATOMIC_CHAR_LOCK_FREE 2
#define __GCC_ATOMIC_SHORT_LOCK_FREE 2
#define __GCC_ATOMIC_INT_LOCK_FREE 2
#define __GCC_ATOMIC_LONG_LOCK_FREE 2
#define __GCC_ATOMIC_LLONG_LOCK_FREE 2
#define __GCC_ATOMIC_POINTER_LOCK_FREE 2
```

Перед этим проверить `grep -n '__ATOMIC_RELAXED' include/stdatomic.h include/tccdefs.h` в архиве. Если `stdatomic.h` определяет их без `#ifndef`, значения совпадают с нашими, и предупреждения о переопределении не будет.

- [x] **`riscv64-asm.c` — ограничение `K`.** В `asm_compute_constraints` (около строки 2353) рядом с `case 'I':` добавить `case 'K':` — непосредственная константа 0…31, обрабатывается как `'I'`, но с проверкой диапазона `0 <= c <= 31`. В `subst_asm_operand` / `asm_gen_code`, где `'I'` подставляет константу (около строки 2544), принять `'K'` так же.

- [x] **`riscv64-tok.h` и `riscv64-asm.c` — `sret`, `mret`, `sfence.vma`.** Добавить токены и кодирование:
  - `sret` → `0x10200073`, `mret` → `0x30200073` (без операндов, как существующий `ecall`);
  - `sfence.vma` без операндов → `0x12000073`; `sfence.vma rs1, rs2` → `0x12000073 | (rs2 << 20) | (rs1 << 15)`; `sfence.vma rs1` — то же с `rs2 = zero`.
  Для имени с точкой — по образцу `DEF_ASM_WITH_SUFFIX(fence, i)`.

- [x] **`tcctok.h` и `tccasm.c` — `.macro` / `.endm` без параметров.** Добавить `DEF_ASMDIR(macro)` и `DEF_ASMDIR(endm)` внутрь диапазона `TOK_ASMDIR_FIRST`…`TOK_ASMDIR_LAST` (перед `section`, он должен остаться последним). В `asm_parse_directive`:
  - на `.macro NAME` запомнить имя и записать все токены до `.endm` в `TokenString` (как это делает `.rept`/`.endr` в том же файле);
  - сохранить макрос в таблице `{имя → TokenString}` текущего `TCCState`;
  - в разборе инструкции (`asm_parse_line` или где ищется опкод) при совпадении имени с макросом проиграть его токены (`begin_macro(ts, 0)`), как проигрывается `.rept`.
  Параметры (`\arg`) не поддерживаются: если после имени в `.macro` есть что-то кроме перевода строки, выдать `tcc_error("macro parameters not supported")`.

- [x] **`tccgen.c` — `packed` у `enum`.** В `struct_decl` для `u == VT_ENUM` после разбора перечислителей: если у объявления есть `packed` (из `ad->a.packed` или атрибута после `}`), выбрать наименьший тип, вмещающий диапазон значений: `char` (1 байт), `short` (2), `int` (4), иначе `long long`. Знаковость — как сейчас. Тип записать туда же, куда tcc пишет выбранный базовый тип перечисления.

- [x] **README:** под «Our changes» по строке на каждый изменённый файл: что изменено и какой тест это проверяет.

### Задача 13. Гейт фазы 3

**Files:** нет.

- [x] **Прогон на QEMU**

```bash
python3 -u src/scripts/kronos.py --tcc-self-k 2>&1 | tee /tmp/p3-gate.log
grep -E 'tcc77 ok|hello from tcc7|kconst ok|priv ok|macro ok|penum ok|predefs ok' /tmp/p3-gate.log
```

Ожидание: все семь строк. `tcc77 ok` здесь означает, что патченый tcc7 по-прежнему сходится сам с собой.

- [ ] **Коммит после «да» пользователя:** `feat(tcc7): K constraint, sret/mret/sfence.vma, .macro, packed enums, GCC predefs`.

---

## Фаза 4: Tilck компилируется и линкуется на Kronos

**Зачем:** получить ядро Tilck, которое собрал гость, прежде чем заниматься загрузкой.

**Вход:** гейт фазы 3 зелёный.

**Выход:** `src/stages/12-tilck/artifacts/tilck.elf` выгружен из гостя; хостовая проверка ELF печатает `tilck.elf ok`.

**Зависит от:** фаза 3.

Все программы этой фазы собирает `/bin/tcc7b`. Общий набор флагов для программ Kronos ниже обозначен как `K7`: `-I /include -nostdlib -Wl,-Ttext=0x10000 -Wl,-oformat=elf64-little /lib/crt.s /lib/libc.c /src/tcc7/lib/lib-arm64.c`.

### Задача 14. Пины Tilck, dtc, Linux uapi

**Files:**
- Create: `src/stages/12-tilck/README.md`
- Create: `build/sources/tilck-01dacaa5.tar`, `build/sources/dtc-<тег>.tar`, `build/sources/linux-uapi-v6.6.tar` (gitignore)
- Modify: `src/scripts/lib/phases.py` (`"12": "12-tilck",`)

- [x] **Архивы**

```bash
mkdir -p /tmp/pin-tilck && cd /tmp/pin-tilck
git clone -q --bare https://github.com/vvaltchev/tilck.git tilck.git
git --git-dir=tilck.git archive --format=tar 01dacaa51a074474a4e5491fc80addfe98956500 -o "$OLDPWD/build/sources/tilck-01dacaa5.tar"
git clone -q --bare https://git.kernel.org/pub/scm/utils/dtc/dtc.git dtc.git
git --git-dir=dtc.git tag -l 'v1.7.*' | sort -V | tail -1
```

Последний тег `v1.7.*` записать в README и архивировать:

```bash
git --git-dir=dtc.git archive --format=tar <тег> -o "$OLDPWD/build/sources/dtc-<тег>.tar"
git clone -q --depth 1 --branch v6.6 --filter=blob:none --sparse https://github.com/torvalds/linux.git linux
cd linux && git sparse-checkout set --no-cone /include/uapi/ /arch/riscv/include/uapi/
git rev-parse HEAD
git archive --format=tar HEAD include/uapi arch/riscv/include/uapi -o "$OLDPWD/build/sources/linux-uapi-v6.6.tar"
cd "$OLDPWD" && shasum -a 256 build/sources/tilck-01dacaa5.tar build/sources/dtc-*.tar build/sources/linux-uapi-v6.6.tar
```

Ожидание: `rev-parse` у Linux — `ffc253263a1375a65fa6c9f62a893e9767fbebfa`. Три sha256 записать в README.

- [x] **README стадии**: что это (ядро Tilck, собирается гостем tcc7b), три пина с sha256, лицензии (Tilck BSD-2, libfdt BSD-2/GPL-2, Linux uapi GPL-2 with syscall note), список наших файлов из задач 15–18 и строка успеха `tilck.elf ok`.

### Задача 15. `headers-install.c`

**Files:**
- Create: `src/stages/12-tilck/src/headers-install.c`
- Modify: `src/scripts/kronos.py`, `src/stages/09-kronos/src/kmain.c`

Замена `make headers_install` для Kronos, где нет sed и `getdents`. Список файлов хост кладёт отдельным файлом `/src/uapi/FILES` (по пути на строку). Это метаданные посадки, не генерация.

Поведение (как `scripts/headers_install.sh` в Linux v6.6):
- вход: `/src/uapi/FILES`, файлы `/src/uapi/include/uapi/<p>` и `/src/uapi/arch/riscv/include/uapi/<p>`;
- выход: `/usr/include/<p>`; файлы архитектуры кладутся поверх общих;
- в каждом файле: удалить слова `__user`, `__force`, `__iomem`; удалить строки `#include <linux/compiler.h>`; заменить `__attribute_const__` на `__attribute__((__const__))`;
- для каждого `include/uapi/asm-generic/X.h`, для которого нет `arch/riscv/include/uapi/asm/X.h`, создать `/usr/include/asm/X.h` с одной строкой `#include <asm-generic/X.h>`.

- [x] **Тест** `src/stages/12-tilck/tests/uapi-check.c` (Files: Create): компилируется `/bin/tcc7b -I /usr/include ...`, включает `<linux/fb.h>`, `<linux/sched.h>`, `<linux/kd.h>`, `<linux/major.h>`, `<linux/auxvec.h>`, `<asm-generic/signal-defs.h>`, `<asm/types.h>` и печатает `uapi ok`. До задачи — `include file not found`.

```c
#include <stdio.h>
#include <asm/types.h>
#include <asm-generic/signal-defs.h>
#include <linux/auxvec.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/sched.h>

int main(void)
{
    struct fb_var_screeninfo v;
    if (sizeof(v.xres) != 4)
        return 1;
    puts("uapi ok");
    return 0;
}
```

- [x] **Программа.** Ввод-вывод только через `open`/`read`/`write`/`close`/`mkdir` из `/lib/libc.c` (их уже использует tcc6). Путь каталога создавать по частям через `mkdir` (Kronos поддерживает `mkdirat`). Около 150 строк.

- [x] **Посадка:** `collect_kfs_tilck()` в `kronos.py` (тот же `_tar_files` с sha256) кладёт файлы uapi в `/src/uapi/...`, их список — в `/src/uapi/FILES`, исходник — в `/src/tilck-tools/headers-install.c`, тест — в `/src/tilck-tests/uapi-check.c`.

- [x] **Шаг в `kmain.c`:** собрать `/bin/headers-install` (`/bin/tcc7b -o /bin/headers-install K7 /src/tilck-tools/headers-install.c`), запустить, собрать и запустить `uapi-check`. Маркеры: `headers-install built`, `uapi ok`.

### Задача 16. `gen-tilck-config.c`

**Files:**
- Create: `src/stages/12-tilck/src/gen-tilck-config.c`, `src/stages/12-tilck/src/tilck-config.txt`

Замена шагов CMake, которые пишут `tilck_gen_headers/`. Поведение то же, что у генератора из проверки 2026-10-28.

- [x] **`tilck-config.txt`** — значения, по строке `ИМЯ=значение`; имена из шаблонов `config/*.h`, `config/kernel/*.h`. Значения по умолчанию взять из `CMakeLists.txt` Tilck (ветка `riscv64`, `KRN32_LIN_VADDR` выключен) и `other/bsp/riscv64/qemu-virt/board_bsp.cmake`:

```
ARCH_GCC_TC=riscv64
ARCH_BITS=64
BASE_VA=0x2000000000
LINEAR_MAPPING_MB=896
KERNEL_BASE_VA=0xFFFFFFFF80000000
KERNEL_VADDR=0xFFFFFFFF80200000
KERNEL_PADDR=0x80200000
KERNEL_STACK_PAGES=4
CMAKE_BUILD_TYPE=Release
DEFAULT_SHELL=/initrd/bin/init
START_SCRIPT=/initrd/etc/start
TILCK_DEVSHELL_PATH=/initrd/bin/init
ENABLED_MODULES_LIST=console serial riscv_irq goldfish null sysfs tracing
ALL_MODULES_LIST=console serial riscv_irq goldfish null sysfs tracing
ON=KERNEL_64BIT_OFFT PANIC_SHOW_REGS KRN_RESCHED_ENABLE_PREEMPT KERNEL_SERCON
```

Числовые параметры `KRN_*`/`*_HANDLES`/`*_HZ`/`*_STACK_PAGES` и параметры модулей (`KRN_TRACE_PRINTK_ON_BOOT`, `KRN_PRINTK_ON_CURR_TTY`, `KRN_FB_*` и другие из `modules/*/options.cmake`) дописать по `tilck_option(...)` в `CMakeLists.txt` и `modules/*/options.cmake`, значение — их `DEFAULT`. Неизвестное имя в шаблоне — ошибка генератора (`unknown @NAME@`), а не тихий `0`.

- [x] **Программа:** читает `tilck-config.txt` и список шаблонов `/src/tilck/CONFIG_FILES`. Подставляет `@NAME@`; `#cmakedefine01 X` превращает в `#define X 1` или `0` (по списку `ON=`); `#cmakedefine X` — в `#define X` или комментарий. Пишет `config/*.h` в `/build/tilck/tilck_gen_headers/`, а `config/kernel/*.h` — в `/build/tilck/kbin/tilck_gen_headers/`, где `BUILD_DIR=/build/tilck`. Отдельно пишет `modules_config.h` (по строке `MOD_<имя> 0/1` на каждый каталог `modules/`, список каталогов — в `/src/tilck/MODULES`; плюс `TTY_COUNT 2`, `SERIAL_CON_IN_VIDEO_MODE 0`, `KERNEL_SERCON 1`, `TERM_BIG_SCROLL_BUF 0`, `PS2_DO_SELFTEST 0`, `PS2_VERBOSE_DEBUG_LOG 0`) и пустой `generated_config.h` с `#pragma once`.

- [x] **Шаг в `kmain.c`:** собрать и запустить. Маркер: `tilck config ok` (печатает сама программа после записи всех файлов).

### Задача 17. Патчи Tilck

**Files:**
- Create: `src/stages/12-tilck/src/override/include/tilck/common/compiler.h`, `override/kernel/main.c`, `override/kernel/safe_ringbuf.c`, `override/common/itoa.c` (копии файлов архива Tilck с изменениями ниже; `*.cpp` в сборку не идут)

- [x] **`compiler.h`:** перед `#else` с `#error Compiler not supported` добавить ветку

```c
#elif defined(__TINYC__)

   #define COMPILER_NAME            "tcc"
   #define COMPILER_MAJOR           0
   #define COMPILER_MINOR           9
   #define COMPILER_PATCHLEVEL      28
```

- [x] **`kernel/main.c`:** строки `#include <3rd_party/acpi/acpi.h>` и `#include <3rd_party/acpi/acexcep.h>` обернуть в `#if MOD_acpi` … `#endif`. Код, который использует ACPI, уже стоит под `if (MOD_acpi …)`. Если tcc всё равно потребует объявления, обернуть и эти блоки в `#if MOD_acpi`.

- [x] **`safe_ringbuf.c`:** то же, что `kernel/safe_ringbuf.cpp`, на C. Шаблоны `__safe_ringbuf_write<n>` и `__safe_ringbuf_read<n>` для n = 0, 1, 2, 4, 8 разворачиваются макросом в функции `__safe_ringbuf_write_<n>` / `__safe_ringbuf_read_<n>` с типом `u8`/`u16`/`u32`/`u64` (n = 0 — копирование `memcpy` по `rb->e_size`). Внешние функции и их сигнатуры — те же, что в блоках `extern "C"` исходника.

- [x] **`itoa.c`:** то же, что `common/itoa.cpp`, на C. Шаблоны `__uitoa_fixed<T>`, `__itoa<T>` и третий шаблон (строка 66) разворачиваются макросом в функции для `s32`, `s64`, `u32`, `u64`. Внешние функции из `extern "C"` — с теми же именами и сигнатурами.

- [x] **Проверка** — фаза 4 гейт: эти четыре файла компилируются в `tilck.rsp`.

### Задача 18. Компиляция и линковка Tilck

**Files:**
- Create: `src/stages/12-tilck/src/cflags.rsp`, `src/stages/12-tilck/src/SOURCES`, `src/stages/12-tilck/src/link.rsp`
- Modify: `src/scripts/kronos.py`, `src/stages/09-kronos/src/kmain.c`

Один вызов tcc7b на файл: в Tilck три разных `misc.c` (`kernel/`, `common/`, `kernel/arch/riscv/`), и при одном общем `-c` их объекты затёрли бы друг друга. `kmain.c` читает `SOURCES` (путь на строку) и для каждого пути `P` запускает `/bin/tcc7b @/src/tilck/cflags.rsp -I <каталог P> -c /src/tilck/P -o /build/tilck/obj/<P с заменой / на __>.o`. `link.rsp` перечисляет те же объекты явно и линкует их. Все три файла — файлы стадии в репозитории, написанные по списку ниже; хост их не генерирует. Пути в KFS: исходники Tilck — `/src/tilck/<путь в архиве>` (с подстановкой `override/`), libfdt — `/src/tilck/libfdt/*` (из архива dtc), объекты — `/build/tilck/obj/`.

Сажать только нужные каталоги архива Tilck: `include/`, `kernel/`, `common/`, `config/`, `modules/{console,serial,riscv_irq,goldfish,null,sysfs,tracing}/`. `kronos.py` печатает итоговый размер `ramfs.bin`. Если он больше `0x3E00000` (область KFS от `+2 МБ` до `+64 МБ` в `entry.s`), фаза останавливается.

- [x] **Флаги компиляции** (`cflags.rsp`):

```
-nostdinc -I /lib/tcc7/include -I /usr/include
-I /src/tilck/include -I /src/tilck/include/system_headers -I /src/tilck/common/3rd_party
-I /build/tilck/kbin -I /build/tilck -I /src/tilck/libfdt
-D__TILCK_KERNEL__ -D__ACTUAL_TILCK_KERNEL_BINARY__ -D__riscv64 -DNDEBUG -DTILCK_RELEASE_BUILD
-fno-common
```

`/usr/include` здесь — это заголовки uapi из задачи 15. Libc ядру не нужна: Tilck — freestanding, свои `string.h` и прочее он берёт из `include/system_headers`. Если файл просит заголовок libc, которого там нет, это фиксируется в README как найденный пробел и закрывается в этой задаче минимальным заголовком в `src/stages/12-tilck/src/override/`.

- [x] **`SOURCES`** (114 файлов из проверки плюс две замены C++ и libfdt):

```
kernel/arch/riscv/cpu.c kernel/arch/riscv/cpu_features.c kernel/arch/riscv/debug.c
kernel/arch/riscv/early_fdt.c kernel/arch/riscv/fault.c kernel/arch/riscv/fpu_memcpy.c
kernel/arch/riscv/irq.c kernel/arch/riscv/misc.c kernel/arch/riscv/mmap.c
kernel/arch/riscv/paging.c kernel/arch/riscv/paging_generic.c kernel/arch/riscv/panic.c
kernel/arch/riscv/process_riscv.c kernel/arch/riscv/sbi.c kernel/arch/riscv/timer.c
kernel/arch/riscv/fault_resumable.S kernel/arch/riscv/fpu.S kernel/arch/riscv/kernel_yield.S
kernel/arch/riscv/misc.S kernel/arch/riscv/start.S kernel/arch/riscv/trap_entry.S
kernel/arch/riscv/user_copy.S kernel/arch/riscv/vdso.S kernel/arch/riscv64/arch_syscalls.c
kernel/auth.c kernel/cmdline.c kernel/cwd.c kernel/datetime.c kernel/debug.c kernel/elf.c
kernel/execve.c kernel/exit.c kernel/fork.c kernel/interrupts.c kernel/kb_input.c kernel/kcond.c
kernel/kmutex.c kernel/ksem.c kernel/main.c kernel/misc.c kernel/modules.c kernel/pipe.c
kernel/poll.c kernel/printk.c kernel/process.c kernel/ringbuf.c kernel/rwlock.c kernel/sched.c
kernel/select.c kernel/signal.c kernel/snprintk.c kernel/sort.c kernel/switch.c kernel/syscalls.c
kernel/tilck_cmd.c kernel/timer.c kernel/uefi.c kernel/user.c kernel/waitpid.c kernel/wobj.c
kernel/wth.c kernel/wth_aux.c kernel/safe_ringbuf.c kernel/bintree/avl_bintree.c
kernel/fs/devfs.c kernel/fs/fat32.c kernel/fs/fat32_mm.c kernel/fs/flock.c kernel/fs/fs_syscalls.c
kernel/fs/kernelfs.c kernel/fs/mp.c kernel/fs/vfs_util.c kernel/fs/ramfs/ramfs.c
kernel/kmalloc/kmalloc.c kernel/mm/process_mm.c kernel/mm/process_mm_base.c kernel/mm/system_mmap.c
kernel/net/net.c kernel/tty/term_aux.c kernel/tty/term_mgmt.c kernel/tty/tty.c kernel/tty/tty_input.c
kernel/tty/tty_ioctl.c kernel/tty/ttyaux.c kernel/vfs/vfs.c
common/asserts.c common/fat32_base.c common/misc.c common/panic.c common/string_util.c common/itoa.c
common/3rd_party/fdt_helper.c common/3rd_party/datetime.c common/3rd_party/crc32.c
modules/console/console.c modules/console/tables.c modules/console/video_term.c
modules/serial/serial.c modules/serial/serial_term.c modules/serial/riscv/fdt_serial.c
modules/serial/riscv/ns16550.c modules/riscv_irq/riscv/fdt_irqchip.c
modules/riscv_irq/riscv/riscv-intc.c modules/riscv_irq/riscv/riscv-plic.c
modules/goldfish/riscv/fdt_rtc.c modules/goldfish/riscv/goldfish.c modules/null/null.c
modules/sysfs/config.c modules/sysfs/sysfs.c modules/tracing/ptype_argp.c
modules/tracing/ptype_buffer.c modules/tracing/ptype_iov.c modules/tracing/tracing.c
modules/tracing/tracing_cmd.c modules/tracing/tracing_metadata.c modules/tracing/tracing_types.c
```

плюс все `libfdt/*.c` из архива dtc (`fdt.c`, `fdt_ro.c`, `fdt_wip.c`, `fdt_sw.c`, `fdt_rw.c`, `fdt_strerror.c`, `fdt_empty_tree.c`, `fdt_addresses.c`, `fdt_overlay.c`, `fdt_check.c` — точный список по архиву). В `SOURCES` пути без пробелов, по одному на строку.

- [x] **Цикл в `kmain.c`:** функция `static int build_tilck_objs(void)` берёт `kfs_get("/src/tilck/SOURCES", &n)`, идёт по строкам и для каждой собирает argv из 9 элементов (`/bin/tcc7b`, `@/src/tilck/cflags.rsp`, `-I`, каталог, `-c`, исходник, `-o`, объект, `0`) в статических буферах. Затем `run(...)`, после него `kfs_get(объект)`. Если объекта нет, функция печатает `tilck obj fail <путь>` и возвращает 0. Если все объекты на месте — печатает `tilck objs ok` и возвращает 1.

- [x] **Линковка** (`link.rsp`): все объекты, `-nostdlib -static -Wl,-Ttext=0xFFFFFFFF80201000 -o /build/tilck/tilck.elf`. Адрес — `KERNEL_VADDR + kernel_text_offset` из `kernel/arch/riscv64/linker_script.ld`. Символы, которые задаёт скрипт линкера Tilck и использует код (`__bss_start`, `__bss_stop`, `_end`, границы `init_array`, `tilck_info`), проверить по `grep` в исходниках:
  - `_end`, `__bss_start` tcc определяет сам;
  - для остальных — свой маленький файл `src/stages/12-tilck/src/override/kernel/arch/riscv/ldsyms.c` с определениями через символы, которые tcc создаёт для секций (`__start_<sec>`/`__stop_<sec>`, `__init_array_start`/`__init_array_end`). Если для какого-то символа нет эквивалента у линкера tcc, это фиксируется в README как пробел линкера и закрывается правкой `tccelf.c` в `src/stages/11-tcc7/src/override/` с тестом в `11-tcc7/tests/`.

- [x] **Посадка:** `collect_kfs_tilck()` кладёт файлы Tilck, override, libfdt, `cflags.rsp`, `SOURCES`, `link.rsp`, `tilck-config.txt`, `CONFIG_FILES`, `MODULES` и инструменты. Вызывается в `run_tcc_self_k` после `collect_kfs_tcc7()`. Маркеры добавить после `"predefs ok"`: `"headers-install built"`, `"uapi ok"`, `"tilck config ok"`, `"tilck objs ok"`, `"tilck linked"`. После `dump_snap(... "tcc7b" ...)` добавить:

```python
        dump_snap(q, q.buf, "tilck", os.path.join(artifacts("12-tilck"), "tilck.elf"))
```

- [x] **Шаг в `kmain.c`:** после тестов tcc7 — `headers-install`, `uapi-check`, `gen-tilck-config`, затем `build_tilck_objs()`. Если она вернула 1 — `run` `/bin/tcc7b @/src/tilck/link.rsp`. Если `/build/tilck/tilck.elf` есть — `tilck linked` и `snap_file("/build/tilck/tilck.elf", "tilck")`. Файлы `@…` раскрывает сам tcc mob (`libtcc.c`, «read @listfile»).

### Задача 19. Гейт фазы 4

**Files:** нет.

- [x] **Прогон на QEMU**

```bash
python3 -u src/scripts/kronos.py --tcc-self-k 2>&1 | tee /tmp/p4-gate.log
grep -E 'tcc77 ok|predefs ok|uapi ok|tilck config ok|tilck objs ok|tilck linked' /tmp/p4-gate.log
```

Ожидание: все шесть строк.

- [x] **Проверка ELF на хосте** (только чтение байтов, выгруженных из гостя)

```bash
python3 -I - src/stages/12-tilck/artifacts/tilck.elf <<'EOF'
import struct, sys
d = open(sys.argv[1], "rb").read()
assert d[:4] == b"\x7fELF" and d[4] == 2 and d[5] == 1, "not ELF64 LE"
e_machine, = struct.unpack_from("<H", d, 18)
assert e_machine == 243, "not RISC-V: %d" % e_machine
e_entry, e_phoff = struct.unpack_from("<QQ", d, 24)
e_phentsize, e_phnum = struct.unpack_from("<HH", d, 54)
loads = []
for i in range(e_phnum):
    p_type, _f, _off, p_vaddr = struct.unpack_from("<IIQQ", d, e_phoff + i * e_phentsize)
    if p_type == 1:
        loads.append(p_vaddr)
assert loads and min(loads) >= 0xFFFFFFFF80200000, "PT_LOAD below KERNEL_VADDR: %s" % [hex(x) for x in loads]
assert e_entry >= 0xFFFFFFFF80200000, "entry %x" % e_entry
print("tilck.elf ok entry=%#x loads=%s size=%d" % (e_entry, [hex(x) for x in loads], len(d)))
EOF
```

Ожидание: `tilck.elf ok …`.

- [x] **Дерево:** `git status --short` ⊆ Allowed paths.

- [ ] **Коммит после «да» пользователя:** `feat(tilck): guest tcc7 compiles and links the Tilck riscv64 kernel`.

---

## Итоговый отчёт (один на весь план)

После гейта фазы 4 — короткий отчёт пользователю:
- какие гейты прошли, с выдержкой строк UART;
- какие пробелы tcc6 / tcc7 / линкера всплыли сверх известных и чем закрыты (файл, тест);
- что записано в README стадий 11 и 12;
- что не сделано и почему.
