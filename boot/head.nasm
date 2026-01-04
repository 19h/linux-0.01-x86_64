;
; head.s - 64-bit startup code for Linux 0.01
;
; Entry: 32-bit protected mode from boot sector
; Exit: 64-bit long mode, calls main()
;
; Memory layout:
;   0x00000 - 0x00FFF: Reserved (real mode IVT, BDA)
;   0x01000 - 0x01FFF: PML4 (Page Map Level 4)
;   0x02000 - 0x02FFF: PDPT (Page Directory Pointer Table)
;   0x03000 - 0x03FFF: PD (Page Directory) 
;   0x04000 - 0x0BFFF: PT0-PT7 (8 Page Tables for 16MB with 4KB pages)
;   0x0C000 - 0x0FFFF: GDT and other data
;   0x10000 - onwards: Kernel code (this file)
;

; We start in 32-bit protected mode
[BITS 32]

section .text

global startup_32, idt, gdt, pg_dir

extern stack_start
extern main

; Page table addresses - using 4KB pages
PML4_ADDR   equ 0x1000
PDPT_ADDR   equ 0x2000
PD_ADDR     equ 0x3000
PT0_ADDR    equ 0x4000          ; PT for 0-2MB
PT1_ADDR    equ 0x5000          ; PT for 2-4MB
PT2_ADDR    equ 0x6000          ; PT for 4-6MB
PT3_ADDR    equ 0x7000          ; PT for 6-8MB
PT4_ADDR    equ 0x8000          ; PT for 8-10MB
PT5_ADDR    equ 0x9000          ; PT for 10-12MB
PT6_ADDR    equ 0xA000          ; PT for 12-14MB
PT7_ADDR    equ 0xB000          ; PT for 14-16MB
GDT_PHYS    equ 0xC000          ; GDT at fixed physical address

; MSR numbers
MSR_EFER    equ 0xC0000080

; EFER bits
EFER_LME    equ 0x100       ; Long Mode Enable
EFER_LMA    equ 0x400       ; Long Mode Active

; CR0 bits
CR0_PG      equ 0x80000000  ; Paging
CR0_PE      equ 0x00000001  ; Protection Enable

; CR4 bits
CR4_PAE     equ 0x20        ; Physical Address Extension

; Page entry flags
PG_PRESENT  equ 0x01
PG_WRITE    equ 0x02
PG_USER     equ 0x04
PG_PS       equ 0x80        ; Page Size (2MB pages) - NOT used

startup_32:
    ; Set up 32-bit data segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, 0x9F000    ; Temporary stack
    
continue_boot:
    ; Clear page table area (0x1000 - 0xD000) = 48KB
    mov     edi, PML4_ADDR
    mov     ecx, 0xC000 / 4     ; 48KB / 4 = 12288 dwords
    xor     eax, eax
    rep     stosd
    
    ; Build GDT at fixed physical address (0xC000) with immediate values
    mov     edi, GDT_PHYS
    
    ; Null descriptor (0x00)
    xor     eax, eax
    mov     [edi], eax
    mov     [edi + 4], eax
    
    ; 64-bit code segment (0x08): L=1, D=0, P=1, DPL=0
    mov     dword [edi + 8], 0x0000FFFF
    mov     dword [edi + 12], 0x00AF9A00
    
    ; 64-bit data segment (0x10): P=1, DPL=0
    mov     dword [edi + 16], 0x0000FFFF
    mov     dword [edi + 20], 0x00CF9200
    
    ; 64-bit user code (0x18): L=1, D=0, DPL=3
    mov     dword [edi + 24], 0x0000FFFF
    mov     dword [edi + 28], 0x00AFFA00
    
    ; 64-bit user data (0x20): DPL=3
    mov     dword [edi + 32], 0x0000FFFF
    mov     dword [edi + 36], 0x00CFF200

    ; Set up PML4[0] -> PDPT
    mov     edi, PML4_ADDR
    mov     eax, PDPT_ADDR | PG_PRESENT | PG_WRITE | PG_USER
    mov     [edi], eax
    mov     dword [edi + 4], 0
    
    ; Set up PDPT[0] -> PD
    mov     edi, PDPT_ADDR
    mov     eax, PD_ADDR | PG_PRESENT | PG_WRITE | PG_USER
    mov     [edi], eax
    mov     dword [edi + 4], 0

    ; Set up PD[0-7] -> PT0-PT7 (8 page tables for 16MB)
    mov     edi, PD_ADDR
    mov     eax, PT0_ADDR | PG_PRESENT | PG_WRITE | PG_USER
    mov     ecx, 8
