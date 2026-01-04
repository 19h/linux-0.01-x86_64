;
; boot.s - 64-bit Linux 0.01 bootloader
;
; This bootloader:
; 1. Loads the kernel at 0x10000
; 2. Moves it to 0x100000 (1MB mark)
; 3. Sets up GDT for 32-bit protected mode
; 4. Jumps to head.s which will transition to 64-bit long mode
;
; NOTE: The 64-bit transition happens in head.s because
; we need more space than 512 bytes for page tables setup.
;

[BITS 16]
[ORG 0]

; 1.44MB floppy: 18 sectors per track
sectors equ 18

; SYSSIZE is set by Makefile based on kernel size
%ifndef SYSSIZE
SYSSIZE equ 0x7F00
%endif

BOOTSEG equ 0x07c0
INITSEG equ 0x9000
SYSSEG  equ 0x1000          ; Load kernel at 0x10000
ENDSEG  equ SYSSEG + SYSSIZE

_start:
    mov     ax, BOOTSEG
    mov     ds, ax
    mov     ax, INITSEG
    mov     es, ax
    mov     cx, 256
    xor     si, si
    xor     di, di
    rep     movsw
    jmp     INITSEG:go

go:
    mov     ax, cs
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x400

    ; Display loading message
    mov     ah, 0x03
    xor     bh, bh
    int     0x10

    mov     cx, msg1_len
    mov     bx, 0x0007
    mov     bp, msg1
    mov     ax, 0x1301
    int     0x10

    ; Load system at 0x10000
    ; Load system at 0x10000
    mov     ax, SYSSEG
    mov     es, ax
    call    read_it
    call    kill_motor

    ; Save cursor position
    mov     ah, 0x03
    xor     bh, bh
    int     0x10
    mov     [510], dx

    ; Disable interrupts for mode switch
    cli

    ; Move system from 0x10000 to 0x100000 (1MB)
    ; We need to use 32-bit addressing, so we'll do this in protected mode
    ; For now, we do a simple copy in real mode using unreal mode trick
    
    ; First, move system to 0x0 temporarily (will be overwritten by page tables)
    ; Actually, let's keep it simple and move in head.s after we're in protected mode
    
    ; Enable A20 line
    call    empty_8042
    mov     al, 0xD1
    out     0x64, al
    call    empty_8042
    mov     al, 0xDF
    out     0x60, al
    call    empty_8042

    ; Reprogram PICs
    mov     al, 0x11
    out     0x20, al
    jmp     short $+2
    jmp     short $+2
    out     0xA0, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0x20        ; IRQ 0-7 -> INT 0x20-0x27
    out     0x21, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0x28        ; IRQ 8-15 -> INT 0x28-0x2F
    out     0xA1, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0x04
    out     0x21, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0x02
    out     0xA1, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0x01
    out     0x21, al
    jmp     short $+2
    jmp     short $+2
    out     0xA1, al
    jmp     short $+2
    jmp     short $+2
    mov     al, 0xFF        ; Mask all interrupts
    out     0x21, al
    jmp     short $+2
    jmp     short $+2
    out     0xA1, al

    ; Load GDT and enter protected mode
    lgdt    [gdt_ptr]
    
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax

    ; Jump to 32-bit code at 0x10000 (where kernel was loaded)
    jmp     dword 0x08:0x10000

; Wait for keyboard controller
empty_8042:
    jmp     short $+2
    jmp     short $+2
    in      al, 0x64
    test    al, 2
    jnz     empty_8042
    ret

; Disk read routines
; Variables (in relocated boot sector at 0x90000)
sread:  dw 1                ; sectors read of current track (starts at 1, sector after boot)
head:   dw 0                ; current head
track:  dw 0                ; current track

;
; read_it - Read kernel from floppy to memory at ES:0
; Entry: ES = starting segment (0x1000)
; Uses:  ES, BX for destination, reads until ES >= ENDSEG
;
read_it:
    mov     ax, es
    test    ax, 0x0fff      ; ES must be at 64KB boundary
