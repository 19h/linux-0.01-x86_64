/*
 * segment.h - 64-bit version
 * 
 * Functions to access user-space memory (via fs segment in original,
 * but in 64-bit mode we typically just access directly or use different
 * mechanisms for user/kernel separation)
 */

static inline unsigned char get_fs_byte(const char * addr)
{
	unsigned char _v;
	__asm__ volatile ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline unsigned short get_fs_word(const unsigned short *addr)
{
	unsigned short _v;
	__asm__ volatile ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;
	__asm__ volatile ("movq %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline void put_fs_byte(char val, char *addr)
{
	__asm__ volatile ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_word(short val, short * addr)
{
	__asm__ volatile ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_long(unsigned long val, unsigned long * addr)
{
	__asm__ volatile ("movq %0,%%fs:%1"::"r" (val),"m" (*addr));
}

/*
 * Get/set the fs segment register value
 * In 64-bit mode, segment registers work differently - fs and gs
 * can be used for thread-local storage but not for memory segmentation
 */
static inline unsigned long get_fs(void)
{
	unsigned long _v;
	__asm__ volatile ("movq %%fs,%0":"=r" (_v));
	return _v;
}

static inline void set_fs(unsigned long val)
{
	__asm__ volatile ("movq %0,%%fs"::"r" (val));
}
