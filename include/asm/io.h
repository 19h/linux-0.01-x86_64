/*
 * I/O port access macros for x86_64
 */

#define outb(value,port) \
__asm__ volatile ("outb %%al,%%dx"::"a" ((unsigned char)(value)),"d" ((unsigned short)(port)))

#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" ((unsigned short)(port))); \
_v; \
})

#define outw(value,port) \
__asm__ volatile ("outw %%ax,%%dx"::"a" ((unsigned short)(value)),"d" ((unsigned short)(port)))

#define inw(port) ({ \
unsigned short _v; \
__asm__ volatile ("inw %%dx,%%ax":"=a" (_v):"d" ((unsigned short)(port))); \
_v; \
})

#define outl(value,port) \
__asm__ volatile ("outl %%eax,%%dx"::"a" ((unsigned int)(value)),"d" ((unsigned short)(port)))

#define inl(port) ({ \
unsigned int _v; \
__asm__ volatile ("inl %%dx,%%eax":"=a" (_v):"d" ((unsigned short)(port))); \
_v; \
})

/* Versions with pause for slow devices */
#define outb_p(value,port) \
__asm__ volatile ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" ((unsigned char)(value)),"d" ((unsigned short)(port)))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" ((unsigned short)(port))); \
_v; \
})
