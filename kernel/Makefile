#
# Makefile for the kernel
#

# These are exported from the parent Makefile
NASM64  = $(NASM) -f elf64

OBJS = sched.o system_call.o traps.o asm.o fork.o \
       panic.o printk.o vsprintf.o tty_io.o console.o \
       keyboard.o rs_io.o hd.o sys.o exit.o serial.o mktime.o \
       switch.o

all: kernel.o

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

# Assembly files
system_call.o: system_call.nasm
	$(NASM64) -o system_call.o system_call.nasm

asm.o: asm.nasm
	$(NASM64) -o asm.o asm.nasm

keyboard.o: keyboard.nasm
	$(NASM64) -o keyboard.o keyboard.nasm

rs_io.o: rs_io.nasm
	$(NASM64) -o rs_io.o rs_io.nasm

switch.o: switch.nasm
	$(NASM64) -o switch.o switch.nasm

kernel.o: $(OBJS)
	$(LD) -r -o kernel.o $(OBJS)

clean:
	rm -f core *.o *.a tmp_make
	for i in *.c; do rm -f `basename $$i .c`.s; done

.PHONY: clean
