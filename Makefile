#
# Makefile for linux 0.01 - 64-bit version
#

# Cross-compiler for x86_64
# On macOS: brew install x86_64-elf-gcc
# On Linux: apt install gcc
CROSS_COMPILE ?= x86_64-elf-

# QEMU emulator
QEMU = qemu-system-x86_64

# Assembler
NASM    = nasm
NASM16  = $(NASM) -f bin
NASM64  = $(NASM) -f elf64

# Toolchain
AS      = $(NASM64)
LD      = $(CROSS_COMPILE)ld
CC      = $(CROSS_COMPILE)gcc
AR      = $(CROSS_COMPILE)ar
OBJCOPY = $(CROSS_COMPILE)objcopy

# Get absolute path to project root for includes
ROOT_DIR := $(shell pwd)

# Compiler flags for freestanding 64-bit kernel
CFLAGS  = -Wall -O2 -ffreestanding -fno-stack-protector -fno-builtin \
          -fno-pie -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
          -nostdinc -I$(ROOT_DIR)/include -m64

LDFLAGS = -nostdlib -z max-page-size=0x1000

CPP     = $(CC) -E -nostdinc -Iinclude

# Export for sub-makes
export CROSS_COMPILE CC LD AR CFLAGS NASM NASM64

ARCHIVES = kernel/kernel.o mm/mm.o fs/fs.o
LIBS     = lib/lib.a

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

all: Image

Image: boot/boot.bin tools/system tools/build
	tools/build boot/boot.bin tools/system > Image
	@# Pad Image to 1.44MB (required for QEMU floppy emulation to work correctly with head 1)
	@dd if=/dev/zero of=Image bs=1 count=1 seek=1474559 conv=notrunc 2>/dev/null
	@echo "Built 64-bit kernel image: Image"

tools/build: tools/build.c
	cc -Wall -O2 -o tools/build tools/build.c

# Boot sector stays 16-bit (real mode entry)
boot/boot.bin: boot/boot_s.nasm tools/system
	@SYSSIZE=$$(( (`stat -f%z tools/system 2>/dev/null || stat -c%s tools/system` + 15) / 16 )); \
	$(NASM16) -DSYSSIZE=$$SYSSIZE -o boot/boot.bin boot/boot_s.nasm

# Head contains both 32-bit (entry from boot) and 64-bit code
# It needs special handling
boot/head.o: boot/head.nasm
	$(NASM) -f elf64 -o boot/head.o boot/head.nasm

tools/system.elf: boot/head.o init/main.o $(ARCHIVES) $(LIBS)
	$(LD) $(LDFLAGS) -T kernel.ld -o tools/system.elf \
		boot/head.o init/main.o $(ARCHIVES) $(LIBS)

tools/system: tools/system.elf
	$(OBJCOPY) -O binary tools/system.elf tools/system

kernel/kernel.o:
	$(MAKE) -C kernel

mm/mm.o:
	$(MAKE) -C mm

fs/fs.o:
	$(MAKE) -C fs

lib/lib.a:
	$(MAKE) -C lib

clean:
	rm -f Image System.map boot/boot.bin boot/tmp_boot.nasm core
	rm -f init/*.o boot/*.o tools/system tools/system.elf tools/build
	$(MAKE) -C mm clean
	$(MAKE) -C fs clean
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean

# Run the kernel in QEMU (Ctrl-A X to exit)
run: Image
	@echo "Starting Linux 0.01 64-bit... (Press Ctrl-A then X to exit)"
	@echo ""
	$(QEMU) -drive file=Image,format=raw,if=floppy -nographic

# Run with graphical display (for VGA output)
run-graphic: Image
	$(QEMU) -drive file=Image,format=raw,if=floppy

# Run with GDB debugging enabled (connect with: gdb -ex "target remote :1234")
debug: Image
	$(QEMU) -drive file=Image,format=raw,if=floppy -nographic -s -S

# Build and boot (alias for run)
boot: run

.PHONY: all clean kernel/kernel.o mm/mm.o fs/fs.o lib/lib.a run run-serial run-graphic debug boot
