/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.nasm), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 *
 * 64-bit version: Uses thread_struct for context, not hardware TSS
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);
extern void ret_from_fork(void);

long last_pid=0;

void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);
	data_limit=get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine for 64-bit mode.
 *
 * In 64-bit mode, we don't use hardware task switching. Instead:
 * - thread_struct holds the kernel context (rsp, callee-saved regs)
 * - The child's kernel stack is set up to return from the fork syscall
 * - No per-task TSS; just update init_tss.rsp0 on context switch
 *
 * Parameters come from sys_fork in system_call.nasm which reads
 * saved registers from the interrupt/syscall stack frame.
 */
int copy_process(int nr, long rbp, long user_rdi, long user_rsi, long gs, 
		long none,
		long rbx, long rcx, long rdx,
		long fs, long es, long ds,
		long rip, long cs, long rflags, long rsp, long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;
	unsigned long *kstack;

	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	p->state = TASK_UNINTERRUPTIBLE;  /* prevent running until fully set up */
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	
	/*
	 * Set up the child's kernel stack.
	 * 
	 * When __switch_to returns, it pops the return address and jumps there.
	 * We set that up to be ret_from_fork, which then does ret_from_sys_call
	 * to restore all registers and iretq back to user mode.
	 *
	 * The stack needs to look like what SAVE_ALL created, but with RAX=0
	 * (fork return value for child).
	 */
	kstack = (unsigned long *)((unsigned long)p + PAGE_SIZE);
	
	/* Build the same stack frame as SAVE_ALL, top to bottom */
	/* CPU interrupt frame (what iretq will pop) */
	*--kstack = ss;            /* SS */
	*--kstack = rsp;           /* RSP (user stack) */
	*--kstack = rflags;        /* RFLAGS */
	*--kstack = cs;            /* CS */
	*--kstack = rip;           /* RIP (return address in user code) */
	
	/* General purpose registers (what RESTORE_ALL will pop) */
	*--kstack = 0;             /* R15 */
	*--kstack = 0;             /* R14 */
	*--kstack = 0;             /* R13 */
	*--kstack = 0;             /* R12 */
	*--kstack = 0;             /* R11 */
	*--kstack = 0;             /* R10 */
	*--kstack = 0;             /* R9 */
	*--kstack = 0;             /* R8 */
	*--kstack = rbp;           /* RBP */
	*--kstack = user_rsi;      /* RSI */
	*--kstack = user_rdi;      /* RDI */
	*--kstack = rdx;           /* RDX */
	*--kstack = rcx;           /* RCX */
	*--kstack = rbx;           /* RBX */
	*--kstack = 0;             /* RAX = 0 (fork returns 0 in child) */
	
	/* Segment registers */
	*--kstack = gs;            /* GS */
	*--kstack = fs;            /* FS */
	*--kstack = es;            /* ES */
	*--kstack = ds;            /* DS */
	
	/* 
	 * Now set up thread_struct for __switch_to.
	 * When we switch to this task, __switch_to will:
	 * 1. Restore RSP from thread.rsp 
	 * 2. Restore callee-saved regs (rbx, rbp, r12-r15)
	 * 3. ret (which pops ret_from_fork address)
	 *
	 * So we need to push ret_from_fork as the return address.
	 */
	*--kstack = (unsigned long)ret_from_fork;
	
	/* Set up thread struct - __switch_to will restore these */
	p->thread.rsp = (unsigned long)kstack;
	p->thread.rbx = rbx;
	p->thread.rbp = rbp;
	p->thread.r12 = 0;
	p->thread.r13 = 0;
	p->thread.r14 = 0;
	p->thread.r15 = 0;
	p->thread.fs = fs;
	p->thread.gs = gs;
	
	/* Save FPU state if parent used math */
	if (last_task_used_math == current)
		__asm__ volatile("fxsave %0" : "=m" (p->i387));
	
	if (copy_mem(nr,p)) {
		free_page((long) p);
		return -EAGAIN;
	}
	
	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]) != NULL)
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	
	/*
	 * In 64-bit mode, we don't set up per-task TSS descriptors.
	 * There's only one TSS (init_tss), and we update rsp0 on context switch.
	 * We still set up per-task LDT for compatibility.
	 */
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	
	p->state = TASK_RUNNING;  /* Now it's safe to run */
	task[nr] = p;	/* do this last, just in case */
	return last_pid;
}

int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
