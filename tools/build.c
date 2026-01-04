/*
 * build.c - create a boot image from boot sector and system kernel
 *
 * Updated to work with modern NASM raw binary boot sector and ELF system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

void die(const char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

void usage(void)
{
	die("Usage: build boot system [> image]");
}

int main(int argc, char **argv)
{
	int id, c;
	long i;
	char buf[1024];
	struct stat st;

	if (argc != 3)
		usage();

	/* Read boot sector - should be exactly 512 bytes raw binary */
	if ((id = open(argv[1], O_RDONLY, 0)) < 0)
		die("Unable to open 'boot'");
	
	if (fstat(id, &st) < 0)
		die("Unable to stat 'boot'");
	
	if (st.st_size != 512)
		fprintf(stderr, "Warning: boot sector is %ld bytes (expected 512)\n", (long)st.st_size);
	
	/* Read and write boot sector */
	memset(buf, 0, sizeof(buf));
	i = read(id, buf, 512);
	if (i <= 0)
		die("Unable to read 'boot'");
	fprintf(stderr, "Boot sector %ld bytes.\n", i);
	
	/* Verify boot signature */
	if ((unsigned char)buf[510] != 0x55 || (unsigned char)buf[511] != 0xAA)
		fprintf(stderr, "Warning: boot signature not found\n");
	
	if (write(1, buf, 512) != 512)
		die("Write call failed");
	close(id);

	/* Read and write system - we expect a raw binary */
	if ((id = open(argv[2], O_RDONLY, 0)) < 0)
		die("Unable to open 'system'");
	
	/* Copy system to output */
	i = 0;
	while ((c = read(id, buf, sizeof(buf))) > 0) {
		if (write(1, buf, c) != c)
			die("Write call failed");
		i += c;
	}
	close(id);
	fprintf(stderr, "System %ld bytes.\n", i);
	
	return 0;
}
