#ifndef __GNUC__
#error I want gcc!
#endif

/* 
 * Don't include string.h - we'll define our own non-inline versions
 * that GCC can call when it generates calls to memcpy/memset for
 * large struct assignments.
 */

/*
 * GCC may generate calls to memcpy/memset for large struct assignments
 * even with -fno-builtin. These must match GCC's expected signatures.
 *
 * GCC expects:
 *   void *memcpy(void *dest, const void *src, size_t n)
 *   void *memset(void *s, int c, size_t n)
 *
 * where size_t is unsigned long on 64-bit.
 */

typedef unsigned long size_t;

void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	while (n--)
		*d++ = *s++;
	return dest;
}

void *memset(void *s, int c, size_t n)
{
	unsigned char *xs = s;
	while (n--)
		*xs++ = (unsigned char)c;
	return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;
	if (d < s) {
		while (n--)
			*d++ = *s++;
	} else {
		d += n;
		s += n;
		while (n--)
			*--d = *--s;
	}
	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1;
	const unsigned char *p2 = s2;
	while (n--) {
		if (*p1 != *p2)
			return *p1 - *p2;
		p1++;
		p2++;
	}
	return 0;
}
