#!/bin/sh
set -e

FASM=${FASM:-fasm}
if ! command -v "$FASM" >/dev/null 2>&1; then
    echo "fasm was not found, put it on the PATH or set FASM"
    exit 1
fi

size() { wc -c < "$1" | tr -d ' '; }

mkdir -p build/bin

echo "building boot64.asm"
"$FASM" boot/boot64.asm build/boot64.bin >/dev/null
if [ "$(size build/boot64.bin)" != 512 ]; then
    echo "boot sector is $(size build/boot64.bin) bytes, has to be 512"
    exit 1
fi

echo "building loader64.asm"
"$FASM" boot/loader64.asm build/loader64.bin >/dev/null
if [ "$(size build/loader64.bin)" != 7680 ]; then
    echo "loader is $(size build/loader64.bin) bytes, has to be 7680"
    exit 1
fi

echo "building kernel64.asm"
"$FASM" kernel/kernel64.asm build/kernel64.bin >/dev/null
KERNEL_BYTES=$(size build/kernel64.bin)
if [ "$KERNEL_BYTES" -gt 524288 ]; then
    echo "kernel is $KERNEL_BYTES bytes, the loader reads at most 524288"
    exit 1
fi

rm -f build/bin/*.serp
for f in apps/*/*.asm assembler/*.asm tests/programs/*.asm; do
    [ -f "$f" ] || continue
    name=$(basename "$f" .asm)
    echo "building $name.serp"
    "$FASM" "$f" "build/bin/$name.serp" >/dev/null
done

if sh tools/serpcc.sh --library >/dev/null 2>&1; then
    for f in examples/c/*.c; do
        name=$(basename "$f" .c)
        echo "building $name.serp from C"
        sh tools/serpcc.sh "$f" -o "build/bin/$name.serp" >/dev/null
    done
    echo "building cc.serp, the C compiler"
    sh tools/serpcc.sh compiler/main.c compiler/tokenize.c compiler/preprocess.c compiler/type.c compiler/parse.c compiler/gen.c -o build/bin/cc.serp >/dev/null
else
    echo "skipping the C programs, clang was not found"
fi

PY=${PYTHON:-python3}
command -v "$PY" >/dev/null 2>&1 || PY=python
echo "making the wallpaper"
"$PY" tools/make_wallpaper.py
echo "making the disk images"
"$PY" tools/make_image.py
"$PY" tools/make_image.py --tests

echo
echo "done, kernel is $KERNEL_BYTES bytes"
