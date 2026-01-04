/*
 * sys.h - System call declarations and table
 *
 * Note: These declarations use (...) to indicate variable/unknown parameters
 * This is necessary because the actual syscall implementations have different
 * signatures that would conflict with void declarations in modern C compilers.
 */

/* System call declarations - using ... to avoid conflicts with implementations */
extern int sys_setup(void);
extern int sys_exit(int);
extern int sys_fork(void);
extern int sys_read(int, char *, int);
extern int sys_write(int, const char *, int);
extern int sys_open(const char *, int, int);
extern int sys_close(int);
extern int sys_waitpid(int, int *, int);
extern int sys_creat(const char *, int);
extern int sys_link(const char *, const char *);
extern int sys_unlink(const char *);
extern int sys_execve(void);
extern int sys_chdir(const char *);
extern int sys_time(long *);
extern int sys_mknod(const char *, int, int);
extern int sys_chmod(const char *, int);
extern int sys_chown(const char *, int, int);
extern int sys_break(void);
extern int sys_stat(const char *, void *);
extern int sys_lseek(int, long, int);
extern int sys_getpid(void);
extern int sys_mount(void);
extern int sys_umount(const char *);
extern int sys_setuid(int);
extern int sys_getuid(void);
extern int sys_stime(long *);
extern int sys_ptrace(void);
extern int sys_alarm(long);
extern int sys_fstat(int, void *);
extern int sys_pause(void);
extern int sys_utime(const char *, void *);
extern int sys_stty(void);
extern int sys_gtty(void);
extern int sys_access(const char *, int);
extern int sys_nice(long);
extern int sys_ftime(void);
extern int sys_sync(void);
extern int sys_kill(int, int);
extern int sys_rename(const char *, const char *);
extern int sys_mkdir(const char *, int);
extern int sys_rmdir(const char *);
extern int sys_dup(int);
extern int sys_pipe(int *);
extern int sys_times(void *);
extern int sys_prof(void);
extern int sys_brk(void *);
extern int sys_setgid(int);
extern int sys_getgid(void);
extern int sys_signal(long, long, long);
extern int sys_geteuid(void);
extern int sys_getegid(void);
extern int sys_acct(void);
extern int sys_phys(void);
extern int sys_lock(void);
extern int sys_ioctl(int, int, long);
extern int sys_fcntl(int, int, long);
extern int sys_mpx(void);
extern int sys_setpgid(int, int);
extern int sys_ulimit(void);
extern int sys_uname(void *);
extern int sys_umask(int);
extern int sys_chroot(const char *);
extern int sys_ustat(int, void *);
extern int sys_dup2(int, int);
extern int sys_getppid(void);
extern int sys_getpgrp(void);
extern int sys_setsid(void);

fn_ptr sys_call_table[] = { 
	(fn_ptr)sys_setup, (fn_ptr)sys_exit, (fn_ptr)sys_fork, (fn_ptr)sys_read,
	(fn_ptr)sys_write, (fn_ptr)sys_open, (fn_ptr)sys_close, (fn_ptr)sys_waitpid,
	(fn_ptr)sys_creat, (fn_ptr)sys_link, (fn_ptr)sys_unlink, (fn_ptr)sys_execve,
	(fn_ptr)sys_chdir, (fn_ptr)sys_time, (fn_ptr)sys_mknod, (fn_ptr)sys_chmod,
	(fn_ptr)sys_chown, (fn_ptr)sys_break, (fn_ptr)sys_stat, (fn_ptr)sys_lseek,
	(fn_ptr)sys_getpid, (fn_ptr)sys_mount, (fn_ptr)sys_umount, (fn_ptr)sys_setuid,
	(fn_ptr)sys_getuid, (fn_ptr)sys_stime, (fn_ptr)sys_ptrace, (fn_ptr)sys_alarm,
	(fn_ptr)sys_fstat, (fn_ptr)sys_pause, (fn_ptr)sys_utime, (fn_ptr)sys_stty,
	(fn_ptr)sys_gtty, (fn_ptr)sys_access, (fn_ptr)sys_nice, (fn_ptr)sys_ftime,
	(fn_ptr)sys_sync, (fn_ptr)sys_kill, (fn_ptr)sys_rename, (fn_ptr)sys_mkdir,
	(fn_ptr)sys_rmdir, (fn_ptr)sys_dup, (fn_ptr)sys_pipe, (fn_ptr)sys_times,
	(fn_ptr)sys_prof, (fn_ptr)sys_brk, (fn_ptr)sys_setgid, (fn_ptr)sys_getgid,
	(fn_ptr)sys_signal, (fn_ptr)sys_geteuid, (fn_ptr)sys_getegid, (fn_ptr)sys_acct,
	(fn_ptr)sys_phys, (fn_ptr)sys_lock, (fn_ptr)sys_ioctl, (fn_ptr)sys_fcntl,
	(fn_ptr)sys_mpx, (fn_ptr)sys_setpgid, (fn_ptr)sys_ulimit, (fn_ptr)sys_uname,
	(fn_ptr)sys_umask, (fn_ptr)sys_chroot, (fn_ptr)sys_ustat, (fn_ptr)sys_dup2,
	(fn_ptr)sys_getppid, (fn_ptr)sys_getpgrp, (fn_ptr)sys_setsid
};
