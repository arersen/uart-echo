# TASK 05. UART ECHO

## 1. Налаштування UART на TARGET
(плата: Rasberry Pi 5, система: Buildroot, UART: /dev/ttyAMA0)

Зміни на boot-розділі (FAT, mmcblk0p1):
- config.txt: `enable_uart=1` (+ `dtoverlay=disable-bt`, якщо Pi 3/4/Zero W)
- cmdline.txt: прибрано `console=serial0,115200`
- inittab: прибрано getty на ttyAMA0

Loopback (TX GPIO14 / pin 8 <-> RX GPIO15 / pin 10):


## 2. Застосунок
- `mycom.c`, `Makefile` (all, clean, deploy, CROSS_COMPILE)
- Збірка: `make CROSS_COMPILE=<toolchain>/bin/aarch64-buildroot-linux-gnu-`
- Деплой: `make deploy TARGET_HOST=root@<ip> CROSS_COMPILE=...`

### Перевірка
(результат loopback / обміну з HOST, фрагмент логу)

### Тест на HOST через pty (виконано)
Вхід `Hello World 123\r\n` -> відповідь `hELLO wORLD 123\r\n`.
Лог містить рядки `[YYYY-MM-DD HH:MM:SS] <повідомлення>`.
SIGINT: порт і лог закриваються, код виходу 0.
Неіснуючий пристрій: повідомлення на екран і в лог, код виходу 1.

## 3. Автозапуск (init/S99mycom, BusyBox init)
(результат після reboot)

## 4. Проблеми та висновки
(rootfs 107 МБ повністю заповнений - скрипт в /etc/init.d та бінарник краще
класти через Buildroot overlay або на /data; відсутність RTC впливає на
timestamp у логах; конфлікт UART з консоллю/Bluetooth)
