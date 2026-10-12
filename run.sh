#!/bin/sh
QEMU=${QEMU:-qemu-system-x86_64}
[ -f build/serpentis-os.img ] || sh build.sh || exit 1
ACCEL="-accel tcg"
[ -w /dev/kvm ] && ACCEL="-accel kvm"
exec "$QEMU" -machine pc -cpu qemu64 $ACCEL -m 512M \
    -drive file=build/serpentis-os.img,format=raw,if=ide,index=0 \
    -boot c -no-shutdown