.pd_loop:
    mov     [edi], eax
    mov     dword [edi + 4], 0
    add     eax, 0x1000         ; Next page table
    add     edi, 8
    dec     ecx
    jnz     .pd_loop

    ; Fill all 8 page tables with 4KB page entries
    ; PT0: maps 0x000000 - 0x1FFFFF (0-2MB)
    ; PT1: maps 0x200000 - 0x3FFFFF (2-4MB)
    ; ... etc
    mov     edi, PT0_ADDR       ; Start of page tables
    mov     eax, PG_PRESENT | PG_WRITE | PG_USER  ; First page at 0x0
    mov     ecx, 8 * 512        ; 8 PTs * 512 entries = 4096 pages
.pt_loop:
    mov     [edi], eax
    mov     dword [edi + 4], 0  ; High 32 bits = 0
    add     eax, 0x1000         ; Next 4KB page
    add     edi, 8
    dec     ecx
    jnz     .pt_loop

    ; Load PML4 address into CR3
    mov     eax, PML4_ADDR
    mov     cr3, eax

    ; Enable PAE in CR4
    mov     eax, cr4
    or      eax, CR4_PAE
    mov     cr4, eax

    ; Enable Long Mode in EFER MSR
    mov     ecx, MSR_EFER
    rdmsr
    or      eax, EFER_LME
    wrmsr

    ; Load 64-bit GDT BEFORE enabling paging
    ; Load address of gdt_ptr_tmp into eax, then lgdt from there
    mov     eax, gdt_ptr_tmp
    lgdt    [eax]

    ; Enable Paging (this activates Long Mode / compatibility mode)
    mov     eax, cr0
    or      eax, CR0_PG
    mov     cr0, eax

    ; Far jump to 64-bit code
    ; In compatibility mode, use direct far jump with 48-bit pointer (m16:32)
    ; NASM: jmp dword 0x08:long_mode_entry should work but may have issues with ELF64
    ; So we use manual bytes: EA <offset32> <selector16>
    
    db      0xEA                        ; JMP FAR ptr16:32 opcode
    dd      long_mode_entry             ; 32-bit offset
    dw      0x0008                      ; 16-bit selector

; GDT pointer for 32-bit code (embedded in text section)
; Points to our fixed physical address copy
align 8
gdt_ptr_tmp:
    dw 63                               ; GDT limit (8 entries * 8 bytes - 1)
    dd GDT_PHYS                         ; GDT base = 0x5000

; ============================================================================
; 64-bit Long Mode Code
; ============================================================================
[BITS 64]

long_mode_entry:
    ; Load the full GDT (gdt64) which has space for TSS and LDTs
    lgdt    [gdt64_ptr]
    
    ; Set up 64-bit data segments
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Set up proper stack
    mov     rsp, stack_start
    
    ; Debug in 64-bit mode: check first byte of main()
    ; Write '6' to show we're in 64-bit mode
    mov     byte [0xB800C], '6'
    mov     byte [0xB800D], 0x0F
    mov     byte [0xB800E], '4'
    mov     byte [0xB800F], 0x0F
    mov     byte [0xB8010], ':'
    mov     byte [0xB8011], 0x0F
    
    ; Read first byte of main
    mov     al, [main]
    ; Print as hex
    mov     cl, al
    shr     cl, 4
    cmp     cl, 10
    jb      .lm_digit1
    add     cl, 'A' - 10
    jmp     .lm_print1
.lm_digit1:
    add     cl, '0'
.lm_print1:
    mov     byte [0xB8012], cl
    mov     byte [0xB8013], 0x0F
    
    mov     cl, al
    and     cl, 0x0F
    cmp     cl, 10
    jb      .lm_digit2
    add     cl, 'A' - 10
    jmp     .lm_print2
.lm_digit2:
    add     cl, '0'
