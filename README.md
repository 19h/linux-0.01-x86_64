# Linux 0.01 - 64-bit Port

This is a port of Linus Torvalds' original Linux 0.01 kernel (1991) to modern 64-bit x86_64 architecture.

## Original Source

Historical 0.01 release of linux kernel taken from ftp://ftp.kernel.org/pub/linux/kernel/Historic/

## 64-bit Port Features

- Full 64-bit long mode support
- 4-level page tables (PML4 -> PDPT -> PD -> PT)
- NASM assembler (replaces GNU as)
- Modern x86_64-elf-gcc cross-compiler
- Built-in shell for testing without filesystem
- Serial console output

## Building

Requirements:
- x86_64-elf-gcc cross-compiler
- NASM assembler
- QEMU for testing

```bash
make clean
make
```

## Running

```bash
make run
```

This boots the kernel in QEMU with serial console connected to your terminal.
Press Ctrl-C to exit. The kernel starts a built-in shell with the following commands:

- `help`   - Show available commands
- `uname`  - Show system information
- `ps`     - Show running processes
- `free`   - Show memory information
- `uptime` - Show system uptime
- `reboot` - Reboot the system

## Architecture Changes from Original

### Boot Process (boot/head.nasm)
- 32-bit protected mode -> 64-bit long mode transition
- 4-level page table setup (identity maps first 16MB)
- 64-bit GDT with kernel/user code/data segments
- 64-bit IDT with 16-byte interrupt gates

### Memory Management (mm/memory.c)
- 64-bit page table traversal (PML4/PDPT/PD/PT)
- 64-bit physical addresses
- Updated page fault handlers

### Interrupt Handling (kernel/system_call.nasm)
- 64-bit interrupt frame (RIP, CS, RFLAGS, RSP, SS)
- Full register save/restore (RAX-R15, segment regs)
- System call via int 0x80

### Process Management
- 64-bit TSS structure (RSP0 only, no saved registers)
- Software task switching via __switch_to
- Thread state in task_struct->thread

### Data Structures
- desc_struct: 8-byte GDT entries
- idt_entry: 16-byte IDT entries  
- tss_struct: 64-bit TSS layout
- All pointers are 64-bit

## File Structure

```
boot/
  head.nasm     - 64-bit startup code, page tables, GDT/IDT
  boot.s        - 16-bit boot sector (unchanged)
kernel/
  system_call.nasm - System call and interrupt entry points
  switch.nasm   - Context switch implementation
  sched.c       - Scheduler (updated for 64-bit)
  fork.c        - Process creation (updated for 64-bit)
mm/
  memory.c      - Memory management (64-bit page tables)
  page.nasm     - Page fault handler
init/
  main.c        - Kernel initialization and built-in shell
include/
  linux/head.h  - 64-bit descriptor structures
  linux/sched.h - 64-bit task_struct, TSS, thread_struct
  asm/system.h  - 64-bit IDT/TSS macros
```

## Limitations

- No filesystem support in built-in shell
- Runs shell in kernel mode (no user-space isolation demo)
- Memory limited to 8MB (as in original)
- Single-processor only

## License

This code follows the original Linux 0.01 license terms.
