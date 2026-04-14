CROSS_COMPILE = /home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-x86_64/bin/riscv64-unknown-linux-gnu-
CC = $(CROSS_COMPILE)gcc

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
	rm -f event_bus test_subscriber test_publisher *.o
