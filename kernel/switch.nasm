;
; switch.nasm - 64-bit context switch implementation
;
; struct task_struct *__switch_to(struct task_struct *prev, struct task_struct *next)
;
; Arguments (System V AMD64 ABI):
;   rdi = prev task_struct pointer
;   rsi = next task_struct pointer
;
; Returns:
;   rax = prev (for potential use by caller)
;
; We need to save/restore callee-saved registers: rbx, rbp, r12-r15
; The stack pointer and return address handle rsp/rip
;

[BITS 64]

section .text

global __switch_to

; Offsets into thread_struct within task_struct
; These must match the C structure layout!
; task_struct layout:
;   0x00: state, counter, priority, signal (8 bytes each = 32 bytes)
;   0x20: sig_restorer (8 bytes)
;   0x28: sig_fn[32] (256 bytes)
;   0x128: exit_code (4 bytes) + padding
;   ... lots more fields ...
; For simplicity, we'll use a define for the thread_struct offset
; This needs to be calculated based on actual struct layout

; Thread struct offsets (relative to start of thread_struct)
THREAD_RSP      equ 0
THREAD_RIP      equ 8
THREAD_RBX      equ 16
THREAD_RBP      equ 24
THREAD_R12      equ 32
THREAD_R13      equ 40
THREAD_R14      equ 48
THREAD_R15      equ 56

; Offset of thread_struct within task_struct
; Calculated from C struct layout:
;   state(8)+counter(8)+priority(8)+signal(8)+sig_restorer(8)+sig_fn[32](256)
;   +exit_code(4)+pad(4)+end_code(8)+end_data(8)+brk(8)+start_stack(8)
;   +pid(8)+father(8)+pgrp(8)+session(8)+leader(8)
;   +uid(2)+euid(2)+suid(2)+gid(2)+egid(2)+sgid(2)+pad(4)
;   +alarm(8)+utime(8)+stime(8)+cutime(8)+cstime(8)+start_time(8)
;   +used_math(2)+pad(2)+tty(4)+umask(2)+pad(6)
;   +pwd(8)+root(8)+close_on_exec(8)+filp[32](256)
;   +ldt[3](48) = 784 bytes
; Verified with offsetof(struct task_struct, thread) = 784 = 0x310

TASK_THREAD     equ 784     ; Offset of thread_struct in task_struct

__switch_to:
    ; Save prev's callee-saved registers
    ; prev is in rdi, next is in rsi
    
    ; Calculate pointer to prev's thread_struct
    lea     rax, [rdi + TASK_THREAD]
    
    ; Save callee-saved registers to prev's thread_struct
    mov     [rax + THREAD_RBX], rbx
    mov     [rax + THREAD_RBP], rbp
    mov     [rax + THREAD_R12], r12
    mov     [rax + THREAD_R13], r13
    mov     [rax + THREAD_R14], r14
    mov     [rax + THREAD_R15], r15
    
    ; Save stack pointer
    mov     [rax + THREAD_RSP], rsp
    
    ; Calculate pointer to next's thread_struct
    lea     rax, [rsi + TASK_THREAD]
    
    ; Restore next's stack pointer
    mov     rsp, [rax + THREAD_RSP]
    
    ; Restore callee-saved registers from next's thread_struct
    mov     rbx, [rax + THREAD_RBX]
    mov     rbp, [rax + THREAD_RBP]
    mov     r12, [rax + THREAD_R12]
    mov     r13, [rax + THREAD_R13]
    mov     r14, [rax + THREAD_R14]
    mov     r15, [rax + THREAD_R15]
    
    ; Return prev in rax (already in rdi, move it)
    mov     rax, rdi
    
    ret
