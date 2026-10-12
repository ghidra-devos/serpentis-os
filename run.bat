@echo off
setlocal

set QEMU="C:\Program Files\qemu\qemu-system-x86_64.exe"
if not exist %QEMU% (
    echo QEMU was not found at %QEMU%
    exit /b 1
)

if not exist build\serpentis-os.img (
    call build.bat
    if errorlevel 1 exit /b 1
)

%QEMU% ^
    -machine pc ^
    -cpu qemu64 ^
    -accel whpx -accel tcg ^
    -m 512M ^
    -drive file=build\serpentis-os.img,format=raw,if=ide,index=0 ^
    -boot c ^
    -no-shutdown
