@echo off
setlocal EnableExtensions EnableDelayedExpansion

set ROOT=%~dp0..
set LIBC=%ROOT%\libc
set WORK=%ROOT%\build\c
set CFLAGS=--target=x86_64-unknown-none-elf -ffreestanding -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -O2 -Wall -Wextra -D__serpentis__ -I"%LIBC%\include"

set B=
if not "%SERP_CLANG%"=="" set B=%SERP_CLANG%\
where "%B%clang" >nul 2>nul || if not exist "%B%clang.exe" (
    echo clang was not found, put it on the PATH or set SERP_CLANG to its folder
    exit /b 1
)

if not exist "%WORK%" mkdir "%WORK%"

if "%~1"=="--library" goto library
if "%~1"=="" goto usage

if not exist "%WORK%\libserp.a" call :build_library || exit /b 1

set OUTPUT=
set OBJECTS=
:arguments
if "%~1"=="" goto link
if "%~1"=="-o" (
    set OUTPUT=%~2
    shift
    shift
    goto arguments
)
set OBJECT=%WORK%\%~n1.o
"%B%clang" %CFLAGS% -c "%~1" -o "!OBJECT!" || exit /b 1
set OBJECTS=!OBJECTS! "!OBJECT!"
if "!OUTPUT!"=="" set OUTPUT=%~dpn1.serp
shift
goto arguments

:link
"%B%ld.lld" -m elf_x86_64 -T "%LIBC%\serpentis.ld" -static -nostdlib --gc-sections -o "%WORK%\program.elf" "%WORK%\start.o" %OBJECTS% "%WORK%\libserp.a" || exit /b 1
"%B%llvm-objcopy" -O binary "%WORK%\program.elf" "%OUTPUT%" || exit /b 1
echo Built %OUTPUT%
exit /b 0

:library
call :build_library
exit /b %errorlevel%

:build_library
for %%F in (start string ctype stdlib stdio graphics setjmp time math) do (
    "%B%clang" %CFLAGS% -c "%LIBC%\%%F.c" -o "%WORK%\%%F.o" || exit /b 1
)
if exist "%WORK%\libserp.a" del "%WORK%\libserp.a"
"%B%llvm-ar" rcs "%WORK%\libserp.a" "%WORK%\string.o" "%WORK%\ctype.o" "%WORK%\stdlib.o" "%WORK%\stdio.o" "%WORK%\graphics.o" "%WORK%\setjmp.o" "%WORK%\time.o" "%WORK%\math.o" || exit /b 1
exit /b 0

:usage
echo Usage: serpcc file.c [more.c ...] [-o program.serp]
echo        serpcc --library
exit /b 1
