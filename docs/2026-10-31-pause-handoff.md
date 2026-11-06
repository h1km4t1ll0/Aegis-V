# Пауза 2026-10-31: где мы и как продолжить

Цель: GCC 4.6.4, собранный гостем с нуля под QEMU. Дизайн: `docs/2026-10-28-gcc464-ladder-design.md`.

## Что готово (всё проверено на QEMU, ничего не закоммичено)

Лестница: hex0 → … → Kronos → tcc6 → **tcc7** (upstream tcc mob + наши патчи) → ядро **Tilck**, собранное гостем, с нашей SBI-прослойкой → **musl** → ступени внутри Tilck. Каждая ступень отдаёт `rootfs.tar` через `/dev/outbox`, и следующая стартует с него. Повторные прогоны дают тот же sha256.

| Ступень | Что | Артефакт | sha256 (начало) |
|---|---|---|---|
| Kronos | tcc6 → tcc7, Tilck, SBI, musl, семя | `14-seed/artifacts/seed-rootfs.tar` | `72501151…` |
| `tcc` | tcc под musl, сходится сам с собой на Tilck | `14-seed/artifacts/rootfs-tcc.tar` | `1b48342f…` |
| `tools1` | make 3.82, patch, gzip, tar 1.12, sed, bzip2 | `15-early-tools/artifacts/rootfs-tools1.tar` | `bfdf3240…` |
| `tools2` | coreutils 5.0, yacc, bash 2.05b | `15-early-tools/artifacts/rootfs-tools2.tar` | `92553b32…` |
| `build1` | grep, gawk, diffutils, m4, flex, findutils | `16-buildtools/artifacts/rootfs-build1.tar` | `305c0c62…` |
| `binutils` | binutils 2.30 | `17-binutils/artifacts/rootfs-binutils.tar` | `10bd8e33…` |
| `gmp` | gmp 4.3.2, mpfr 2.4.2, mpc 0.8.1, `make check` зелёный | `18-gmp-mpfr-mpc/artifacts/rootfs-gmp.tar` | `1461af4e…` |
| `gcc1` | **GCC 4.6.4, собранный tcc** (C), hello, `-O2`-тест | `19-gcc464/artifacts/rootfs-gcc1.tar` | `21bcb7ae…` |
| `musl2` | musl, пересобранный gcc-1 | `19-gcc464/artifacts/rootfs-musl2.tar` | `8a13fc3c…` |

Подробности и все отклонения — в README каждой стадии (`src/stages/11-tcc7` … `19-gcc464`).

## Что идёт прямо сейчас

**Прогон 5 ступени `gcc3`** (`make bootstrap`, план 5, фаза 3) запущен 2026-10-31 около 10:40 и оставлен работать:
- процессы: `python3 -u src/scripts/tilck.py --rung gcc3` (pid 16679) и QEMU (pid 16681);
- лог: `/tmp/gcc3-gate.log`; UART: `src/stages/12-tilck/artifacts/uart-tilck.log` (пишется в конце);
- на момент паузы: `stage1 cc1 built` (11099 с), `stage1 libgcc built` (11229 с), идёт stage 2;
- таймаут 48 часов. Ожидаемое время до конца — ещё несколько часов (stage 2, stage 3, сравнение, установка, тесты, упаковка).

Проверить:
```bash
tail -20 /tmp/gcc3-gate.log
```
Успех — в логе `Comparison successful`, `bootstrap compare ok`, `hello from gcc 4.6.4`, `gcc3 sanity ok`, `rootfs sha256 match`, `rung done gcc3`, и появился `src/stages/19-gcc464/artifacts/rootfs-gcc3.tar`.

Если машина нужна, прогон можно остановить (тогда его придётся запустить заново, около 10 часов):
```bash
kill 16681
```

## Главные находки последних дней

