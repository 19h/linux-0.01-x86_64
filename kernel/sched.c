/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <signal.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task), *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

/*
 * Global TSS for 64-bit mode
 * In long mode, we only need one TSS for all tasks.
 * It's used only for:
 * - RSP0: kernel stack pointer (updated on task switch)
 * - IST: interrupt stack table entries
 * - IOPB: I/O permission bitmap
 * Context switching is done entirely in software via __switch_to
 */
struct tss_struct init_tss __attribute__((aligned(16))) = {
	.reserved0 = 0,
	.rsp0 = 0,  /* Will be set during sched_init */
	.rsp1 = 0,
	.rsp2 = 0,
	.reserved1 = 0,
	.ist = {0,},
	.reserved2 = 0,
	.reserved3 = 0,
	.iopb_offset = sizeof(struct tss_struct),  /* No IOPB, point past TSS */
};

/* __switch_to is implemented in switch.nasm and declared in linux/sched.h */

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 *
 * In 64-bit mode, we use fxsave/fxrstor for SSE state (512 bytes)
 */
void math_state_restore()
{
	if (last_task_used_math)
		__asm__ volatile("fxsave %0" : "=m" (last_task_used_math->i387));
	if (current->used_math)
		__asm__ volatile("fxrstor %0" :: "m" (current->i387));
	else {
		__asm__ volatile("fninit");
		current->used_math=1;
	}
	last_task_used_math=current;
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if ((*p)->signal && (*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

void do_timer(long cpl)
{
	if (cpl)
		current->utime++;
	else
		current->stime++;
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds)
{
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return seconds;
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

int sys_signal(long signal,long addr,long restorer)
{
	long i;

	switch (signal) {
		case SIGHUP: case SIGINT: case SIGQUIT: case SIGILL:
		case SIGTRAP: case SIGABRT: case SIGFPE: case SIGUSR1:
		case SIGSEGV: case SIGUSR2: case SIGPIPE: case SIGALRM:
		case SIGCHLD:
			i=(long) current->sig_fn[signal-1];
			current->sig_fn[signal-1] = (fn_ptr) addr;
			current->sig_restorer = (fn_ptr) restorer;
			return i;
		default: return -1;
	}
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	/*
	 * In 64-bit mode, we use a single global TSS.
	 * Set rsp0 to the top of init_task's kernel stack.
	 */
	init_tss.rsp0 = (unsigned long)&init_task + PAGE_SIZE;
	
	/* Set up TSS descriptor in GDT (16 bytes in 64-bit mode) */
	set_tss_desc(gdt+FIRST_TSS_ENTRY, &init_tss);
	
	/* Set up LDT descriptor for init task */
	set_ldt_desc(gdt+FIRST_LDT_ENTRY, &(init_task.task.ldt));
	
	/* Clear remaining GDT entries for other tasks' LDTs */
	p = gdt+2+FIRST_LDT_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
	
	/* Load task register with TSS selector */
	ltr(0);
	/* Load LDT register with init task's LDT */
	lldt(0);
	
	/* Set up PIT timer for 100 Hz */
	outb_p(0x36,0x43);
	outb_p(LATCH & 0xff , 0x40);
	outb(LATCH >> 8 , 0x40);
	
	/* Set up timer interrupt handler */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	
	/* Set up system call interrupt */
	set_system_gate(0x80,&system_call);
}
