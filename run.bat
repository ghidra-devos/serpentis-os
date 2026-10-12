@echo off
setlocal

if "%QEMU%"=="" set QEMU=qemu-system-x86_64
where "%QEMU%" >nul 2>nul || if not exist "%QEMU%" (
    echo qemu was not found, put it on the PATH or set QEMU
    exit /b 1
)

if not exist build\serpentis-os.img (
    call build.bat
    if errorlevel 1 exit /b 1
)

"%QEMU%" ^
    -machine pc ^
    -cpu qemu64 ^
    -accel whpx -accel tcg ^
    -m 512M ^
    -drive file=build\serpentis-os.img,format=raw,if=ide,index=0 ^
    -boot c ^
    -no-shutdown
