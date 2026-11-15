; start of every program built on the host with fasm. writes the SERP header by hand and calls
; main. inside the os apple writes the header itself and programs include /lib/start.inc instead

include '../../config.inc'
include '../../include/syscall.inc'
include '../../include/executable.inc'

if ~ defined SERP_STACK | defined serp_default_stack
    serp_default_stack = 1
    SERP_STACK = 0x1000
end if

format binary as 'serp'
use64
org USER_CODE_ADDRESS - SERP_HEADER_SIZE

    dd SERP_MAGIC
    dw SERP_VERSION
    dw SERP_FLAG_USER
    dq SERP_HEADER_SIZE
    dq _start - USER_CODE_ADDRESS
    dq SERP_HEADER_SIZE
    dq serp_code_end - USER_CODE_ADDRESS
    dq 0
    dq 0
    dq serp_bss_end - serp_code_end
    dq SERP_STACK
    dq (serp_code_end - USER_CODE_ADDRESS + 0xFFF) shr 12

_start:
    call main
    mov edi, eax
    call sys_exit

postpone
{
    serp_bss_end:
}
