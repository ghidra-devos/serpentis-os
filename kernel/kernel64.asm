format binary
use64
org 0x10000

include '../config.inc'
include '../include/syscall.inc'
include '../include/trap.inc'
include '../include/process.inc'
include '../include/executable.inc'

kernel_entry:
    cli
    cld

    mov rsp, KERNEL_STACK_TOP
    xor rbp, rbp

    call gdt_init
    call idt_init
    call tss_init
    call serial_init

    call fpu_init
    call process_init
    call scheduler_init
    call address_space_init

    call vga_init
    call vga_map_palette
    call vga_load_palette
    call vga_load_art_palette

    mov al, COLOR_BLACK
    call vga_clear

    call ui_draw
    call terminal_init
    call cpu_init
    call random_init
    call keyboard_init
    call speaker_init
    call pic_init
    call pit_init
    call mouse_init
    call ata_init
    call fs_init
    call theme_restore
    call pipe_init
    call ipc_init
    call tty_init

    sti

kernel_init_loop:
    mov rsi, init_path
    xor edx, edx
    call exec_run_path_args
    jc .missing
    call exec_collect_root
    jmp kernel_init_loop
.missing:
    mov rsi, init_missing_text
    call terminal_print_line
.halt:
    hlt
    jmp .halt

init_path db '/bin/login.serp', 0
init_missing_text db 'Cannot start /bin/login.serp', 0

include 'arch/gdt_tss.asm'
include 'process/user_mode.asm'
include 'syscall/syscall.asm'
include 'process/process.asm'
include 'process/scheduler_queue.asm'
include 'process/process_context.asm'
include 'process/process_handles.asm'
include 'process/pipe.asm'
include 'memory/address_space.asm'
include 'graphics/vga12.asm'
include 'graphics/font.asm'
include 'console/strings.asm'
include 'console/numbers.asm'
include 'console/terminal.asm'
include 'graphics/ui.asm'
include 'arch/exceptions.asm'
include 'arch/idt.asm'
include 'interrupts/pic.asm'
include 'interrupts/pit.asm'
include 'arch/cpu.asm'
include 'drivers/rtc.asm'
include 'console/random.asm'
include 'drivers/speaker.asm'
include 'drivers/keyboard.asm'
include 'drivers/mouse.asm'
include 'drivers/ata.asm'
include 'filesystem/filesystem.asm'
include 'filesystem/filesystem_alloc.asm'
include 'filesystem/filesystem_extra.asm'
include 'filesystem/filesystem_tools.asm'
include 'filesystem/filesystem_transfer.asm'
include 'filesystem/access.asm'
include 'syscall/syscall_fs.asm'
include 'syscall/syscall_console.asm'
include 'syscall/syscall_gui.asm'
include 'syscall/syscall_tty.asm'
include 'syscall/syscall_kcommand.asm'
include 'syscall/syscall_info.asm'
include 'syscall/syscall_user.asm'
include 'syscall/sha256.asm'
include 'process/serp_loader.asm'
include 'process/exec.asm'
include 'shell/shell.asm'
include 'shell/commands_core.asm'
include 'shell/commands_system.asm'
include 'shell/commands_tools.asm'
include 'shell/commands_fs.asm'
include 'drivers/serial.asm'

assert MAILBOX_ADDRESS + MAILBOX_STRIDE * PROCESS_MAX <= CAPTURE_ADDRESS
assert CAPTURE_ADDRESS + CAPTURE_BYTES <= TTY_ADDRESS
assert TTY_ADDRESS + TTY_STRIDE * TTY_MAX <= FPU_AREA_ADDRESS
assert FPU_AREA_ADDRESS + FPU_STATE_BYTES * PROCESS_MAX <= FS_FILE_BUFFER_ADDRESS
assert FPU_AREA_ADDRESS and 15 = 0
assert AS_FREE_STACK_ADDRESS + AS_FREE_STACK_SIZE * 8 <= PIPE_AREA_ADDRESS
assert AS_POOL_END <= FS_ENTRIES_ADDRESS
assert FS_BITMAP_SHADOW_ADDRESS + FS_BITMAP_BYTES <= FS_EXTENT_MAP_ADDRESS
assert FS_EXTENT_MAP_ADDRESS + FS_BITMAP_BYTES <= ATA_DMA_BUFFER
assert ATA_DMA_BUFFER + ATA_DMA_SECTORS * 512 <= ATA_PRD_TABLE
assert ATA_PRD_TABLE + 16 <= SHM_PHYSICAL
assert ATA_DMA_BUFFER and 0xFFFF = 0  ; a prd entry must not cross a 64 KB boundary
assert FS_ENTRIES_ADDRESS + FS_ENTRIES_BYTES <= FS_ENTRIES_BACKUP_ADDRESS
assert FS_ENTRIES_BACKUP_ADDRESS + FS_ENTRIES_BYTES <= FS_ENTRIES_SHADOW_ADDRESS
assert FS_ENTRIES_SHADOW_ADDRESS + FS_ENTRIES_BYTES <= FS_BITMAP_ADDRESS
assert FS_ENTRY_EXTENTS + FS_EXTENT_MAX * FS_EXTENT_SIZE <= FS_ENTRY_SIZE
assert FS_ENTRY_PATH + FS_PATH_MAX <= FS_ENTRY_OWNER
assert 1 shl FS_ENTRY_SHIFT = FS_ENTRY_SIZE
assert pipe_table + PIPE_MAX * PIPE_SIZE <= AS_POOL_START
assert PROCESS_MAX <= 32
