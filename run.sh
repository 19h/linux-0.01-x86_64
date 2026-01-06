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

# Start QEMU with serial on TCP port (in background, don't wait)
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

# Wait for QEMU to be ready
sleep 1

# Use expect for proper interactive terminal handling
# Convert \n (Enter key) to \r (carriage return) for the serial console
exec expect -c "
    set timeout -1
    spawn nc 127.0.0.1 $PORT
    interact {
        \"\\n\" {send \"\\r\"}
        \"\\r\" {send \"\\r\"}
        \003 {exit}
    }
"
