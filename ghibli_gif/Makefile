# From SDK host-tools path
CROSS_COMPILE = /home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-x86_64/bin/riscv64-unknown-linux-gnu-
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Os -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L -flto -I../IPC
LDFLAGS = -static -lm -s -flto

# IPC bus client
BUS_CLIENT = ../IPC/bus_client.c

all: play_gif gifctl

play_gif: play_gif.c $(BUS_CLIENT)
	$(CC) $(CFLAGS) -o play_gif play_gif.c $(BUS_CLIENT) $(LDFLAGS)

gifctl:
	$(CC) -Os -s -static -o gifctl gifctl.c

clean:
	rm -f play_gif gifctl
