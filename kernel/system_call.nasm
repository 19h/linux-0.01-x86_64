;
; system_call.nasm - 64-bit system call and interrupt handling
;
; In 64-bit mode, syscall/sysret is preferred over int 0x80, but
; we'll support both for compatibility.
;
; System V AMD64 ABI calling convention:
;   Args: rdi, rsi, rdx, rcx, r8, r9 (then stack)
;   Return: rax (and rdx for 128-bit)
;   Caller-saved: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
;   Callee-saved: rbx, rbp, r12, r13, r14, r15
;
; For syscalls via int 0x80 (Linux 32-bit compat):
;   Syscall #: rax
;   Args: rbx, rcx, rdx, rsi, rdi, rbp
;

[BITS 64]

section .text

SIG_CHLD        equ 17
nr_system_calls equ 67

; Stack frame after all pushes (bottom = higher address)
; CPU pushes (on privilege change): SS, RSP, RFLAGS, CS, RIP
; We push: DS, ES, FS, GS, RAX, RBX, RCX, RDX, RDI, RSI, RBP, R8-R15
; 
; Offsets from RSP after all pushes:
FRAME_DS        equ 0
FRAME_ES        equ 8
FRAME_FS        equ 16
FRAME_GS        equ 24
FRAME_RAX       equ 32
FRAME_RBX       equ 40
FRAME_RCX       equ 48
FRAME_RDX       equ 56
FRAME_RDI       equ 64
FRAME_RSI       equ 72
FRAME_RBP       equ 80
FRAME_R8        equ 88
FRAME_R9        equ 96
FRAME_R10       equ 104
FRAME_R11       equ 112
FRAME_R12       equ 120
FRAME_R13       equ 128
FRAME_R14       equ 136
FRAME_R15       equ 144
; Interrupt frame (pushed by CPU):
FRAME_RIP       equ 152
FRAME_CS        equ 160
FRAME_RFLAGS    equ 168
FRAME_RSP       equ 176     ; User RSP (only on privilege change)
FRAME_SS        equ 184     ; User SS (only on privilege change)

; Task struct offsets (updated for 64-bit)
state           equ 0
counter         equ 8
priority        equ 16
signal          equ 24
restorer        equ 32
sig_fn          equ 40

; Export symbols
global system_call, sys_fork, timer_interrupt, hd_interrupt, sys_execve
global ret_from_fork, ret_from_sys_call

; Import from C
extern sys_call_table, schedule, current, task, jiffies
extern do_timer, do_execve, find_empty_process, copy_process
extern verify_area, do_exit, do_hd, unexpected_hd_interrupt

;
; Macro to save all registers
;
%macro SAVE_ALL 0
    push    r15
    push    r14
    push    r13
    push    r12
    push    r11
    push    r10
    push    r9
    push    r8
    push    rbp
    push    rsi
    push    rdi
    push    rdx
    push    rcx
    push    rbx
    push    rax
    xor     rax, rax
    mov     ax, gs
    push    rax
    mov     ax, fs
    push    rax
    mov     ax, es
    push    rax
    mov     ax, ds
    push    rax
    mov     ax, 0x10        ; Kernel data segment
    mov     ds, ax
    mov     es, ax
%endmacro

;
; Macro to restore all registers
;
%macro RESTORE_ALL 0
    pop     rax
    mov     ds, ax
    pop     rax
    mov     es, ax
    pop     rax
    mov     fs, ax
    pop     rax
    mov     gs, ax
    pop     rax
    pop     rbx
    pop     rcx
    pop     rdx
    pop     rdi
    pop     rsi
    pop     rbp
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15
%endmacro

align 16
bad_sys_call:
    mov     qword [rsp + FRAME_RAX], -1
    jmp     ret_from_sys_call

align 16
reschedule:
    call    schedule
    jmp     ret_from_sys_call

align 16
system_call:
    SAVE_ALL
    
    ; Get syscall number from saved RAX
    mov     rax, [rsp + FRAME_RAX]
    
    ; Validate syscall number  
    cmp     rax, nr_system_calls
    jae     bad_sys_call
    
    ; Set up args: Linux uses rbx, rcx, rdx, rsi, rdi, rbp for syscall args
    ; Fetch from saved registers, convert to System V AMD64 ABI
    mov     rdi, [rsp + FRAME_RBX]      ; arg1
    mov     rsi, [rsp + FRAME_RCX]      ; arg2
    mov     rdx, [rsp + FRAME_RDX]      ; arg3
    mov     rcx, [rsp + FRAME_RSI]      ; arg4
    mov     r8,  [rsp + FRAME_RDI]      ; arg5
    mov     r9,  [rsp + FRAME_RBP]      ; arg6
    
    ; Call the syscall handler
    call    [sys_call_table + rax*8]
    
    ; Save return value
    mov     [rsp + FRAME_RAX], rax

