#!/bin/bash
# Run Linux 0.01 64-bit kernel with proper terminal handling
# Press Ctrl-A then X to exit QEMU

cd "$(dirname "$0")"

if [ ! -f Image ]; then
    echo "Building kernel..."
    make || exit 1
fi

echo "Starting Linux 0.01 64-bit..."
echo "Press Ctrl-A then X to exit"
echo ""

# Use script to ensure proper PTY allocation
exec script -q /dev/null qemu-system-x86_64 -drive file=Image,format=raw,if=floppy -nographic
