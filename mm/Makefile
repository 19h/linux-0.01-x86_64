#
# Makefile for mm
#

# These are exported from the parent Makefile
NASM64  = $(NASM) -f elf64

OBJS = memory.o page.o

all: mm.o

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

page.o: page.nasm
	$(NASM64) -o page.o page.nasm

mm.o: $(OBJS)
	$(LD) -r -o mm.o $(OBJS)

clean:
	rm -f core *.o *.a tmp_make

.PHONY: all clean