- **Причина падений gcc-1 — не tcc, а Tilck.** Три ошибки в `munmap()`/`mremap()` портили кучу у большого `cc1`. Исправлены, тесты `munmapspan ok`, `munmappart ok`, `mremap ok`. Разбор — в `19-gcc464/README.md`, раздел «Root cause».
- **Tilck не запускал `#!`-скрипт с >30 аргументами** (gcc `collect-ld`). Исправлено в `kernel/execve.c`, тест `scriptargs ok`.
- **Эпилог функции в tcc читал ниже `sp`.** Отсюда редкие зависания. Исправлено, тест `epilog ok`.
- **tcc неправильно кодировал `lr`/`sc`**, и атомарные операции musl обнуляли память. Исправлено.
- gmp/mpfr/mpc собраны tcc и зовут `alloca` из рантайма tcc. В `gcc3/run.sh` это закрыто `LIBS=alloca.o`. Чище — пересобрать их gcc позже.

## Что дальше

1. **Проверить итог прогона 5 `gcc3`.**
   - Зелёный — перепроверить (`tilck.py --rung gcc3` повторно не обязательно: достаточно лога и sha256 артефакта) и перейти к фазе 4.
   - Красный — разбор по логу и UART. Исправлять с тестом, как раньше.
2. **Фаза 4 плана 5 (C++)** — `docs/2026-10-30-gcc464-plan.md`. Известное препятствие: tar 1.12 не понимает префикс ustar. 7 заголовков `libstdc++-v3/include/ext/pb_ds/detail/` не распакуются. Предложение: маленький свой распаковщик ustar на C (поле `prefix` + `name`), собранный gcc-3, только для этих файлов. Записать в README.
3. После C++ — строки журнала в design doc и `docs/bootstrap-to-linux.md`, итоговый отчёт.
4. **Коммиты.** Ничего не закоммичено (ждём «да»). Предлагаемые сообщения — в отчётах фаз и в README стадий.

## Как продолжать работу

- Планы: `docs/2026-10-28-tcc7-tilck-plan.md` (выполнен), `docs/2026-10-29-tilck-boot-musl-plan.md` (выполнен), `docs/2026-10-29-tilck-early-tools-plan.md` (выполнен), `docs/2026-10-29-buildtools-binutils-gmp-plan.md` (выполнен), `docs/2026-10-30-gcc464-plan.md` (фазы 1–2 выполнены, 3 идёт, 4 впереди).
- Способ: `/phased-subagent-driven-development` по плану. Ревью по фазам нет, отчёт в конце. Один исполнитель на фазу. Координатор перепроверяет гейт своим прогоном. Один QEMU за раз.
- Сборка всего с нуля до семени: `python3 -u src/scripts/kronos.py --tcc-self-k` (~9 минут). Ступени: `python3 -u src/scripts/tilck.py --rung NAME` (`tcc`, `tools1`, `tools2`, `build1`, `binutils`, `gmp`, `gcc1`, `musl2`, `gcc3`). Тест ядра: `tilck.py --test`.
- Закреплённые исходники лежат в `build/sources/` (gitignore, 29 файлов). Хеши — в README стадий и в `tilck.py`/`kronos.py`.
- Резервные копии рабочего дерева: `build/backup/` (последняя — `2026-10-31-pause.{tgz,diff,list,head}`, до этого — по одной на каждую веху).
- Диагностические инструменты на хосте (не в лестнице): `/tmp/b3-xp/tcc-xp`, `/tmp/r3/bin/tcc`, `/tmp/g464/`, `/tmp/gcc3-*`. `/tmp` может очиститься при перезагрузке — они не нужны для продолжения, только ускоряют отладку.


---

# Пауза 2026-11-05 (17:50): где мы сейчас

**Ступень `gcc3` зелёная** (прогон 5, 31.10): `rootfs-gcc3.tar` sha256 `40d964b6…e47e`.

