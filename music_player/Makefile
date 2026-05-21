# From SDK buildroot musl toolchain
CROSS_COMPILE = /home/lambt9/Desktop/duo-buildroot-sdk/host-tools/gcc/riscv64-linux-musl-x86_64/bin/riscv64-unknown-linux-musl-
SYSROOT = /home/lambt9/Desktop/duo-buildroot-sdk/buildroot-2021.05/output/milkv-duo-sd_musl_riscv64/staging
TINYALSA_INCLUDE = /home/lambt9/Desktop/framebuffer/ghibli_music_box/tinyalsa/include
TINYALSA_LIB = /home/lambt9/Desktop/duo-buildroot-sdk/middleware/v2/cv181x/lib_musl_riscv64
IPC_DIR = ../IPC
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Os -std=c99 -D_POSIX_C_SOURCE=200112L --sysroot=$(SYSROOT) -I$(TINYALSA_INCLUDE) -I$(IPC_DIR)
LDFLAGS = -pthread $(TINYALSA_LIB)/libtinyalsa.a -Wl,--dynamic-linker=/lib/ld-musl-riscv64v0p7_xthead.so.1

all: music_player

music_player: music_player.c utils.o $(IPC_DIR)/bus_client.o
	$(CC) $(CFLAGS) -o music_player music_player.c utils.o $(IPC_DIR)/bus_client.o $(LDFLAGS)
	$(CROSS_COMPILE)strip music_player

utils.o: utils.c utils.h music_player.h
	$(CC) $(CFLAGS) -c utils.c

$(IPC_DIR)/bus_client.o:
	$(MAKE) -C $(IPC_DIR) bus_client.o

adc_volume_ctrl:
	$(CC) $(CFLAGS) -o adc_volume_ctrl adc_volume_ctrl.c

clean:
	rm -f music_player utils.o adc_volume_ctrl
