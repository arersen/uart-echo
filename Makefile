# Usage:
#   make                              build (native on the Pi, or with CROSS_COMPILE)
#   make install-local                install on THIS machine (run on the Pi)
#   make install TARGET_HOST=user@ip  install on TARGET over ssh/scp
#   make deploy  TARGET_HOST=user@ip  copy only the binary
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2
APP := mycom
UNIT := mycom.service

TARGET_HOST ?= pi@raspberrypi.local
TARGET_DIR ?= ~
SCP_FLAGS ?=
SSH ?= ssh

BIN_DIR ?= /usr/local/bin
UNIT_DIR ?= /etc/systemd/system

.PHONY: all clean deploy install install-local uninstall format

all: $(APP)

$(APP): mycom.c
	$(CC) $(CFLAGS) -o $@ $<

deploy: $(APP)
	scp $(SCP_FLAGS) $(APP) $(TARGET_HOST):$(TARGET_DIR)/

# Local install (run on the target itself)
install-local: $(APP) $(UNIT)
	-sudo systemctl stop $(UNIT)
	sudo install -D -m 755 $(APP) $(BIN_DIR)/$(APP)
	sudo install -D -m 644 $(UNIT) $(UNIT_DIR)/$(UNIT)
	sudo systemctl daemon-reload
	sudo systemctl enable $(UNIT)

# Remote install: copy to /tmp, then move into place with sudo
install: $(APP) $(UNIT)
	scp $(SCP_FLAGS) $(APP) $(UNIT) $(TARGET_HOST):/tmp/
	$(SSH) -t $(TARGET_HOST) 'sudo systemctl stop $(UNIT) 2>/dev/null; \
		sudo install -D -m 755 /tmp/$(APP) $(BIN_DIR)/$(APP) && \
		sudo install -D -m 644 /tmp/$(UNIT) $(UNIT_DIR)/$(UNIT) && \
		sudo systemctl daemon-reload && \
		sudo systemctl enable $(UNIT)'

uninstall:
	-sudo systemctl disable --now $(UNIT)
	sudo rm -f $(BIN_DIR)/$(APP) $(UNIT_DIR)/$(UNIT)
	sudo systemctl daemon-reload

format:
	clang-format -i mycom.c

clean:
	rm -f $(APP) *.log