ret_from_sys_call:
    ; Check if we need to reschedule
    mov     rax, [current]
    cmp     qword [rax + state], 0
    jne     reschedule
    cmp     qword [rax + counter], 0
    je      reschedule
    
    ; Check for signals
    mov     rax, [current]
    cmp     rax, [task]         ; Don't process signals for task 0
    je      .restore
    
    ; Check if returning to user mode
    mov     rbx, [rsp + FRAME_CS]
    test    rbx, 3
    jz      .restore            ; Kernel mode, no signals
    
    ; TODO: Full signal handling
    ; For now, just restore and return

.restore:
    RESTORE_ALL
    iretq

;
; ret_from_fork - Entry point for new child processes after fork
;
; When __switch_to switches to a newly forked child, it returns here.
; The child's kernel stack has been set up by copy_process with:
; - Saved registers (SAVE_ALL format)  
; - Return value (RAX = 0)
; 
; We just need to restore registers and iretq back to user mode.
;
align 16
ret_from_fork:
    ; Child process starts here after first context switch
    ; Stack already has saved registers from copy_process
    jmp     ret_from_sys_call

align 16
timer_interrupt:
    SAVE_ALL
    
    ; Increment jiffies
    inc     qword [jiffies]
    
    ; Send EOI to PIC
    mov     al, 0x20
    out     0x20, al
    
    ; Call do_timer(cpl)
    ; cpl = 0 if from kernel, 3 if from user
    mov     rdi, [rsp + FRAME_CS]
    and     rdi, 3
    call    do_timer
    
    jmp     ret_from_sys_call

align 16
sys_execve:
    ; rdi already has the first argument from syscall
    ; We need to pass pointer to saved registers (for RIP modification)
    lea     rsi, [rsp + FRAME_RIP]
    call    do_execve
    ret

align 16
sys_fork:
    ; Find an empty process slot
    call    find_empty_process
    test    rax, rax
    js      .done               ; Error, return negative value
    
    ; copy_process(nr, rbp, rdi, rsi, gs, none, rbx, rcx, rdx, fs, es, ds,
    ;              rip, cs, rflags, rsp, ss)
    ; Push all the arguments (right to left for cdecl-style)
    ; System V AMD64 ABI: first 6 in registers, rest on stack
    
    mov     rdi, rax            ; nr (process slot)
    mov     rsi, [rsp + FRAME_RBP]      ; rbp
    mov     rdx, [rsp + FRAME_RDI]      ; rdi (user's rdi)
    mov     rcx, [rsp + FRAME_RSI]      ; rsi (user's rsi)
    mov     r8,  [rsp + FRAME_GS]       ; gs
    mov     r9,  0                      ; none (placeholder)
    
    ; Remaining args on stack (reverse order)
    push    qword [rsp + FRAME_SS + 8]      ; ss (+8 because we pushed)
    push    qword [rsp + FRAME_RSP + 16]    ; rsp (+16)
    push    qword [rsp + FRAME_RFLAGS + 24] ; rflags
    push    qword [rsp + FRAME_CS + 32]     ; cs
    push    qword [rsp + FRAME_RIP + 40]    ; rip
    push    qword [rsp + FRAME_DS + 48]     ; ds
    push    qword [rsp + FRAME_ES + 56]     ; es
    push    qword [rsp + FRAME_FS + 64]     ; fs
    push    qword [rsp + FRAME_RDX + 72]    ; rdx
    push    qword [rsp + FRAME_RCX + 80]    ; rcx
    push    qword [rsp + FRAME_RBX + 88]    ; rbx
    
    call    copy_process
    
    ; Clean up stack (11 args * 8 bytes = 88 bytes)
    add     rsp, 88

.done:
    ret

align 16
hd_interrupt:
    SAVE_ALL
    
    ; EOI to both PICs
    mov     al, 0x20
    out     0xA0, al            ; Slave first
    jmp     short .delay1
.delay1:
    jmp     short .delay2
.delay2:
    out     0x20, al            ; Then master
    
    ; Call handler
    mov     rax, [do_hd]
    test    rax, rax
    jnz     .call
    lea     rax, [unexpected_hd_interrupt]
.call:
    call    rax
    
    RESTORE_ALL
    iretq

; do_hd is defined in hd.c as: void (*do_hd)(void) = NULL;
