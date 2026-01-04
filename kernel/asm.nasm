;
; asm.s contains the low-level code for most hardware faults.
; page_exception is handled by the mm, so that isn't here. This
; file also handles (hopefully) fpu-exceptions due to TS-bit, as
; the fpu must be properly saved/restored. This hasn't been tested.
;
; 64-bit version
;

[BITS 64]

section .text

; Export interrupt handlers
global divide_error, debug, nmi, int3, overflow, bounds, invalid_op
global device_not_available, double_fault, coprocessor_segment_overrun
global invalid_TSS, segment_not_present, stack_segment
global general_protection, coprocessor_error, reserved

; Import C handlers
extern do_divide_error, do_int3, do_nmi, do_overflow, do_bounds
extern do_invalid_op, do_device_not_available, do_double_fault
extern do_coprocessor_segment_overrun, do_invalid_TSS
extern do_segment_not_present, do_stack_segment, do_general_protection
extern do_coprocessor_error, do_reserved
extern math_state_restore, current, last_task_used_math

; Stack frame after interrupt (without error code):
;   +40  SS
;   +32  RSP
;   +24  RFLAGS
;   +16  CS
;   +8   RIP
;   +0   (error code if present, else we push 0)

divide_error:
    push    qword 0             ; Fake error code
    push    rax
    lea     rax, [do_divide_error]
    jmp     no_error_code_common

no_error_code_common:
    ; RAX = handler address
    ; Stack: error_code, saved_rax, <interrupt frame>
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
    
    ; Save segment registers (though in 64-bit they're mostly unused)
    mov     rbx, ds
    push    rbx
    mov     rbx, es
    push    rbx
    
    ; Set up kernel data segments
    mov     bx, 0x10
    mov     ds, bx
    mov     es, bx
    
    ; Call handler: handler(rsp, error_code)
    ; rdi = pointer to registers on stack
    ; rsi = error code
    mov     rdi, rsp
    mov     rsi, [rsp + 18*8]   ; error code (after 16 regs + 2 seg regs)
    
    ; Ensure stack is 16-byte aligned for call
    and     rsp, ~0xF
    
    call    rax
    
    ; Restore
    pop     rbx
    mov     es, bx
    pop     rbx
    mov     ds, bx
    
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

debug:
    push    qword 0
    push    rax
    lea     rax, [do_int3]      ; Using do_int3 for debug too
    jmp     no_error_code_common

nmi:
    push    qword 0
    push    rax
    lea     rax, [do_nmi]
    jmp     no_error_code_common

int3:
    push    qword 0
    push    rax
    lea     rax, [do_int3]
    jmp     no_error_code_common

overflow:
    push    qword 0
    push    rax
    lea     rax, [do_overflow]
    jmp     no_error_code_common

bounds:
    push    qword 0
    push    rax
    lea     rax, [do_bounds]
    jmp     no_error_code_common

invalid_op:
    push    qword 0
    push    rax
    lea     rax, [do_invalid_op]
    jmp     no_error_code_common

device_not_available:
    push    qword 0
    push    rax
    ; Check if we need to do math emulation or state restore
    mov     rax, cr0
    bt      rax, 2              ; Check EM bit
    jc      .math_emulate
    clts                        ; Clear TS flag
    ; Check if this task was using math
    mov     rax, [current]
    cmp     rax, [last_task_used_math]
    je      .done
    ; Save/restore math state
    push    rdi
    push    rsi
    call    math_state_restore
    pop     rsi
    pop     rdi
.done:
    pop     rax
    add     rsp, 8
    iretq
.math_emulate:
    lea     rax, [do_device_not_available]
    jmp     no_error_code_common

coprocessor_segment_overrun:
    push    qword 0
    push    rax
    lea     rax, [do_coprocessor_segment_overrun]
    jmp     no_error_code_common

reserved:
    push    qword 0
    push    rax
    lea     rax, [do_reserved]
    jmp     no_error_code_common

coprocessor_error:
    push    qword 0
    push    rax
    lea     rax, [do_coprocessor_error]
    jmp     no_error_code_common

; Exceptions with error code pushed by CPU
double_fault:
    ; Error code already pushed by CPU
    push    rax
    lea     rax, [do_double_fault]
    jmp     error_code_common

error_code_common:
    ; Stack: error_code (from CPU), saved_rax
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
    
    mov     rbx, ds
    push    rbx
    mov     rbx, es
    push    rbx
    
    mov     bx, 0x10
    mov     ds, bx
    mov     es, bx
    
    ; Call handler(rsp, error_code)
    mov     rdi, rsp
    mov     rsi, [rsp + 18*8]   ; error code
    
    and     rsp, ~0xF
    call    rax
    
    pop     rbx
    mov     es, bx
    pop     rbx
    mov     ds, bx
    
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

invalid_TSS:
    push    rax
    lea     rax, [do_invalid_TSS]
    jmp     error_code_common

segment_not_present:
    push    rax
    lea     rax, [do_segment_not_present]
    jmp     error_code_common

stack_segment:
    push    rax
    lea     rax, [do_stack_segment]
    jmp     error_code_common

general_protection:
    push    rax
    lea     rax, [do_general_protection]
    jmp     error_code_common
