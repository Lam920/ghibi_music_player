# From SDK host-tools path
CROSS_COMPILE = /home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-x86_64/bin/riscv64-unknown-linux-gnu-
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -O2 -std=c99 -D_POSIX_C_SOURCE=200112L -static
LDFLAGS = -lm -static

play_bin:
	$(CC) $(CFLAGS) -o play_bin play_bin.c $(LDFLAGS)

clean:
	rm -f play_bin