.lm_print2:
    mov     byte [0xB8014], cl
    mov     byte [0xB8015], 0x0F

    ; Clear BSS (if needed)
    ; ... 

    ; A20 gate is already enabled by the boot sector, skip check

    ; Check for x87 FPU
    mov     rax, cr0
    and     rax, ~0x04          ; Clear EM
    or      rax, 0x02           ; Set MP
    mov     cr0, rax
    fninit
    
    ; Set up IDT
    call    setup_idt

    ; Jump to C code
    ; In System V AMD64 ABI, args go in: rdi, rsi, rdx, rcx, r8, r9
    xor     rdi, rdi            ; argc = 0
    xor     rsi, rsi            ; argv = NULL  
    xor     rdx, rdx            ; envp = NULL
    
    ; Push return address for if main returns
    mov     rax, hang
    push    rax
    
    ; Call main
    jmp     main

hang:
    hlt
    jmp     hang

; ============================================================================
; IDT Setup
; ============================================================================
setup_idt:
    ; Fill IDT with 256 entries pointing to ignore_int
    ; 64-bit IDT entry format (16 bytes):
    ;   0-1:   Offset 15:0
    ;   2-3:   Segment selector (0x08 = kernel code)
    ;   4:     IST (0)
    ;   5:     Type/attr (0x8E = present, DPL=0, 64-bit interrupt gate)
    ;   6-7:   Offset 31:16
    ;   8-11:  Offset 63:32
    ;   12-15: Reserved (0)
    
    lea     rdi, [idt]              ; Destination: IDT table
    mov     rcx, 256                ; 256 entries
    lea     rsi, [ignore_int]       ; Handler address

.rp_idt:
    ; Build IDT entry for ignore_int handler
    mov     rax, rsi                ; Get handler address
    
    ; Dword 0: offset 15:0 + selector 0x08 in bits 16-31
    mov     edx, eax                ; offset 15:0
    and     edx, 0xFFFF
    or      edx, 0x00080000         ; selector 0x08 << 16
    mov     dword [rdi], edx
    
    ; Dword 1: offset 31:16 + type/attr
    mov     edx, eax
    shr     edx, 16                 ; offset 31:16
    and     edx, 0xFFFF
    shl     edx, 16
    or      edx, 0x8E00             ; type = 0x8E (64-bit interrupt gate, DPL=0)
    mov     dword [rdi + 4], edx
    
    ; Dword 2: offset 63:32
    mov     rdx, rsi
    shr     rdx, 32
    mov     dword [rdi + 8], edx
    
    ; Dword 3: reserved
    mov     dword [rdi + 12], 0
    
    add     rdi, 16                 ; Next entry
    dec     rcx
    jnz     .rp_idt

    ; Load IDT
    lidt    [idt_ptr]
    ret

; Default interrupt handler
align 16
ignore_int:
    push    rax
    push    rcx
    mov     byte [0xB8000], '!'     ; Show something on screen
    mov     byte [0xB8001], 0x4F    ; White on red
    pop     rcx
    pop     rax
    iretq

; ============================================================================
; Data Section
; ============================================================================
section .data

align 16
gdt64:
    dq 0x0000000000000000       ; 0x00: Null descriptor
    dq 0x00AF9A000000FFFF       ; 0x08: 64-bit code segment (L=1, D=0)
    dq 0x00CF92000000FFFF       ; 0x10: 64-bit data segment
    dq 0x00AFFA000000FFFF       ; 0x18: 64-bit user code (L=1, DPL=3)
    dq 0x00CFF2000000FFFF       ; 0x20: 64-bit user data (DPL=3)
    dq 0x0000000000000000       ; 0x28: Reserved for TSS low
    dq 0x0000000000000000       ; 0x30: Reserved for TSS high (64-bit TSS is 16 bytes)
    times 249 dq 0              ; Space for LDT's
gdt64_end:

; 32-bit GDT pointer for use before long mode is active
gdt64_ptr32:
    dw gdt64_end - gdt64 - 1    ; GDT limit
    dd gdt64                    ; GDT base (32-bit)

; 64-bit GDT pointer for use in long mode
gdt64_ptr:
    dw gdt64_end - gdt64 - 1    ; GDT limit
    dq gdt64                    ; GDT base (64-bit)

align 16
idt:
    times 256 dq 0, 0           ; 256 entries, 16 bytes each
idt_end:

idt_ptr:
    dw idt_end - idt - 1        ; IDT limit (4095 = 256*16 - 1)
    dq idt                      ; IDT base

; Alias for C code
gdt equ gdt64
pg_dir equ PML4_ADDR

section .bss

align 4096
; Stack
resb 8192
stack_top:
