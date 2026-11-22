# Бутстрап: с нуля до Linux

Живой план. Сюда же писать прогресс, чтобы можно было продолжить с другой сессии.

Дата старта: 2026-09-06.

## Цель

Гость сам, с нуля, собирает Linux и грузится в него.

Хост после сида Simon больше ничего не компилирует. Не тащить `riscv64-elf-gcc` / clang в лестницу. Не подменять компилятор заглушкой, которая всегда печатает hello.

Критерий конца: в QEMU `virt` грузится ядро Linux, собранное гостевым gcc; userspace минимум busybox; этим же toolchain ядро можно пересобрать ещё раз уже под Linux.

## Правила

- Guest compiles guest. Один QEMU: Simon → reset → Kronos → дальше.
- Не прыгать через ступень.
- Не считать «бинарь слинковался» за «компилятор работает». Работает = взял исходник, собрал, программа делает то, что должна.
- Без упрощений в смысле семантики: пустышки вместо backend / GMP / as / ld — это не gcc.
- **Открытость.** В лестницу входят исходники с записанным коммитом или хешем. Собирает их гость. Готовый бинарь компилятора (Codeberg, Guix, Debian, старый `cc1.elf`) в лестницу не класть.
- **Следующий компилятор — GCC 4.6.4** (форк Ekaitz, RISC-V), его собирает гостевой `tcc6`. Ступени 4.0.4 и 4.7.4 сняты.

## Где мы сейчас

Сид до **tcc6** зелёный: UART `hello from tcc6`. Ступени gcc 4.0.4 и 4.7.4 сняты (каталоги удалены, `--from-cc1` снят).

**Сейчас (2026-11-21):** GCC 9.5.0 собран внутри Tilck, прошёл `make bootstrap` (`Comparison successful`), C/C++ sanity и проверку неподвижной точки на 100 файлах (0 различий). Образ: `20-gcc95/artifacts/rootfs-gcc9.tar`. Стадия `21-linux`: Linux 6.6.0 собран внутри Tilck нашим GCC и загрузился в QEMU. Прошли PID 1, fork/pipe/wait, файловый IO, BusyBox и компиляция/запуск C/C++ уже под Linux; штатное выключение. Затем Linux полностью пересобрал ядро своим GCC и загрузился как `6.6.0-aegis-selfhost`: C/C++, Bash, pthread и ext4 прошли проверки. Текущий нативный образ: `21-linux/artifacts/rootfs-linux-selfhost.tar`, рабочая копия диска со всеми исходниками и объектами: `21-linux/artifacts/linux-selfhost.raw`. Прежний `rootfs-linux-ready.tar` сохранён; запуск нового образа требует явного `--rootfs`. Диск `21-linux/artifacts/linux-data-03.raw`: ext4 4 ГиБ, создан гостевым mke2fs; два запуска QEMU подтвердили сохранность данных и собранной на диске программы. Последний образ загружает корень с ext4 на `21-linux/artifacts/linux-diskroot.raw` через маленький initramfs, запускает Bash и сохраняет системные файлы между запусками. План: `docs/2026-11-19-linux66-plan.md`.

Старая запись: посадить исходники [ekaitz-zarraga/gcc](https://codeberg.org/ekaitz-zarraga/gcc) `c0d20242b1` и собрать их гостевым `tcc6`. Успех: свой `cc1` печатает `4.6.4` и собирает hello. Потом этот компилятор, уже с C++, собирает апстримный GCC 9 из исходников.

## Лестница

Статус: `готово` / `частично` / `нет` / `сейчас`.

| # | Что | Зачем | Статус |
|---|-----|--------|--------|
| 0 | hex0 → … → tcc-pnut → Kronos → tcc2…tcc6 | сид | готово |
| 1 | GCC 4.6.4, форк Ekaitz, исходники | мост: tcc собирает C-компилятор, который умеет C++ и RISC-V | готово (под Tilck, 2026-11-06) |
| 2 | musl, собранный этим gcc, затем пересборка 4.6.4 с C++ | libc и `cc1plus` | готово (`musl2`, `gcc3`, `gcc4`) |
| 3 | настоящие as, ld, ar, nm; shell и make | чтобы дальше собирать gcc как в live-bootstrap | готово (binutils 2.30, bash, make 3.82) |
| **4** | **апстримный GCC 9.5 из исходников** | официальный RISC-V | **готово: C/C++, bootstrap, 100 файлов без различий** |
| **5** | **Linux + BusyBox, затем пересборка на Linux** | **цель** | **готово: 6.6.0-aegis-selfhost собран под Linux и прошёл отдельную загрузку, C/C++, Bash, pthread и ext4** |

Цепочка, которой на RISC-V дошли до GCC 9: tcc → этот 4.6.4 → апстримный 9.5. Исходники 9 берутся с ftp.gnu.org, когда 4.6.4 уже живой.

## Что уже сделано (факт)

- Лестница hex0…tcc6 на Kronos зелёная. См. `docs/2026-08-29-compiler-ladder.md`.
- 2026-10-18: gcc 4.0.4 и 4.7.4 сняты с лестницы. Их каталоги и бинари удалены. Следующий компилятор — исходники GCC 4.6.4.

## Журнал

Писать сюда коротко, с датой, когда ступень сдвинулась или выяснился блокер.

- 2026-11-21 — Корень Linux перенесён на ext4. Два запуска подтвердили сохранение программы в `/usr/local/bin`, настроек в `/etc` и `/root/.bashrc`. С отдельной последовательной консоли проверен интерактивный Bash с управлением заданиями.
- 2026-11-20 — Linux полностью пересобрал ядро нашим GCC 9.5; новое ядро 6.6.0-aegis-selfhost загрузилось в отдельном QEMU и прошло проверки C/C++, Bash, двух pthread и сохранности ext4. Исходники, объекты и логи сохранены на диске, контрольный образ прошёл гостевую/хостовую SHA256. Корень пока initramfs.
- 2026-11-19 — Linux 6.6.0 собран гостевым GCC 9.5, загрузился с нашим SBI, прошёл userspace/C/C++ тесты. GCC 9.5 ранее подтвердил bootstrap и fixed point. Ext4-диск 4 ГиБ прошёл проверку между двумя запусками QEMU; программа, собранная гостевым GCC прямо на диске, сохранилась и снова запустилась.
- 2026-10-18 — **курс: GCC 4.6.4 из исходников.** 4.0.4 и 4.7.4 сняты. Готовые бинари компилятора в лестницу не входят. Собирает гостевой tcc6.
- 2026-09-07 — **курс: ступень 0b, не tcc.** 4.7.4 собирает только 4.0.4. tcc-бинарь 4.7.4 и `kronos_*` под miscompile — тупик. Дальше чинить gcc 4.0.4, пока не съест TU 4.7.4. (Путь закрыт 2026-10-18.)
- 2026-09-06 — цель и лестница зафиксированы. 4.7.4 cc1 слинкован tcc, при запуске trap. (Путь отменён 2026-09-07.)
- 2026-09-06 — краш не в crt: `putc('\\n')` в obstack diagnostic/pretty-print, указатель на контекст мусор. Следующее: откуда вызывается эта функция и почему `*arg` невалиден (init diagnostic vs layout tcc).
- 2026-09-06 — линк cc1: `'init_ggc_heuristics' defined twice` (заглушка ggc-page.c + ggc-common.c). Убрал дубль из заглушки.
- 2026-09-06 — `init_emit_once` падает в `gen_const_vector` (после `em-vx`). Пропуск vector/fixed `const_tiny_rtx` для целого RV64.
- 2026-09-07 — после `cc1474-bd` trap `mcause 0xc` в `lang_hooks.init`. `LANG_HOOKS_INITIALIZER` заменён на field-by-field fill; `debug_hooks` принудительно `do_nothing`.
- 2026-09-07 — fill живой (`cc1474-lh`/`lx`), `lang_hooks.init()` всё равно мусорный PC. Следующее: прямой вызов `c_objc_common_init` (настоящий C frontend).
- 2026-09-07 — прямой `c_objc_common_init` дошёл до `cc1474-cd`, затем trap на `c_init_decl_processing` (тот же мусорный PC). Дробим init + UART `dp0`.
- 2026-09-07 — `c_parse_init` и push_scope живые (`pi`/`pj`/`ps`/`nb`). Trap на `c_common_nodes_and_builtins`.
- 2026-09-07 — дробление `c_common_nodes_and_builtins` + прямой `pushdecl` вместо `lang_hooks.decls.pushdecl`. Пропуск optional TImode/decimal/fixed-point хуков `targetm` (тот же класс мусорного PC).
- 2026-09-07 — split живой (`nb`/`nb0`). Trap на `build_common_tree_nodes` (тот же PC). Дробим её; `va_list` = `void*` без `targetm.build_builtin_va_list`.
- 2026-09-07 — `tn0` живой, trap на `make_node(ERROR_MARK)`. Дробим `tree_code_size`: tcc ломает switch и падает в `lang_hooks.tree_size`.
- 2026-09-07 — `tn0b` живой (`ERROR_MARK` создан). Trap на следующем шаге: `initialize_sizetypes` → первый `INTEGER_TYPE` зовёт мусорный `targetm.set_default_type_attributes`. Пропуск хука + дробление `layout_type`.
- 2026-09-07 — `initialize_sizetypes` / `build_common_tree_nodes` / `c_common_nodes_and_builtins` живые (`sz3`/`tnz`/`nbz`). Uncached `build_int_cst_wide` (без сломанного htab). Дальше trap `mcause 0xd` в `lang_dependent_init_target` (`lt`, скорее `init_optabs`). Менять `toplev.c`/`optabs.c` после живых функций нельзя: tcc ломает тот же TU, откат на `ra`. Частичный overwrite `targetm.*` тоже бьёт `backend_init`. Дальше: полное field-by-field заполнение `targetm` в маленьком `riscv64-targetm.c`, как `lang_hooks`.
