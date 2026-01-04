#ifndef _HEAD_H
#define _HEAD_H

/*
 * GDT descriptor entry (8 bytes)
 * In 64-bit mode, standard segment descriptors are still 8 bytes.
 * TSS/LDT descriptors are 16 bytes (but we handle those specially).
 */
typedef struct desc_struct {
	unsigned int a, b;
} desc_table[256];

/*
 * IDT gate entry (16 bytes in 64-bit mode)
 */
struct idt_entry {
	unsigned short offset_low;
	unsigned short selector;
	unsigned char ist;
	unsigned char type_attr;
	unsigned short offset_mid;
	unsigned int offset_high;
	unsigned int reserved;
} __attribute__((packed));

extern unsigned long pg_dir[1024];
extern desc_table gdt;
extern struct idt_entry idt[256];

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
