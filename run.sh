#!/bin/bash
# Run Linux 0.01 64-bit kernel

cd "$(dirname "$0")"

if [ ! -f Image ]; then
    echo "Building kernel..."
    make || exit 1
fi

PORT=4321

# Kill any existing QEMU on this port
pkill -f "tcp::$PORT" 2>/dev/null
sleep 0.5

echo "============================================"
echo "  Linux 0.01 - 64-bit Port"  
echo "============================================"
echo ""

# Start QEMU with serial on TCP port (in background)
qemu-system-x86_64 \
    -drive file=Image,format=raw,if=floppy \
    -display none \
    -serial tcp::$PORT,server=on,wait=on &

QEMU_PID=$!

# Cleanup on exit
cleanup() {
    kill $QEMU_PID 2>/dev/null
    wait $QEMU_PID 2>/dev/null
}
trap cleanup EXIT INT TERM

# Give QEMU a moment to start listening
sleep 0.5

echo "Type 'help' for commands. Press Ctrl-C to exit."
echo ""

# Connect interactively via nc
# The expect wrapper ensures proper terminal handling
if command -v expect &>/dev/null; then
    expect -c "
        set timeout -1
        spawn nc 127.0.0.1 $PORT
        interact
    "
else
    # Fallback to plain nc
    nc 127.0.0.1 $PORT
fi
