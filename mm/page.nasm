;
; page.s - 64-bit page fault handler
;
; The page fault handler for 64-bit mode.
; CR2 contains the faulting virtual address.
;

[BITS 64]

section .text

global page_fault

extern do_no_page, do_wp_page

page_fault:
    ; Error code was pushed by CPU
    ; Stack: error_code, RIP, CS, RFLAGS, RSP, SS
    
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rdi
    push    rsi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    
    ; Set up kernel segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    
    ; Get error code (saved after all registers)
    mov     rsi, [rsp + 15*8]   ; error_code
    
    ; Get faulting address from CR2
    mov     rdi, cr2
    
    ; Check error code bit 0: 0 = page not present, 1 = protection violation
    test    rsi, 1
    jnz     .write_protect
    
    ; Page not present
    call    do_no_page
    jmp     .done
    
.write_protect:
    ; Write protection fault
    call    do_wp_page
    
.done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rsi
    pop     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    add     rsp, 8              ; Pop error code
    iretq
