/*
 * system.h - 64-bit version
 * 
 * System macros for x86_64
 */

/*
 * move_to_user_mode - Switch from kernel mode to user mode
 *
 * In 64-bit mode, we need to set up the stack for iretq:
 * - SS (user data segment selector with RPL 3)
 * - RSP (user stack pointer)
 * - RFLAGS
 * - CS (user code segment selector with RPL 3)
 * - RIP (return address)
 *
 * Segment selectors (assuming GDT layout from head.nasm):
 * 0x00 - null
 * 0x08 - kernel code (64-bit)
 * 0x10 - kernel data
 * 0x18 - user code (64-bit) with RPL 3 = 0x1b
 * 0x20 - user data with RPL 3 = 0x23
 */
#define move_to_user_mode() \
__asm__ volatile ( \
	"movq %%rsp, %%rax\n\t" \
	"pushq $0x23\n\t"       /* SS = user data selector */ \
	"pushq %%rax\n\t"       /* RSP */ \
	"pushfq\n\t"            /* RFLAGS */ \
	"orq $0x200, (%%rsp)\n\t" /* Set IF in RFLAGS on stack */ \
	"pushq $0x1b\n\t"       /* CS = user code selector */ \
	"leaq 1f(%%rip), %%rax\n\t" /* Get address of label 1 */ \
	"pushq %%rax\n\t"       /* RIP = label 1 */ \
	"iretq\n"               /* switch to user mode */ \
	"1:\tmovl $0x23, %%eax\n\t" \
	"movw %%ax, %%ds\n\t" \
	"movw %%ax, %%es\n\t" \
	"movw %%ax, %%fs\n\t" \
	"movw %%ax, %%gs" \
	:::"rax", "memory")

#define sti() __asm__ volatile ("sti":::"memory")
#define cli() __asm__ volatile ("cli":::"memory")
#define nop() __asm__ volatile ("nop"::)

#define iret() __asm__ volatile ("iretq"::)

/*
 * IDT gate descriptor in 64-bit mode is 16 bytes.
 * The idt_entry struct is defined in linux/head.h
 */
extern struct idt_entry idt[];

#define _set_gate(n, type, dpl, addr) \
do { \
	unsigned long __addr = (unsigned long)(addr); \
	idt[n].offset_low = __addr & 0xFFFF; \
	idt[n].selector = 0x08; /* kernel code segment */ \
	idt[n].ist = 0; \
	idt[n].type_attr = 0x80 | ((dpl) << 5) | (type); \
	idt[n].offset_mid = (__addr >> 16) & 0xFFFF; \
	idt[n].offset_high = __addr >> 32; \
	idt[n].reserved = 0; \
} while(0)

#define set_intr_gate(n, addr) \
	_set_gate(n, 14, 0, addr)

#define set_trap_gate(n, addr) \
	_set_gate(n, 15, 0, addr)

#define set_system_gate(n, addr) \
	_set_gate(n, 15, 3, addr)

/*
 * TSS descriptor in 64-bit mode is 16 bytes (spans two GDT entries)
 * LDT descriptor is also 16 bytes in 64-bit mode
 *
 * 64-bit system descriptor format:
 * Bytes 0-1:  Limit[15:0]
 * Bytes 2-3:  Base[15:0]
 * Byte 4:     Base[23:16]
 * Byte 5:     Type[3:0], 0, DPL[1:0], P
 * Byte 6:     Limit[19:16], AVL, 0, 0, G
 * Byte 7:     Base[31:24]
 * Bytes 8-11: Base[63:32]
 * Bytes 12-15: Reserved (must be 0)
 */
#define _set_tssldt_desc(n, addr, limit, type) \
do { \
	unsigned long __addr = (unsigned long)(addr); \
	unsigned char *__p = (unsigned char *)(n); \
	/* Limit [15:0] */ \
	__p[0] = (limit) & 0xFF; \
	__p[1] = ((limit) >> 8) & 0xFF; \
	/* Base [15:0] */ \
	__p[2] = __addr & 0xFF; \
	__p[3] = (__addr >> 8) & 0xFF; \
	/* Base [23:16] */ \
	__p[4] = (__addr >> 16) & 0xFF; \
	/* Type + DPL + Present (0x80 = present, type is in low nibble) */ \
	__p[5] = (type) | 0x80; \
	/* Limit [19:16] + Granularity (G=0 for byte granularity) */ \
	__p[6] = ((limit) >> 16) & 0x0F; \
	/* Base [31:24] */ \
	__p[7] = (__addr >> 24) & 0xFF; \
	/* Base [63:32] */ \
	__p[8] = (__addr >> 32) & 0xFF; \
	__p[9] = (__addr >> 40) & 0xFF; \
	__p[10] = (__addr >> 48) & 0xFF; \
	__p[11] = (__addr >> 56) & 0xFF; \
	/* Reserved */ \
	__p[12] = 0; \
	__p[13] = 0; \
	__p[14] = 0; \
	__p[15] = 0; \
} while(0)

/* Type 0x09 = Available 64-bit TSS, Type 0x02 = LDT */
#define set_tss_desc(n, addr) _set_tssldt_desc(((char *)(n)), addr, 103, 0x09)
#define set_ldt_desc(n, addr) _set_tssldt_desc(((char *)(n)), addr, 23, 0x02)

/* Segment descriptor helper (unused in pure 64-bit but kept for compatibility) */
#define _set_seg_desc(gate_addr, type, dpl, base, limit) \
do { \
	unsigned long *__g = (unsigned long *)(gate_addr); \
	*__g = ((base) & 0xff000000UL) | \
		(((base) & 0x00ff0000UL) >> 16) | \
		((limit) & 0xf0000UL) | \
		((unsigned long)(dpl) << 13) | \
		(0x00408000UL) | \
		((unsigned long)(type) << 8); \
	*(__g + 1) = (((base) & 0x0000ffffUL) << 16) | \
		((limit) & 0x0ffffUL); \
} while(0)
