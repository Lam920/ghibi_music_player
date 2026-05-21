CROSS_COMPILE = /home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
CC = $(CROSS_COMPILE)gcc

export PATH=/home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/host-tools/gcc/riscv64-linux-musl-x86_64/bin/:$PATH

# Size optimization flags:
# -Os: optimize for size
# -flto: link-time optimization
# -ffunction-sections -fdata-sections: separate functions/data for better dead code elimination
# -fno-unwind-tables: remove exception handling tables (saves ~1-2KB)
# -fno-asynchronous-unwind-tables: remove async exception tables
CFLAGS = -Wall -Os -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L -flto -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables

# Dynamic linking (much smaller ~11KB) - requires shared libs on target
# Use correct musl dynamic linker for Milk-V Duo
LDFLAGS = -lm -s -flto -Wl,--gc-sections -Wl,--dynamic-linker=/lib/ld-musl-riscv64v0p7_xthead.so.1
# Static linking (larger ~409KB but standalone) - best for embedded devices
#LDFLAGS = -static -lm -s -flto -Wl,--gc-sections

COMMON_SRC = bus_client.c

.PHONY: all clean tests

all: event_bus

tests: test_subscriber test_publisher

event_bus: event_bus.c $(COMMON_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_subscriber: test_subscriber.c $(COMMON_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_publisher: test_publisher.c $(COMMON_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	/usr/bin/rm -f event_bus test_publisher *.o

#install/soc_cv1800b_milkv_duo_sd/
#ln -s ld-musl-riscv64v0p7_xthead.so.1 /lib/ld-musl-riscv64.so.1
