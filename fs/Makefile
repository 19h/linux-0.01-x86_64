#
# Makefile for fs
#

# All variables are exported from the parent Makefile

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

OBJS = open.o read_write.o inode.o file_table.o buffer.o super.o \
       block_dev.o char_dev.o file_dev.o stat.o exec.o pipe.o namei.o \
       bitmap.o fcntl.o ioctl.o tty_ioctl.o truncate.o

all: fs.o

fs.o: $(OBJS)
	$(LD) -r -o fs.o $(OBJS)

clean:
	rm -f core *.o *.a tmp_make

.PHONY: clean
