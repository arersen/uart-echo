# Report TASK06: Автозапуск UART ECHO-застосунку через systemd

**1. Розробка systemd unit-файлу (.service)**

1.а) Сервіс налаштовано на запуск програми з правильним UART-інтерфейсом.
```sh
awlik@jambyl:~/uart-echo $ cat mycom.service
[Unit]
Description=UART ECHO service (mycom)
After=sysinit.target dev-ttyAMA0.device
Wants=dev-ttyAMA0.device
StartLimitIntervalSec=60
StartLimitBurst=5

[Service]
Type=simple
ExecStart=/usr/local/bin/mycom /dev/ttyAMA0
LogsDirectory=mycom
WorkingDirectory=/var/log/mycom
Restart=on-failure
RestartSec=2
KillSignal=SIGTERM
TimeoutStopSec=5

[Install]
WantedBy=multi-user.target
awlik@jambyl:~/uart-echo $
```

1.б) Сервіс коректно перезапускається при падінні програми завдяки налаштованій політиці.
Використано директиву `Restart=on-failure` у секції [Service].

1.в) Сервіс запускається після того, як система готова до роботи з пристроями.
After=sysinit.target dev-ttyAMA0.device

1.г) При зупинці сервісу програма коректно завершується (systemd надсилає SIGTERM).
Використовується стандартна поведінка systemd

---

**2. Доопрацювання Makefile**

2.а) Додано ціль `install`, яка копіює бінарник, unit-файл, оновлює конфігурацію systemd та вмикає сервіс.
```
# Remote install: copy to /tmp, then move into place with sudo
install: $(APP) $(UNIT)
        scp $(SCP_FLAGS) $(APP) $(UNIT) $(TARGET_HOST):/tmp/
        $(SSH) -t $(TARGET_HOST) 'sudo systemctl stop $(UNIT) 2>/dev/null; \
                sudo install -D -m 755 /tmp/$(APP) $(BIN_DIR)/$(APP) && \
                sudo install -D -m 644 /tmp/$(UNIT) $(UNIT_DIR)/$(UNIT) && \
                sudo systemctl daemon-reload && \
                sudo systemctl enable $(UNIT)'
```

2.б) Перевірка роботи: сервіс автоматично запускається, працює у фоновому режимі, приймає дані та коректно реагує на команди керування.

**Вивід з терміналу (Статус сервісу після make install та перезавантаження):**

Швидка перевірка програми без systemd
```bash
awlik@jambyl:~/uart-echo $ make
gcc -Wall -Wextra -O2 -o mycom mycom.c
awlik@jambyl:~/uart-echo $ ./mycom /dev/ttyAMA0
Started on /dev/ttyAMA0 at 115200 8N1
^CStopped
```

Встановлення сервісу
```bash
awlik@jambyl:~/uart-echo $ make install-local
sudo systemctl stop mycom.service
sudo install -D -m 755 mycom /usr/local/bin/mycom
sudo install -D -m 644 mycom.service /etc/systemd/system/mycom.service
sudo systemctl daemon-reload
sudo systemctl enable mycom.service
```

Запуск і перевірка echo
```bash
awlik@jambyl:~/uart-echo $ sudo systemctl start mycom
awlik@jambyl:~/uart-echo $ systemctl is-enabled mycom
enabled
awlik@jambyl:~/uart-echo $


awlik@jambyl:~/uart-echo $ printf 'Hello\r\n' > /dev/ttyAMA0
awlik@jambyl:~/uart-echo $ jobs
awlik@jambyl:~/uart-echo $ sudo systemctl stop mycom
awlik@jambyl:~/uart-echo $ sudo tail -n 6 /var/log/mycom/*.log
==> /var/log/mycom/20261022_154219.log <==
[2026-10-22 15:42:19] Started on /dev/ttyAMA0 at 115200 8N1
[2026-10-22 16:04:35] Stopped

==> /var/log/mycom/20261022_160717.log <==
[2026-10-22 16:10:11] hELLO
[2026-10-22 16:10:11] Hello
[2026-10-22 16:10:11] hELLO
[2026-10-22 16:10:11] Hello
[2026-10-22 16:10:11] hELLO
[2026-10-22 16:10:11] Stopped
awlik@jambyl:~/uart-echo $
```

Перезапуск при падінні
```bash
awlik@jambyl:~/uart-echo $ sudo systemctl start mycom
awlik@jambyl:~/uart-echo $ sudo pkill -9 mycom
awlik@jambyl:~/uart-echo $ systemctl status mycom
● mycom.service - UART ECHO service (mycom)
     Loaded: loaded (/etc/systemd/system/mycom.service; enabled; preset: enabled)
     Active: active (running) since Thu 2026-10-22 16:11:44 EEST; 11s ago
 Invocation: ff356f14c83440b5bd93ef82542af8bf
   Main PID: 1966 (mycom)
      Tasks: 1 (limit: 19413)
        CPU: 9ms
     CGroup: /system.slice/mycom.service
             └─1966 /usr/local/bin/mycom /dev/ttyAMA0

Oct 22 16:11:42 jambyl systemd[1]: mycom.service: Main process exited, code=killed, status=9/KILL
Oct 22 16:11:42 jambyl systemd[1]: mycom.service: Failed with result 'signal'.
Oct 22 16:11:44 jambyl systemd[1]: mycom.service: Scheduled restart job, restart counter is at 1.
Oct 22 16:11:44 jambyl systemd[1]: Started mycom.service - UART ECHO service (mycom).
awlik@jambyl:~/uart-echo $ systemctl show mycom -p NRestarts
NRestarts=1
awlik@jambyl:~/uart-echo $
```
```bash
awlik@jambyl:~/uart-echo $ sudo systemctl restart mycom
awlik@jambyl:~/uart-echo $ systemctl status mycom
● mycom.service - UART ECHO service (mycom)
     Loaded: loaded (/etc/systemd/system/mycom.service; enabled; preset: enabled)
     Active: active (running) since Thu 2026-10-22 16:13:15 EEST; 10s ago
 Invocation: 5021d702e9ec403eab10c75a3a634ed6
   Main PID: 1979 (mycom)
      Tasks: 1 (limit: 19413)
        CPU: 9ms
     CGroup: /system.slice/mycom.service
             └─1979 /usr/local/bin/mycom /dev/ttyAMA0

Oct 22 16:13:15 jambyl systemd[1]: Started mycom.service - UART ECHO service (mycom).
awlik@jambyl:~/uart-echo $
```

Автозапуск після перезавантаження
```bash
awlik@jambyl:~/uart-echo $ sudo reboot
*rebooting*

awlik@jambyl:~ $ systemctl status mycom
● mycom.service - UART ECHO service (mycom)
     Loaded: loaded (/etc/systemd/system/mycom.service; enabled; preset: enabled)
     Active: active (running) since Thu 2026-10-22 16:16:08 EEST; 1min 44s ago
 Invocation: fb2eda3eaa0b4b30b09ed87b5206da93
   Main PID: 639 (mycom)
      Tasks: 1 (limit: 19413)
        CPU: 8ms
     CGroup: /system.slice/mycom.service
             └─639 /usr/local/bin/mycom /dev/ttyAMA0

Oct 22 16:16:08 jambyl systemd[1]: Started mycom.service - UART ECHO service (mycom).
awlik@jambyl:~ $ journalctl -u mycom -b --no-pager | tail
Oct 22 16:16:08 jambyl systemd[1]: Started mycom.service - UART ECHO service (mycom).
awlik@jambyl:~ $
```