**Ступень `gcc4` (C++, план 5, фаза 4), прогон 2 (15:46–17:41):**
- C++ собран gcc-3, установлен; `g++ -v: gcc version 4.6.4`, `hello from g++ 4.6.4`, `gcc4 sanity ok`.
- Исправлено по пути: свой распаковщик `gcc4/src/ustar-x.c` (длинные пути pb_ds), `gcc4/src/mkustar-long.patch` (mkustar пишет длинные пути), `gcc4/patches/riscv-init-array.patch` (hello.cpp падал с abort — статические конструкторы).
- Гость дошёл до `rootfs sha256 d64fe635…92c0`, `rung done gcc4`, `rung exit 0`, но хост пометил прогон FAIL: нет `fixedpoint ok`. Поэтому `rootfs-gcc4.tar` не сохранён.
- Логи: `src/stages/19-gcc464/artifacts/{gcc4-gate-run1,gcc4-gate-run2}.log`, `uart-gcc4-run{1,2}.log`.

**Открытая проблема — неподвижная точка.** cc1 от gcc-3 и cc1 от gcc-4 компилируют одни и те же исходники GCC с одинаковыми флагами по-разному: `fixedpoint 71 files, 18 differ` (одинаково в прогонах 1 и 2; cmp работает правильно — пробы 0/1/1). Пример (combine.c, -O1): c3 делает `x*41` сдвигами и сложениями, c4 — `li 41; mul`. Различаются combine, expr, reload, sel-sched, cse, var-tracking, optabs, varasm, tree-cfg, tree-inline, emit-rtl, haifa-sched, tree-data-ref, cfgexpand, ipa-inline (+ O2 combine/expr/cse). Значит, `Comparison successful` в gcc3, скорее всего, был ложным (наш пересчёт: 465 из 595 различаются).

**Что делать дальше:**
1. Детерминизм: один и тот же cc1 (c3, потом c4) дважды на combine.c и expr.c, в том числе с другой длиной окружения или argv. Начатая диагностика («Diagnostic A») убита при паузе, файлов не оставила.
2. Если один cc1 сам с собой не совпадает — неинициализированная память. Главный подозреваемый: Tilck отдаёт ненулевые страницы при anonymous mmap/mremap-росте (calloc в musl 1.1.24 доверяет нулям от ядра). Проверить также bss и brk. Тест в ядре по образцу munmapspan/mremap.
3. Если оба детерминированы, но различаются — один из cc1 собран криво. Искать через expmed.c (synth_mult, init_expmed, alg_hash) и riscv rtx_costs.
4. Исправить с тестом. Если виновато ядро — решить, перегонять ли gcc3 (около 3,5 ч), потом gcc4 (около 1,5 ч) до `fixedpoint ok`, `rootfs sha256 match`, `rung done gcc4`.
5. README 19-gcc464 (прогоны gcc4 и находка), журнал в design doc, `docs/bootstrap-to-linux.md`. Коммит — только после «да».

---

# 2026-11-06: GCC 4.6.4 готов

- Причина расхождения gcc-3/gcc-4 — ошибка форка: `riscv_rtx_costs` с сигнатурой более нового GCC читал за границей таблиц режимов. Исправлено `19-gcc464/gcc3/patches/riscv-rtx-costs.patch` (применяется в `gcc3` и `gcc4`), тест `cc1 layout ok`.
- `gcc3` прогон 6: зелёный, `rootfs-gcc3.tar` `10eca495…e038` (193 119 232 байт).
- `gcc4` прогон 3 и контрольный прогон координатора: оба зелёные, `fixedpoint 71 files, 0 differ`, `fixedpoint ok`, `hello from g++ 4.6.4`, `rootfs sha256 match`, `rung done gcc4`; `rootfs-gcc4.tar` `1caf88b7…b9b4` (215 772 160 байт) — одинаковый в обоих прогонах.
- Открыто (не блокирует): наш пересчёт после `make bootstrap` видит 465 различных пар `prev-*`/текущих объектов при успешном `compare` GCC (README 19-gcc464).
- Логи: `19-gcc464/artifacts/{gcc3-gate-run6,gcc4-gate-run3,gcc4-gate-verify}.log`, `uart-*.log`. Копия: `build/backup/2026-11-06-gcc4-green.tgz`.
- Ничего не закоммичено. Дальше (решает пользователь): GCC 9.5, потом Linux.
