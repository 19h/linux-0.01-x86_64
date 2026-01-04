;
; keyboard.s - 64-bit keyboard interrupt handler
;

[BITS 64]

section .text

global keyboard_interrupt

extern do_tty_interrupt, table_list

; Buffer constants
size        equ 1024
head        equ 8           ; Offset in queue struct (64-bit pointers)
tail        equ 16
proc_list   equ 24
buf         equ 32

section .data
mode:   db 0                ; caps, alt, ctrl and shift mode
leds:   db 2                ; num-lock on
e0:     db 0

section .text

keyboard_interrupt:
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
    
    xor     rax, rax
    in      al, 0x60            ; Read scan code
    
    cmp     al, 0xe0
    je      set_e0
    cmp     al, 0xe1
    je      set_e1
    
    ; Call key handler
    lea     rbx, [key_table]
    call    [rbx + rax*8]
    mov     byte [e0], 0

e0_e1:
    ; Acknowledge keyboard
    in      al, 0x61
    jmp     short .d1
.d1:    jmp     short .d2
.d2:    or      al, 0x80
    jmp     short .d3
.d3:    jmp     short .d4
.d4:    out     0x61, al
    jmp     short .d5
.d5:    jmp     short .d6
.d6:    and     al, 0x7F
    out     0x61, al
    
    ; EOI
    mov     al, 0x20
    out     0x20, al
    
    ; Notify TTY
    xor     rdi, rdi
    call    do_tty_interrupt
    
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
    iretq

set_e0:
    mov     byte [e0], 1
    jmp     e0_e1

set_e1:
    mov     byte [e0], 2
    jmp     e0_e1

; Put character(s) in queue
put_queue:
    push    rcx
    push    rdx
    mov     rdx, [table_list]
    mov     rcx, [rdx + head]
.loop:
    mov     [rdx + buf + rcx], al
    inc     rcx
    and     rcx, size - 1
    cmp     rcx, [rdx + tail]
    je      .done
    shrd    rax, rbx, 8
    jz      .store
    shr     rbx, 8
    jmp     .loop
.store:
    mov     [rdx + head], rcx
    mov     rcx, [rdx + proc_list]
    test    rcx, rcx
    jz      .done
    mov     qword [rcx], 0
.done:
    pop     rdx
    pop     rcx
    ret

ctrl:
    mov     al, 0x04
    jmp     short ctrl_alt_common
alt:
    mov     al, 0x10
ctrl_alt_common:
    cmp     byte [e0], 0
    je      .store
    add     al, al
.store:
    or      [mode], al
    ret

unctrl:
    mov     al, 0x04
    jmp     short unctrl_unalt_common
unalt:
    mov     al, 0x10
unctrl_unalt_common:
    cmp     byte [e0], 0
    je      .store
    add     al, al
.store:
    not     al
    and     [mode], al
    ret

lshift:
    or      byte [mode], 0x01
    ret
unlshift:
    and     byte [mode], 0xfe
    ret
rshift:
    or      byte [mode], 0x02
    ret
unrshift:
    and     byte [mode], 0xfd
    ret

caps:
    test    byte [mode], 0x80
    jne     caps_done
    xor     byte [leds], 4
    xor     byte [mode], 0x40
    or      byte [mode], 0x80
set_leds:
    call    kb_wait
    mov     al, 0xed
    out     0x60, al
    call    kb_wait
    mov     al, [leds]
    out     0x60, al
caps_done:
    ret

uncaps:
    and     byte [mode], 0x7f
    ret

scroll:
    xor     byte [leds], 1
    jmp     set_leds

num:
    xor     byte [leds], 2
    jmp     set_leds

cursor:
    sub     al, 0x47
    jb      cursor_done
    cmp     al, 12
    ja      cursor_done
    jne     cur2
    ; Check for ctrl-alt-del
    test    byte [mode], 0x0c
    je      cur2
    test    byte [mode], 0x30
    jne     reboot
cur2:
    cmp     byte [e0], 0x01
    je      cur
    test    byte [leds], 0x02
    je      cur
    test    byte [mode], 0x03
    jne     cur
    xor     rbx, rbx
    lea     rcx, [num_table]
    mov     al, [rcx + rax]
    jmp     put_queue
cursor_done:
    ret

