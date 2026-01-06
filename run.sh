#!/usr/bin/expect -f
# Run Linux 0.01 64-bit kernel interactively

set timeout -1

# Change to script directory
cd [file dirname [info script]]

# Build if needed
if {![file exists "Image"]} {
    puts "Building kernel..."
    exec make
}

# Kill any old QEMU
catch {exec pkill -9 qemu-system-x86}
after 500

puts "============================================"
puts "  Linux 0.01 - 64-bit Port"
puts "============================================"
puts ""
puts "Press Ctrl-] to exit"
puts ""

# Spawn QEMU directly with serial on stdio
spawn qemu-system-x86_64 -drive file=Image,format=raw,if=floppy -display none -serial stdio

# Interactive mode - pass all input/output through
interact {
    \035 {puts "\nExiting..."; exit}
}
