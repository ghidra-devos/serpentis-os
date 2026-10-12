#!/bin/sh
# same as serpcc.bat. CLANG_BIN can point at a folder with clang, ld.lld,
# llvm-ar and llvm-objcopy, otherwise they come from the PATH
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIBC=$ROOT/libc
WORK=$ROOT/build/c
B=${CLANG_BIN:+$CLANG_BIN/}
CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -O2 -Wall -Wextra -D__serpentis__ -I$LIBC/include"

if ! command -v "${B}clang" >/dev/null 2>&1; then
    echo "clang was not found, install it or set CLANG_BIN"
    exit 1
fi
mkdir -p "$WORK"

library() {
    for f in start string ctype stdlib stdio graphics setjmp time math; do
        "${B}clang" $CFLAGS -c "$LIBC/$f.c" -o "$WORK/$f.o"
    done
    rm -f "$WORK/libserp.a"
    "${B}llvm-ar" rcs "$WORK/libserp.a" "$WORK/string.o" "$WORK/ctype.o" "$WORK/stdlib.o" "$WORK/stdio.o" "$WORK/graphics.o" "$WORK/setjmp.o" "$WORK/time.o" "$WORK/math.o"
}

case "$1" in
--library) library; exit 0 ;;
"") echo "usage: serpcc.sh file.c [more.c ...] [-o program.serp]"; echo "       serpcc.sh --library"; exit 1 ;;
esac

[ -f "$WORK/libserp.a" ] || library

OUTPUT=
OBJECTS=
while [ $# -gt 0 ]; do
    if [ "$1" = -o ]; then
        OUTPUT=$2
        shift 2
        continue
    fi
    obj=$WORK/$(basename "$1" .c).o
    "${B}clang" $CFLAGS -c "$1" -o "$obj"
    OBJECTS="$OBJECTS $obj"
    [ -n "$OUTPUT" ] || OUTPUT=${1%.c}.serp
    shift
done

"${B}ld.lld" -m elf_x86_64 -T "$LIBC/serpentis.ld" -static -nostdlib --gc-sections -o "$WORK/program.elf" "$WORK/start.o" $OBJECTS "$WORK/libserp.a"
"${B}llvm-objcopy" -O binary "$WORK/program.elf" "$OUTPUT"
echo "built $OUTPUT"