cur:
    lea     rcx, [cur_table]
    mov     al, [rcx + rax]
    cmp     al, '9'
    ja      ok_cur
    mov     ah, '~'
ok_cur:
    shl     eax, 16
    mov     ax, 0x5b1b
    xor     rbx, rbx
    jmp     put_queue

num_table:
    db "789 456 1230,"
cur_table:
    db "HA5 DGC YB623"

func:
    sub     al, 0x3B
    jb      func_done
    cmp     al, 9
    jbe     ok_func
    sub     al, 18
    cmp     al, 10
    jb      func_done
    cmp     al, 11
    ja      func_done
ok_func:
    cmp     rcx, 4
    jl      func_done
    lea     rcx, [func_table]
    mov     eax, [rcx + rax*4]
    xor     rbx, rbx
    jmp     put_queue
func_done:
    ret

func_table:
    dd 0x415b5b1b, 0x425b5b1b, 0x435b5b1b, 0x445b5b1b
    dd 0x455b5b1b, 0x465b5b1b, 0x475b5b1b, 0x485b5b1b
    dd 0x495b5b1b, 0x4a5b5b1b, 0x4b5b5b1b, 0x4c5b5b1b

key_map:
    db 0, 27, "1234567890+'", 127, 9
    db "qwertyuiop}", 0, 10, 0, "asdfghjkl|{"
    db 0, 0, "'zxcvbnm,.-", 0, '*', 0, 32
    times 16 db 0
    db '-', 0, 0, 0, '+'
    db 0, 0, 0, 0, 0, 0, 0, '<'
    times 10 db 0

shift_map:
    db 0, 27, '!"#$%&/()', '=', '?', '`', 127, 9
    db "QWERTYUIOP]^", 10, 0, "ASDFGHJKL\["
    db 0, 0, "*ZXCVBNM;:_", 0, '*', 0, 32
    times 16 db 0
    db '-', 0, 0, 0, '+'
    db 0, 0, 0, 0, 0, 0, 0, '>'
    times 10 db 0

alt_map:
    db 0, 0, 0, '@', 0, '$', 0, 0, '{', '[', ']', '}', '\', 0
    db 0, 0
    times 11 db 0
    db '~', 10, 0
    times 11 db 0
    db 0, 0
    times 11 db 0
    db 0, 0, 0, 0
    times 16 db 0
    times 5 db 0
    db 0, 0, 0, 0, 0, 0, 0, '|'
    times 10 db 0

do_self:
    lea     rbx, [alt_map]
    test    byte [mode], 0x20
    jnz     .lookup
    lea     rbx, [shift_map]
    test    byte [mode], 0x03
    jnz     .lookup
    lea     rbx, [key_map]
.lookup:
    mov     al, [rbx + rax]
    or      al, al
    jz      none
    test    byte [mode], 0x4c
    jz      .check_ctrl
    cmp     al, 'a'
    jb      .check_ctrl
    cmp     al, 'z'
    ja      .check_ctrl
    sub     al, 32
.check_ctrl:
    test    byte [mode], 0x0c
    jz      .check_alt
    cmp     al, 64
    jb      .check_alt
    cmp     al, 96
    jae     .check_alt
    sub     al, 64
.check_alt:
    test    byte [mode], 0x10
    jz      .put
    or      al, 0x80
.put:
    and     rax, 0xff
    xor     rbx, rbx
    call    put_queue
none:
    ret

minus:
    cmp     byte [e0], 1
    jne     do_self
    mov     rax, '/'
    xor     rbx, rbx
    jmp     put_queue

; Key table - 256 entries
key_table:
    dq none, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, ctrl, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, lshift, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, do_self, do_self, do_self
    dq do_self, minus, rshift, do_self
    dq alt, do_self, caps, func
    dq func, func, func, func
    dq func, func, func, func
    dq func, num, scroll, cursor
    dq cursor, cursor, do_self, cursor
    dq cursor, cursor, do_self, cursor
    dq cursor, cursor, cursor, cursor
    dq none, none, do_self, func
    dq func, none, none, none
    times 168 dq none           ; Fill rest with none

kb_wait:
    push    rax
.loop:
    in      al, 0x64
    test    al, 0x02
    jnz     .loop
    pop     rax
    ret

reboot:
    call    kb_wait
    mov     word [0x472], 0x1234
    mov     al, 0xfc
    out     0x64, al
.die:
    jmp     .die
