; the abi: syscall numbers, error codes, key codes, structs shared with programs.
; this file is copied to /lib/defs.inc so programs built inside the os see the same numbers

; int 0x80, rax = number. the holes are calls that got removed, numbers are not reused
SYS_EXIT            = 0
SYS_WRITE           = 1
SYS_READ            = 2
SYS_OPEN            = 3
SYS_CLOSE           = 4
SYS_SEEK            = 5
SYS_STAT            = 6
SYS_READDIR         = 7
SYS_MKDIR           = 8
SYS_UNLINK          = 9
SYS_RENAME          = 10
SYS_SYNC            = 11
SYS_GETKEY          = 12
SYS_SPAWN           = 15
SYS_GETPID          = 17
SYS_CHDIR           = 28
SYS_GETCWD          = 29
SYS_GETTICKS        = 30
SYS_YIELD           = 31
SYS_WAITPID         = 32

WAIT_NOHANG         = 1

SYS_CON_INFO        = 33
SYS_CON_DRAW        = 34
SYS_CON_RESTORE     = 35
SYS_GETKEY_NOWAIT   = 1
SYS_FB_BLIT         = 36
SYS_EVENT           = 37
SYS_SEND            = 38
SYS_SHM_ATTACH      = 39
SYS_SERVICE         = 40
SYS_TTY_OPEN        = 41
SYS_TTY_READ        = 42
SYS_TTY_INPUT       = 43
SYS_KCOMMAND        = 44
SYS_LOG             = 45
SYS_DUP2            = 46
SYS_ALLOC           = 47
SYS_SYSINFO         = 48
SYS_KILL            = 49
SYS_PIPE            = 50
SYS_TIME            = 51
SYS_THEME           = 52
SYS_PALETTE         = 53
SYS_ISATTY          = 54
SYS_GETUID          = 55
SYS_SPAWNAS         = 56
SYS_CHMOD           = 57
SYS_CHOWN           = 58
SYS_PASSWD          = 59
PALETTE_FREE_FIRST  = 48

SHM_ADDRESS         = 0x1E200000

THEME_CURRENT       = 0
THEME_SET           = 1
THEME_NAME          = 2
THEME_PALETTE       = 3
THEME_NAME_MAX      = 12
THEME_PALETTE_BYTES = 48

TIME_SECOND         = 0
TIME_MINUTE         = 1
TIME_HOUR           = 2
TIME_DAY            = 3
TIME_MONTH          = 4
TIME_YEAR           = 6
TIME_BYTES          = 8
KEY_INTERRUPT       = 3
KEY_END_OF_INPUT    = 4

KEY_UP              = 0x101
KEY_DOWN            = 0x102
KEY_LEFT            = 0x103
KEY_RIGHT           = 0x104
KEY_HOME            = 0x105
KEY_END             = 0x106
KEY_PAGE_UP         = 0x107
KEY_PAGE_DOWN       = 0x108
KEY_DELETE          = 0x109
KEY_INSERT          = 0x10A
KEY_SHIFT_ENTER     = 0x10B
KEY_F1              = 0x111
KEY_CTRL            = 0x1000
KEY_SUPER           = 0x2000
MOUSE_SUPER         = 1

FD_STDIN            = 0
FD_STDOUT           = 1
FD_STDERR           = 2

OPEN_READ           = 1
OPEN_WRITE          = 2
OPEN_CREATE         = 0x40      ; same bit as linux O_CREAT, easier to remember

USER_STAT_SIZE      = 24
USER_STAT_FILE_SIZE = 0
USER_STAT_FLAGS     = 8
USER_STAT_MTIME     = 12
USER_STAT_OWNER     = 16
USER_STAT_MODE      = 20

SEEK_SET            = 0
SEEK_CUR            = 1
SEEK_END            = 2

SYS_DIRENT_NAME     = 0
SYS_DIRENT_NAME_MAX = 240
SYS_DIRENT_FLAGS    = 240
SYS_DIRENT_SIZE     = 244
SYS_DIRENT_ENTRY    = 248
SYS_DIRENT_OWNER    = 252
SYS_DIRENT_MODE     = 254
SYS_DIRENT_BYTES    = 256

SYS_FILE_FLAG       = 1
SYS_DIR_FLAG        = 2

SYS_STAT_SIZE       = 24

SYS_OK              = 0
SYS_ERR_INVALID     = -1
SYS_ERR_NOT_FOUND   = -2
SYS_ERR_NO_MEMORY   = -3
SYS_ERR_ACCESS      = -4
SYS_ERR_EXISTS      = -5
SYS_ERR_NOT_DIRECTORY = -6
SYS_ERR_IS_DIRECTORY  = -7
SYS_ERR_INVALID_ADDRESS = -8
SYS_ERR_NO_PROCESS  = -9
SYS_ERR_BAD_FORMAT  = -10
SYS_ERR_IO          = -11
SYS_ERR_BUSY        = -12
SYS_ERR_FULL        = -13
