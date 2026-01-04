#
# Makefile for lib
#

# All variables are exported from the parent Makefile

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

OBJS = ctype.o _exit.o open.o close.o errno.o write.o dup.o setsid.o \
       execve.o wait.o string.o

lib.a: $(OBJS)
	$(AR) rcs lib.a $(OBJS)

clean:
	rm -f core *.o *.a tmp_make

.PHONY: clean
