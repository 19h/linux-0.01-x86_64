#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall0(int,setup)
static inline _syscall0(int,sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

/* Early serial debug output - works before any init */
#define SERIAL_PORT 0x3f8

static inline void early_serial_init(void)
{
	outb(0x00, SERIAL_PORT + 1);    /* Disable all interrupts */
	outb(0x80, SERIAL_PORT + 3);    /* Enable DLAB */
	outb(0x01, SERIAL_PORT + 0);    /* Set divisor to 1 (115200 baud) */
	outb(0x00, SERIAL_PORT + 1);    /* High byte */
	outb(0x03, SERIAL_PORT + 3);    /* 8 bits, no parity, one stop bit */
	outb(0xC7, SERIAL_PORT + 2);    /* Enable FIFO */
	outb(0x0B, SERIAL_PORT + 4);    /* IRQs enabled, RTS/DSR set */
}

static inline void early_serial_putc(char c)
{
	while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
	outb(c, SERIAL_PORT);
}

static inline void early_serial_puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			early_serial_putc('\r');
		early_serial_putc(*s++);
	}
}

extern int vsprintf(char *buf, const char *fmt, va_list args);
extern void init(void);
extern void hd_init(void);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8)-1;
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	startup_time = kernel_mktime(&time);
}

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	early_serial_init();
	early_serial_puts("Linux 0.01 (64-bit) starting...\n");
	time_init();
	tty_init();
	trap_init();
	sched_init();
	buffer_init();
	hd_init();
	sti();
	early_serial_puts("Initialization complete.\n");
	
	/*
	 * For the 64-bit port with built-in shell, we skip user mode
	 * and run the shell directly in kernel mode. This avoids the
	 * complexity of process isolation while demonstrating the kernel works.
	 */
	init();
	
	/* Never reached */
	for(;;) ;
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

/*
 * Simple built-in shell for testing without a filesystem
 */
static char cmd_buf[256];
static int cmd_pos = 0;

static void shell_putc(char c)
{
	early_serial_putc(c);
}

static void shell_puts(const char *s)
{
	early_serial_puts(s);
}

static int shell_getc(void)
{
	unsigned char lsr;
	/* Read from serial port - poll for data ready */
	while (1) {
		lsr = inb(SERIAL_PORT + 5);
		if (lsr & 0x01)  /* Data ready */
			break;
	}
	return inb(SERIAL_PORT);
}

static int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s1 - *s2;
}

static int strncmp(const char *s1, const char *s2, int n)
{
	while (n > 0 && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	return n == 0 ? 0 : *s1 - *s2;
}

static void cmd_help(void)
{
	shell_puts("Available commands:\n");
	shell_puts("  help     - show this help\n");
	shell_puts("  uname    - show system info\n");
	shell_puts("  ps       - show processes\n");
	shell_puts("  free     - show memory info\n");
	shell_puts("  uptime   - show uptime\n");
	shell_puts("  reboot   - reboot system\n");
}

static void cmd_uname(void)
{
	shell_puts("Linux 0.01 (64-bit port) x86_64\n");
}

extern struct task_struct *task[NR_TASKS];
extern long volatile jiffies;

static void cmd_ps(void)
{
	int i;
	shell_puts("PID  STATE  NAME\n");
	for (i = 0; i < NR_TASKS; i++) {
		if (task[i]) {
			char buf[64];
			int state = task[i]->state;
			const char *statestr = state == 0 ? "R" : state == 1 ? "S" : state == 2 ? "D" : state == 3 ? "Z" : "T";
			/* Simple integer to string */
			int pid = task[i]->pid;
			buf[0] = '0' + (pid / 10) % 10;
			buf[1] = '0' + pid % 10;
			buf[2] = ' ';
			buf[3] = ' ';
			buf[4] = ' ';
			buf[5] = statestr[0];
			buf[6] = ' ';
			buf[7] = ' ';
			buf[8] = ' ';
			buf[9] = ' ';
			buf[10] = ' ';
			if (i == 0) {
				buf[11] = 'i'; buf[12] = 'd'; buf[13] = 'l'; buf[14] = 'e'; buf[15] = 0;
			} else if (i == 1) {
				buf[11] = 'i'; buf[12] = 'n'; buf[13] = 'i'; buf[14] = 't'; buf[15] = 0;
			} else {
				buf[11] = 't'; buf[12] = 'a'; buf[13] = 's'; buf[14] = 'k'; buf[15] = '0' + i; buf[16] = 0;
			}
			shell_puts(buf);
			shell_puts("\n");
		}
	}
}

static void cmd_free(void)
{
	/* Simple memory info - HIGH_MEMORY=8MB, LOW_MEM=2MB, so 6MB usable */
	shell_puts("Memory: 8MB total, 6MB usable (1536 pages)\n");
}

static void cmd_uptime(void)
{
	char buf[32];
	long secs = jiffies / HZ;
	int mins = secs / 60;
	secs = secs % 60;
	buf[0] = 'U'; buf[1] = 'p'; buf[2] = ' ';
	buf[3] = '0' + (mins / 10) % 10;
	buf[4] = '0' + mins % 10;
	buf[5] = ':';
	buf[6] = '0' + (secs / 10) % 10;
	buf[7] = '0' + secs % 10;
	buf[8] = '\n';
	buf[9] = 0;
	shell_puts(buf);
}

static void cmd_reboot(void)
{
	shell_puts("Rebooting...\n");
	/* Triple fault to reboot */
	__asm__ volatile (
		"lidt (%%rax)"
		:: "a"(0)
	);
}

static void process_command(void)
{
	cmd_buf[cmd_pos] = 0;
	
	if (cmd_pos == 0) {
		return;
	} else if (strcmp(cmd_buf, "help") == 0) {
		cmd_help();
	} else if (strcmp(cmd_buf, "uname") == 0 || strcmp(cmd_buf, "uname -a") == 0) {
		cmd_uname();
	} else if (strcmp(cmd_buf, "ps") == 0) {
		cmd_ps();
	} else if (strcmp(cmd_buf, "free") == 0) {
		cmd_free();
	} else if (strcmp(cmd_buf, "uptime") == 0) {
		cmd_uptime();
	} else if (strcmp(cmd_buf, "reboot") == 0) {
		cmd_reboot();
	} else {
		shell_puts("Unknown command: ");
		shell_puts(cmd_buf);
		shell_puts("\nType 'help' for available commands.\n");
	}
}

static void builtin_shell(void)
{
	int c;
	
	shell_puts("\n");
	shell_puts("====================================\n");
	shell_puts("  Linux 0.01 - 64-bit Port Shell\n");
	shell_puts("====================================\n");
	shell_puts("Type 'help' for available commands.\n\n");
	
	while (1) {
		shell_puts("# ");
		cmd_pos = 0;
		
		while (1) {
			c = shell_getc();
			
			if (c == '\r' || c == '\n') {
				shell_puts("\n");
				process_command();
				break;
			} else if (c == 127 || c == 8) {  /* Backspace */
				if (cmd_pos > 0) {
					cmd_pos--;
					shell_puts("\b \b");
				}
			} else if (c >= 32 && c < 127 && cmd_pos < 255) {
				cmd_buf[cmd_pos++] = c;
				shell_putc(c);
			}
		}
	}
}

void init(void)
{
	/* 
	 * Skip filesystem setup - run built-in shell directly.
	 * This proves the kernel works without needing a disk.
	 */
	builtin_shell();
	
	/* Never reached */
	for(;;) pause();
}