.die:
    jnz     .die
    xor     bx, bx          ; BX = offset within segment (starts at 0)

.rp_read:
    ; Check if we've loaded enough
    mov     ax, es
    cmp     ax, ENDSEG
    jb      .ok1_read
    
    ret                     ; Done loading

.ok1_read:
    ; Calculate sectors to read
    ; AX = sectors remaining on current track
    mov     ax, sectors     ; 18 sectors per track
    sub     ax, [sread]     ; minus sectors already read
    
    ; Check if reading this many would overflow the segment
    mov     cx, ax
    shl     cx, 9           ; CX = bytes to read
    add     cx, bx          ; CX = ending offset
    jnc     .ok2_read       ; No overflow, use calculated count
    je      .ok2_read       ; Exactly at boundary is OK
    
    ; Would overflow - calculate sectors to reach segment boundary
    xor     ax, ax
    sub     ax, bx          ; AX = bytes remaining in segment
    shr     ax, 9           ; AX = sectors that fit

.ok2_read:
    ; AX = number of sectors to read (must be > 0)
    test    ax, ax
    jz      .next_segment
    
    call    read_track      ; Read sectors, returns actual count in AL
    
    ; Update position
    mov     cx, ax          ; CX = sectors actually read
    add     ax, [sread]     ; Update sread
    cmp     ax, sectors     ; Finished this track?
    jne     .ok3_read
    
    ; Move to next track
    mov     ax, 1
    sub     ax, [head]      ; Toggle head (0->1 or 1->0)
    jne     .ok4_read
    inc     word [track]    ; If head wrapped to 0, next cylinder
.ok4_read:
    mov     [head], ax
    xor     ax, ax          ; Reset sread to 0 for new track
    


.ok3_read:
    mov     [sread], ax
    
    ; Advance buffer pointer
    shl     cx, 9           ; CX = bytes read
    add     bx, cx          ; BX += bytes
    jnc     .rp_read        ; If no overflow, continue in same segment

.next_segment:
    ; Move to next 64KB segment
    mov     ax, es
    add     ax, 0x1000      ; Next segment
    mov     es, ax
    xor     bx, bx          ; Reset offset
    jmp     .rp_read

;
; read_track - Read sectors from floppy
; Entry: AL = number of sectors to read
; Exit:  AL = number of sectors actually read
; Uses:  Reads to ES:BX
;
read_track:
    push    bx
    push    cx
    push    dx
    push    si
    
    mov     si, ax          ; SI = save sector count for retry
    
.retry:
    mov     ax, si          ; AL = sectors to read
    mov     ah, 2           ; AH = BIOS read function
    
    mov     dx, [track]
    mov     cx, [sread]
    inc     cx              ; CL = starting sector (1-based)
    mov     ch, dl          ; CH = cylinder/track number
    
    mov     dx, [head]
    mov     dh, dl          ; DH = head
    mov     dl, 0           ; DL = drive 0
    
    int     0x13
    jc      .error
    
    ; Success - AL contains sectors read
    pop     si
    pop     dx
    pop     cx
    pop     bx
    ret

.error:
    ; Reset disk controller and retry
    push    bx              ; Save BX before reset
    xor     ax, ax
    xor     dx, dx
    int     0x13
    pop     bx              ; Restore BX after reset
    jmp     .retry

kill_motor:
    push    dx
    mov     dx, 0x3f2
    xor     al, al
    out     dx, al
    pop     dx
    ret

; GDT for initial protected mode entry
; We'll set up the real 64-bit GDT in head.s
align 8
gdt:
    dq 0x0000000000000000   ; Null descriptor
    dq 0x00CF9A000000FFFF   ; 32-bit code segment, base 0, limit 4GB
    dq 0x00CF92000000FFFF   ; 32-bit data segment, base 0, limit 4GB

gdt_ptr:
    dw $ - gdt - 1          ; GDT limit
    dd gdt + 0x90000        ; GDT base (relocated to 0x90000)

msg1:
    db 13, 10, "Loading 64-bit system...", 13, 10, 13, 10
msg1_len equ $ - msg1

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
