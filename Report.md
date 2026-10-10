# TASK 05. UART ECHO

Автор: arersen
Дата: 2026-10-10

## 1. Середовище

| | |
|---|---|
| HOST | Ubuntu (VirtualBox), gcc, make, buildroot toolchain |
| TARGET | Raspberry Pi 5 Model B Rev 1.1 |
| ОС на TARGET | Buildroot (BusyBox init), glibc 2.44, aarch64 |
| UART | `/dev/ttyAMA0`, 115200 8N1 |

## 2. Налаштування UART на TARGET

На Pi 5 пристрій `ttyAMA10` це окремий відлагоджувальний UART, який не виведений на 40-піновий роз'єм. Щоб UART з'явився на GPIO14/GPIO15, у `config.txt` (boot-розділ, `mmcblk0p1`) додано overlay:

```
[all]
dtoverlay=uart0-pi5
```

Файл `cmdline.txt` не змінювався: консоль у ньому тільки `console=tty1`, тому ядро не займає послідовний порт. Рядок getty в `/etc/inittab` не чіпався.

Після перезавантаження з'явився пристрій `/dev/ttyAMA0`:

```
# ls /dev | grep AMA
ttyAMA0
ttyAMA10
#
```

Права доступу (робота під root):

```
# ls -l /dev/ttyAMA0
crw-------    1 root     root      204,  64 Jan  1 00:00 /dev/ttyAMA0
#
```

### Loopback-тест

Підключення: pin 8 (TXD, GPIO14) з'єднано перемичкою з pin 10 (RXD, GPIO15).

```
stty -F /dev/ttyAMA0 115200 raw -echo
cat /dev/ttyAMA0 &
echo test > /dev/ttyAMA0
test
```


## 3. Застосунок mycom

Файли: `mycom.c`, `Makefile`, `.clang-format`, `init/S99mycom`.

Запуск: `./mycom <device> [baudrate]` (за замовчуванням 115200 8N1).

Що робить програма:

- відкриває порт у raw-режимі через `termios.h` (8 біт, без парності, 1 стоп-біт, без flow control);
- збирає символи в рядок до `\r`, `\n` або `\r\n`;
- відповідає тим самим рядком із зміненим регістром (`a`→`A`, `B`→`b`) і закінченням `\r\n`;
- записує кожен отриманий рядок у лог із міткою часу `[YYYY-MM-DD HH:MM:SS]`; ім'я лога це час запуску (`YYYYMMDD_HHMMSS.log`);
- якщо порт недоступний (немає пристрою, немає прав, помилка налаштування), виводить повідомлення на екран, пише його в лог і завершується з кодом 1;
- обробляє `SIGINT` і `SIGTERM`: виходить із циклу, відновлює налаштування порту, закриває порт і лог.

### Makefile

Цілі: `all`, `clean`, `deploy`, `format`. Змінна `CROSS_COMPILE` задає префікс тулчейна.

```
make CROSS_COMPILE=/home/jambyl/ajax_project/buildroot/output/host/bin/aarch64-buildroot-linux-gnu-
make deploy TARGET_HOST=root@192.168.0.183 CROSS_COMPILE=...
```

```
file mycom
mycom: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, for GNU/Linux 3.7.0, not stripped
```

### Перевірка на HOST (без заліза)

Через пару віртуальних портів (`socat` або pty):

| Вхід | Відповідь |
|---|---|
| `Hello World 123\r\n` | `hELLO wORLD 123\r\n` |
| `Hello\r` | `hELLO\r\n` |
| `abc\n` | `ABC\r\n` |

- Лог містить рядки з мітками часу та повідомлення `Started` / `Stopped`.
- `SIGINT`: порт і лог закриваються, код виходу 0.
- Неіснуючий пристрій (`./mycom /dev/ttyNOPE`): повідомлення на екрані та в лозі, код виходу 1.

### Перевірка на TARGET

Запуск на платі:

```
Started on /dev/ttyAMA0 at 115200 8N1
```


![обмін](docs/img/video.mp4)


```
# cat 19700101_004127.log
[1970-01-01 00:41:27] Started on /dev/ttyAMA0 at 115200 8N1
[1970-01-01 00:41:59] hello World!
[1970-01-01 00:42:05] he
[1970-01-01 00:43:08] abcdef
[1970-01-01 00:43:12] hello World
[1970-01-01 00:43:23] hekHello World!
[1970-01-01 00:44:40] hello World
[1970-01-01 00:44:44] Hello World!
[1970-01-01 00:44:49] abcdef
[1970-01-01 00:44:53] abBdcdDefgJ
[1970-01-01 00:44:56] hello WORLD
#
```

## 4. Автозапуск

Скрипт `init/S99mycom` для BusyBox init (`start`/`stop`/`restart`, запуск через `start-stop-daemon`).



## 5. Проблеми та висновки

1. **Rootfs 107 МБ заповнений на 100%.** `scp` створив `mycom` розміром 0 байт, лог не створювався, програма тихо завершувалась. Вирішення: окремий розділ `mmcblk0p3` (~29 ГБ), монтування в `/data` і `mount --bind /data/root /root`.
2. **Монтування зникає після перезавантаження.** Потрібно повторювати вручну, у `/etc/fstab` записати нема куди. Постійне рішення: збільшити rootfs або додати скрипт через Buildroot overlay.
3. **Немає актуального часу.** Дата після ребуту 1970 рік, тому мітки часу в логах і в іменах лог-файлів хибні. Потрібно виставляти `date -s` або використати RTC (`rtc0`) із батарейкою.
4. **На Pi 5 UART на роз'ємі не ввімкнений за замовчуванням.** Є лише `ttyAMA10` (відлагоджувальний). Потрібен `dtoverlay=uart0-pi5`.
5. **Кілька процесів читали один порт** (`cat` і два `mycom`). Кожен байт отримує лише один процес, тому літери губилися, а Enter доводилося натискати двічі. Вирішення: перед запуском перевіряти `ps | grep -E 'mycom|cat'`.
6. **PuTTY шле лише `\r` при Enter.** Спочатку програма чекала `\r\n`, тому рядок не завершувався. Виправлено: рядок закінчується на `\r`, `\n` або `\r\n`.
7. **SSH-сесія PuTTY не підключена до UART.** Вона дає лише шел плати. Для обміну з HOST потрібен USB-TTL адаптер (3,3 В), підключений хрест-навхрест (RX↔TX) і GND, PuTTY у режимі Serial.
8. **Обмеження BusyBox:** немає `file`, `tar -z`, а `wget` без HTTPS.

## 6. Коміти

TODO: вивід `git log --oneline`.
