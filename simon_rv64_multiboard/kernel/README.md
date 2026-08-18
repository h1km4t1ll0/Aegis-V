# kernel/

Единственный бинарник «снаружи»: `boot.S` собирается хостовым gcc/clang.

- `boot.S` — stage 1, адрес загрузки платы. Читает `simon.hex0` из хвоста образа и прыгает в simon.
- `simon.S` — stage 2: UART CLI, ФС из `build/files.pl`, `hex0`/`do`, syscalls для M2/Mes.

Адреса и UART не здесь — в `boards/<плата>.h`.
