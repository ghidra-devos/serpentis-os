@echo off
setlocal EnableExtensions

if "%FASM%"=="" set FASM=C:\K_OS\FASM.EXE

if not exist "%FASM%" (
    echo FASM was not found at %FASM%
    echo set FASM to the full path of FASM.EXE
    exit /b 1
)

if not exist build mkdir build

echo Building boot64.asm
"%FASM%" boot\boot64.asm build\boot64.bin
if errorlevel 1 goto failed

for %%A in ("build\boot64.bin") do set BOOT_BYTES=%%~zA
if not "%BOOT_BYTES%"=="512" (
    echo Invalid boot size: %BOOT_BYTES%
    goto failed
)

echo Building loader64.asm
"%FASM%" boot\loader64.asm build\loader64.bin
if errorlevel 1 goto failed

for %%A in ("build\loader64.bin") do set LOADER_BYTES=%%~zA
if not "%LOADER_BYTES%"=="7680" (
    echo Invalid loader size: %LOADER_BYTES%
    goto failed
)

echo Building kernel64.asm
"%FASM%" kernel\kernel64.asm build\kernel64.bin
if errorlevel 1 goto failed

for %%A in ("build\kernel64.bin") do set KERNEL_BYTES=%%~zA
if %KERNEL_BYTES% GTR 524288 (
    echo Kernel is too large: %KERNEL_BYTES% bytes
    echo Maximum: 524288 bytes
    goto failed
)

if not exist build\bin mkdir build\bin
del /q build\bin\*.serp 2>nul

for /d %%D in (apps\* assembler tests\programs) do (
    for %%F in (%%D\*.asm) do (
        echo Building %%~nF.serp
        "%FASM%" %%F build\bin\%%~nF.serp
        if errorlevel 1 goto failed
    )
)

call tools\serpcc.bat --library >nul 2>nul
if errorlevel 1 (
    echo Skipping C programs: clang was not found
) else (
    for %%F in (examples\c\*.c) do (
        echo Building %%~nF.serp from C
        call tools\serpcc.bat %%F -o build\bin\%%~nF.serp >nul
        if errorlevel 1 goto failed
    )
    echo Building cc.serp, the C compiler
    call tools\serpcc.bat compiler\main.c compiler\tokenize.c compiler\preprocess.c compiler\type.c compiler\parse.c compiler\gen.c -o build\bin\cc.serp >nul
    if errorlevel 1 goto failed
)

echo Creating wallpaper
python tools\make_wallpaper.py
if errorlevel 1 goto failed

echo Creating disk images
python tools\make_image.py
if errorlevel 1 goto failed
python tools\make_image.py --tests
if errorlevel 1 goto failed

echo.
echo serpentis-os build completed.
echo Kernel bytes: %KERNEL_BYTES%
exit /b 0

:failed
echo.
echo Build failed.
exit /b 1
