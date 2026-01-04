#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif

extern char * strerror(int errno);

/*
 * String functions for x86_64
 * 
 * The original Linux 0.01 used heavily optimized i386 inline assembly.
 * For the 64-bit port, we use simpler C implementations that the compiler
 * can optimize. These can be replaced with optimized assembly later.
 */

static inline char * strcpy(char * dest, const char *src)
{
	char *d = dest;
	while ((*d++ = *src++) != '\0')
		;
	return dest;
}

static inline char * strncpy(char * dest, const char *src, int count)
{
	char *d = dest;
	while (count > 0 && (*d++ = *src++) != '\0')
		count--;
	while (count-- > 0)
		*d++ = '\0';
	return dest;
}

static inline char * strcat(char * dest, const char * src)
{
	char *d = dest;
	while (*d)
		d++;
	while ((*d++ = *src++) != '\0')
		;
	return dest;
}

static inline char * strncat(char * dest, const char * src, int count)
{
	char *d = dest;
	while (*d)
		d++;
	while (count-- > 0 && (*d = *src++) != '\0')
		d++;
	*d = '\0';
	return dest;
}

static inline int strcmp(const char * cs, const char * ct)
{
	while (*cs && *cs == *ct) {
		cs++;
		ct++;
	}
	return (unsigned char)*cs - (unsigned char)*ct;
}

static inline int strncmp(const char * cs, const char * ct, int count)
{
	while (count > 0 && *cs && *cs == *ct) {
		cs++;
		ct++;
		count--;
	}
	if (count == 0)
		return 0;
	return (unsigned char)*cs - (unsigned char)*ct;
}

static inline char * strchr(const char * s, char c)
{
	while (*s) {
		if (*s == c)
			return (char *)s;
		s++;
	}
	return c ? NULL : (char *)s;
}

static inline char * strrchr(const char * s, char c)
{
	const char *last = NULL;
	while (*s) {
		if (*s == c)
			last = s;
		s++;
	}
	return c ? (char *)last : (char *)s;
}

static inline int strspn(const char * cs, const char * ct)
{
	const char *s = cs;
	const char *p;
	while (*s) {
		for (p = ct; *p; p++)
			if (*p == *s)
				break;
		if (!*p)
			break;
		s++;
	}
	return s - cs;
}

static inline int strcspn(const char * cs, const char * ct)
{
	const char *s = cs;
	const char *p;
	while (*s) {
		for (p = ct; *p; p++)
			if (*p == *s)
				return s - cs;
		s++;
	}
	return s - cs;
}

static inline char * strpbrk(const char * cs, const char * ct)
{
	const char *p;
	while (*cs) {
		for (p = ct; *p; p++)
			if (*p == *cs)
				return (char *)cs;
		cs++;
	}
	return NULL;
}

static inline char * strstr(const char * cs, const char * ct)
{
	const char *s, *p, *q;
	if (!*ct)
		return (char *)cs;
	for (s = cs; *s; s++) {
		p = s;
		q = ct;
		while (*p && *q && *p == *q) {
			p++;
			q++;
		}
		if (!*q)
			return (char *)s;
	}
	return NULL;
}

static inline int strlen(const char * s)
{
	const char *sc = s;
	while (*sc)
		sc++;
	return sc - s;
}

extern char * ___strtok;

static inline char * strtok(char * s, const char * ct)
{
	char *token;
	const char *p;
	
	if (s)
		___strtok = s;
	if (!___strtok)
		return NULL;
	
	/* Skip leading delimiters */
	while (*___strtok) {
		for (p = ct; *p; p++)
			if (*p == *___strtok)
				break;
		if (!*p)
			break;
		___strtok++;
	}
	
	if (!*___strtok)
		return NULL;
	
	token = ___strtok;
	
	/* Find end of token */
	while (*___strtok) {
		for (p = ct; *p; p++)
			if (*p == *___strtok) {
				*___strtok++ = '\0';
				return token;
			}
		___strtok++;
	}
	
	___strtok = NULL;
	return token;
}

static inline void * memcpy(void * dest, const void * src, int n)
{
	char *d = dest;
	const char *s = src;
	while (n--)
		*d++ = *s++;
	return dest;
}

static inline void * memmove(void * dest, const void * src, int n)
{
	char *d = dest;
	const char *s = src;
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

static inline int memcmp(const void * cs, const void * ct, int count)
{
	const unsigned char *s1 = cs;
	const unsigned char *s2 = ct;
	while (count--) {
		if (*s1 != *s2)
			return *s1 - *s2;
		s1++;
		s2++;
	}
	return 0;
}

static inline void * memchr(const void * cs, char c, int count)
{
	const unsigned char *s = cs;
	while (count--) {
		if (*s == (unsigned char)c)
			return (void *)s;
		s++;
	}
	return NULL;
}

static inline void * memset(void * s, char c, int count)
{
	char *d = s;
	while (count--)
		*d++ = c;
	return s;
}

#endif
