# Usage:
#   make                                  native build
#   make CROSS_COMPILE=/path/to/bin/aarch64-buildroot-linux-gnu-
#   make deploy TARGET_HOST=root@192.168.0.183 CROSS_COMPILE=...
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2
APP := mycom

TARGET_HOST ?= root@192.168.0.183
TARGET_DIR ?= /root
SCP_FLAGS ?=

.PHONY: all clean deploy format

all: $(APP)

$(APP): mycom.c
	$(CC) $(CFLAGS) -o $@ $<

deploy: $(APP)
	scp $(SCP_FLAGS) $(APP) $(TARGET_HOST):$(TARGET_DIR)/

format:
	clang-format -i mycom.c

clean:
	rm -f $(APP) *.log
