;
; rs_io.s - 64-bit serial I/O interrupt handlers
;

[BITS 64]

section .text

global rs1_interrupt, rs2_interrupt

extern table_list, do_tty_interrupt

size        equ 1024
rs_addr     equ 0
head        equ 8
tail        equ 16
proc_list   equ 24
buf         equ 32
startup     equ 256

align 16
rs1_interrupt:
    push    qword table_list + 16   ; Queue for COM1 (offset 16 for 64-bit)
    jmp     rs_int

align 16
rs2_interrupt:
    push    qword table_list + 32   ; Queue for COM2

rs_int:
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rdi
    push    rsi
    push    r8
    push    r9
    push    r10
    push    r11
    
    ; Set up kernel segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    
    ; Get queue pointer
    mov     rdx, [rsp + 10*8]       ; Queue address from stack
    mov     rdx, [rdx]              ; Dereference
    mov     rdx, [rdx + rs_addr]    ; Get port base address
    add     rdx, 2                  ; IIR register

rep_int:
    xor     rax, rax
    in      al, dx
    test    al, 1                   ; Check if interrupt pending
    jnz     end_int
    cmp     al, 6
    ja      end_int
    
    mov     rcx, [rsp + 10*8]
    push    rdx
    sub     rdx, 2
    lea     rbx, [jmp_table]
    call    [rbx + rax*4]           ; Note: still using 32-bit offsets in table
    pop     rdx
    jmp     rep_int

end_int:
    mov     al, 0x20
    out     0x20, al
    
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rsi
    pop     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    add     rsp, 8                  ; Pop queue pointer
    iretq

jmp_table:
    dq modem_status
    dq write_char
    dq read_char
    dq line_status

modem_status:
    add     rdx, 6
    in      al, dx
    ret

line_status:
    add     rdx, 5
    in      al, dx
    ret

read_char:
    in      al, dx
    mov     rdx, rcx
    sub     rdx, table_list
    shr     rdx, 3                  ; Divide by 8 for 64-bit
    mov     rcx, [rcx]
    mov     rbx, [rcx + head]
    mov     [rcx + buf + rbx], al
    inc     rbx
    and     rbx, size - 1
    cmp     rbx, [rcx + tail]
    je      .done
    mov     [rcx + head], rbx
    push    rdx
    mov     rdi, rdx
    call    do_tty_interrupt
    pop     rdx
.done:
    ret

write_char:
    mov     rcx, [rcx + 8]          ; write-queue (offset 8 for 64-bit pointer)
    mov     rbx, [rcx + head]
    sub     rbx, [rcx + tail]
    and     rbx, size - 1
    jz      write_buffer_empty
    cmp     rbx, startup
    ja      .write
    mov     rbx, [rcx + proc_list]
    test    rbx, rbx
    jz      .write
    mov     qword [rbx], 0
.write:
    mov     rbx, [rcx + tail]
    mov     al, [rcx + buf + rbx]
    out     dx, al
    inc     rbx
    and     rbx, size - 1
    mov     [rcx + tail], rbx
    cmp     rbx, [rcx + head]
    je      write_buffer_empty
    ret

write_buffer_empty:
    mov     rbx, [rcx + proc_list]
    test    rbx, rbx
    jz      .disable
    mov     qword [rbx], 0
.disable:
    inc     rdx
    in      al, dx
    jmp     short .d1
.d1:    jmp     short .d2
.d2:    and     al, 0x0d
    out     dx, al
    ret
