#!/bin/bash
# Run Linux 0.01 64-bit kernel

cd "$(dirname "$0")"

if [ ! -f Image ]; then
    echo "Building kernel..."
    make || exit 1
fi

echo "============================================"
echo "  Linux 0.01 - 64-bit Port"
echo "============================================"
echo ""
echo "Type 'help' for commands."
echo "Press Ctrl-A then X to exit QEMU."
echo ""

# Use expect if available for best experience
if command -v expect &>/dev/null; then
    expect -c '
        set timeout -1
        spawn qemu-system-x86_64 -drive file=Image,format=raw,if=floppy -nographic
        interact
    '
else
    # Fallback: direct QEMU (may have input issues on some terminals)
    exec qemu-system-x86_64 -drive file=Image,format=raw,if=floppy -nographic
fi
