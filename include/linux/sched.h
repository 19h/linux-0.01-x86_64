#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char * str);
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();

/*
 * FPU state for x86_64 using fxsave/fxrstor (512 bytes)
 */
struct i387_struct {
	unsigned short	cwd;
	unsigned short	swd;
	unsigned short	twd;
	unsigned short	fop;
	unsigned long	rip;
	unsigned long	rdp;
	unsigned int	mxcsr;
	unsigned int	mxcsr_mask;
	unsigned int	st_space[32];	/* 8*16 bytes for FP regs */
	unsigned int	xmm_space[64];	/* 16*16 bytes for XMM regs */
	unsigned int	padding[24];
} __attribute__((aligned(16)));

/*
 * 64-bit TSS structure
 * In long mode, the TSS doesn't contain saved registers - it's only used for:
 * - Stack pointers for privilege level changes (RSP0, RSP1, RSP2)
 * - Interrupt Stack Table (IST) pointers
 * - I/O permission bitmap
 */
struct tss_struct {
	unsigned int	reserved0;
	unsigned long	rsp0;		/* Stack pointer for ring 0 */
	unsigned long	rsp1;		/* Stack pointer for ring 1 */
	unsigned long	rsp2;		/* Stack pointer for ring 2 */
	unsigned long	reserved1;
	unsigned long	ist[7];		/* Interrupt Stack Table */
	unsigned long	reserved2;
	unsigned short	reserved3;
	unsigned short	iopb_offset;	/* I/O permission bitmap offset */
} __attribute__((packed));

/*
 * Thread state for context switching
 * This holds the callee-saved registers and stack pointer
 */
struct thread_struct {
	unsigned long	rsp;		/* Kernel stack pointer */
	unsigned long	rip;		/* Instruction pointer (for new threads) */
	unsigned long	rbx;
	unsigned long	rbp;
	unsigned long	r12;
	unsigned long	r13;
	unsigned long	r14;
	unsigned long	r15;
	unsigned long	fs;		/* FS segment base (for TLS) */
	unsigned long	gs;		/* GS segment base */
};

struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	fn_ptr sig_restorer;
	fn_ptr sig_fn[32];
/* various fields */
	int exit_code;
	unsigned long end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* Thread state for context switching */
	struct thread_struct thread;
/* FPU state */
	struct i387_struct i387;
/* Kernel stack - must be at the end for alignment */
	unsigned long kernel_stack[1024];  /* 8KB kernel stack */
};

/*
 * INIT_TASK is used to set up the first task table
 * For 64-bit, we use flat memory model (base=0, no limit)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,NULL,{(fn_ptr) 0,}, \
/* ec,brk... */	0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0133,NULL,NULL,0, \
/* filp */	{NULL,}, \
/* ldt */	{ \
		{0,0}, \
		{0xFFFF, 0x00AFFA00},  /* 64-bit code: base=0, G=1, L=1, P=1, DPL=3, S=1, type=0xA */ \
		{0xFFFF, 0x00CFF200},  /* 64-bit data: base=0, G=1, P=1, DPL=3, S=1, type=0x2 */ \
	}, \
/* thread */	{0,}, \
/* i387 */	{0,}, \
/* stack */	{0,}, \
}

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void sleep_on(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-user_cs, 4-user_ds
 * 5-6: TSS (16 bytes in 64-bit mode, spans 2 entries)
 * 7-8: LDT0 (16 bytes in 64-bit mode, spans 2 entries)
 * etc...
 */
#define FIRST_TSS_ENTRY 5
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+2)  /* TSS is 16 bytes = 2 entries */
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
#define ltr(n) __asm__ volatile("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__ volatile("lldt %%ax"::"a" (_LDT(n)))

/*
 * Get current task number from TSS selector
 */
static inline int get_task_nr(void)
{
	unsigned short tr;
	__asm__ volatile ("str %0" : "=r" (tr));
	return (tr - (FIRST_TSS_ENTRY << 3)) >> 4;
}
#define str(n) ((n) = get_task_nr())

/*
 * Context switch implementation
 * __switch_to is implemented in assembly (kernel/switch.nasm)
 */
extern struct task_struct *__switch_to(struct task_struct *prev, struct task_struct *next);

/* Global TSS - defined in kernel/sched.c */
extern struct tss_struct init_tss;

#define switch_to(n) do { \
	struct task_struct *__next = task[n]; \
	if (__next && current != __next) { \
		struct task_struct *__prev = current; \
		current = __next; \
		/* Update TSS rsp0 to point to top of new task's kernel stack */ \
		init_tss.rsp0 = (unsigned long)__next + PAGE_SIZE; \
		__switch_to(__prev, __next); \
		if (current == last_task_used_math) \
			__asm__ volatile ("clts"); \
	} \
} while(0)

#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

/*
 * 64-bit segment descriptor manipulation
 */
static inline void _set_base(char *addr, unsigned long base)
{
	*(unsigned short *)(addr + 2) = base & 0xFFFF;
	*(addr + 4) = (base >> 16) & 0xFF;
	*(addr + 7) = (base >> 24) & 0xFF;
}

static inline void _set_limit(char *addr, unsigned long limit)
{
	*(unsigned short *)addr = limit & 0xFFFF;
	*(addr + 6) = (*(addr + 6) & 0xF0) | ((limit >> 16) & 0x0F);
}

#define set_base(ldt, base)  _set_base((char *)&(ldt), (unsigned long)(base))
#define set_limit(ldt, limit) _set_limit((char *)&(ldt), ((limit) - 1) >> 12)

static inline unsigned long _get_base(const char *addr)
{
	return (unsigned long)(*(unsigned short *)(addr + 2)) |
	       ((unsigned long)(*(unsigned char *)(addr + 4)) << 16) |
	       ((unsigned long)(*(unsigned char *)(addr + 7)) << 24);
}

#define get_base(ldt) _get_base((const char *)&(ldt))

static inline unsigned long get_limit(unsigned long segment)
{
	unsigned long limit;
	__asm__ volatile ("lsl %1, %0" : "=r" (limit) : "r" (segment));
	return limit + 1;
}

#endif
