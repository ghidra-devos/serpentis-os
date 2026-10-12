#ifndef SERPENTIS_H
#define SERPENTIS_H

#include <stddef.h>
#include <stdint.h>

enum {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_READ = 2,
    SYS_OPEN = 3,
    SYS_CLOSE = 4,
    SYS_SEEK = 5,
    SYS_STAT = 6,
    SYS_READDIR = 7,
    SYS_MKDIR = 8,
    SYS_UNLINK = 9,
    SYS_RENAME = 10,
    SYS_SYNC = 11,
    SYS_GETKEY = 12,
    SYS_SPAWN = 15,
    SYS_GETPID = 17,
    SYS_CHDIR = 28,
    SYS_GETCWD = 29,
    SYS_GETTICKS = 30,
    SYS_YIELD = 31,
    SYS_WAITPID = 32,
    SYS_CON_INFO = 33,
    SYS_CON_DRAW = 34,
    SYS_CON_RESTORE = 35,
    SYS_FB_BLIT = 36,
    SYS_EVENT = 37,
    SYS_SEND = 38,
    SYS_SHM_ATTACH = 39,
    SYS_SERVICE = 40,
    SYS_TTY_OPEN = 41,
    SYS_TTY_READ = 42,
    SYS_TTY_INPUT = 43,
    SYS_KCOMMAND = 44,
    SYS_LOG = 45,
    SYS_DUP2 = 46,
    SYS_ALLOC = 47,
    SYS_SYSINFO = 48,
    SYS_KILL = 49,
    SYS_PIPE = 50,
    SYS_TIME = 51,
    SYS_THEME = 52,
    SYS_PALETTE = 53,
    SYS_ISATTY = 54,
    SYS_GETUID = 55,
    SYS_SPAWNAS = 56,
    SYS_CHMOD = 57,
    SYS_CHOWN = 58,
    SYS_PASSWD = 59
};

enum {
    SERP_OPEN_READ = 1,
    SERP_OPEN_WRITE = 2,
    SERP_OPEN_CREATE = 0x40
};

enum {
    SERP_SEEK_SET = 0,
    SERP_SEEK_CUR = 1,
    SERP_SEEK_END = 2
};

enum {
    SERP_WAIT_NOHANG = 1,
    SERP_KEY_NOWAIT = 1
};

enum {
    SERP_KEY_INTERRUPT = 3,
    SERP_KEY_END_OF_INPUT = 4,
    SERP_KEY_BACKSPACE = 8,
    SERP_KEY_TAB = 9,
    SERP_KEY_ENTER = 10,
    SERP_KEY_ESCAPE = 27,
    SERP_KEY_UP = 0x101,
    SERP_KEY_DOWN = 0x102,
    SERP_KEY_LEFT = 0x103,
    SERP_KEY_RIGHT = 0x104,
    SERP_KEY_HOME = 0x105,
    SERP_KEY_END = 0x106,
    SERP_KEY_PAGE_UP = 0x107,
    SERP_KEY_PAGE_DOWN = 0x108,
    SERP_KEY_DELETE = 0x109,
    SERP_KEY_INSERT = 0x10A,
    SERP_KEY_SHIFT_ENTER = 0x10B,
    SERP_KEY_F1 = 0x111,
    SERP_KEY_CTRL = 0x1000,
    SERP_KEY_SUPER = 0x2000
};

enum {
    SERP_FILE = 1,
    SERP_DIRECTORY = 2
};

enum {
    SERP_ERR_INVALID = -1,
    SERP_ERR_NOT_FOUND = -2,
    SERP_ERR_NO_MEMORY = -3,
    SERP_ERR_ACCESS = -4,
    SERP_ERR_EXISTS = -5,
    SERP_ERR_NOT_DIRECTORY = -6,
    SERP_ERR_IS_DIRECTORY = -7,
    SERP_ERR_INVALID_ADDRESS = -8,
    SERP_ERR_NO_PROCESS = -9,
    SERP_ERR_BAD_FORMAT = -10,
    SERP_ERR_IO = -11,
    SERP_ERR_BUSY = -12,
    SERP_ERR_FULL = -13
};

enum {
    SERP_EVENT_NONE = 0,
    SERP_EVENT_KEY = 1,
    SERP_EVENT_MOUSE = 2,
    SERP_EVENT_MESSAGE = 3,
    SERP_EVENT_TTY = 4,
    SERP_EVENT_TICK = 5
};

enum {
    SERP_EVENT_NOWAIT = 1,
    SERP_EVENT_WANT_TICK = 2
};

enum {
    SERP_SERVICE_QUERY = 0,
    SERP_SERVICE_REGISTER = 1,
    SERP_SERVICE_FONT = 2,
    SERP_SERVICE_CLAIM = 3
};

enum {
    SERP_THEME_CURRENT = 0,
    SERP_THEME_SET = 1,
    SERP_THEME_NAME = 2,
    SERP_THEME_PALETTE = 3
};

enum {
    SERP_SCREEN_WIDTH = 640,
    SERP_SCREEN_HEIGHT = 480,
    SERP_FONT_HEIGHT = 16,
    SERP_PALETTE_FREE = 48,
    SERP_SHM_ADDRESS = 0x1E200000
};

struct serp_stat {
    uint64_t size;
    uint32_t flags;
    uint32_t mtime;
    uint32_t owner;
    uint32_t mode;
};

struct serp_dirent {
    char name[240];
    uint32_t flags;
    uint32_t size;
    uint32_t entry;
    uint16_t owner;
    uint16_t mode;
};

struct serp_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t reserved;
    uint16_t year;
};

struct serp_message {
    uint32_t op;
    uint32_t window;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    char title[40];
};

struct serp_event {
    uint32_t type;
    uint32_t code;
    int32_t x;
    int32_t y;
    union {
        uint32_t buttons;
        struct serp_message message;
    } data;
};

struct serp_sysinfo {
    uint64_t memory_total;
    uint64_t memory_used;
    uint64_t ticks;
    uint64_t processes;
};

#ifdef __scc__
#define serp_call(number, a, b, c) __syscall(number, a, b, c)
#define serp_call4(number, a, b, c, d) __syscall(number, a, b, c, d)
#else
static inline long serp_call(long number, long a, long b, long c)
{
    long result;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c)
                     : "memory");
    return result;
}

static inline long serp_call4(long number, long a, long b, long c, long d)
{
    long result;
    register long fourth __asm__("r10") = d;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c), "r"(fourth)
                     : "memory");
    return result;
}
#endif

static inline __attribute__((noreturn)) void serp_exit(int code)
{
    serp_call(SYS_EXIT, code, 0, 0);
    __builtin_unreachable();
}

static inline long serp_write(int fd, const void *data, size_t length)
{
    return serp_call(SYS_WRITE, fd, (long)data, (long)length);
}

static inline long serp_read(int fd, void *data, size_t length)
{
    return serp_call(SYS_READ, fd, (long)data, (long)length);
}

static inline int serp_open(const char *path, int flags)
{
    return (int)serp_call(SYS_OPEN, (long)path, flags, 0);
}

static inline int serp_close(int fd)
{
    return (int)serp_call(SYS_CLOSE, fd, 0, 0);
}

static inline long serp_seek(int fd, long offset, int whence)
{
    return serp_call(SYS_SEEK, fd, offset, whence);
}

static inline int serp_stat(const char *path, struct serp_stat *info)
{
    return (int)serp_call(SYS_STAT, (long)path, (long)info, 0);
}

static inline long serp_readdir(int fd, struct serp_dirent *entry)
{
    return serp_call(SYS_READDIR, fd, (long)entry, sizeof *entry);
}

static inline int serp_mkdir(const char *path)
{
    return (int)serp_call(SYS_MKDIR, (long)path, 0, 0);
}

static inline int serp_unlink(const char *path)
{
    return (int)serp_call(SYS_UNLINK, (long)path, 0, 0);
}

static inline int serp_rename(const char *from, const char *to)
{
    return (int)serp_call(SYS_RENAME, (long)from, (long)to, 0);
}

static inline int serp_sync(void)
{
    return (int)serp_call(SYS_SYNC, 0, 0, 0);
}

static inline int serp_getkey(int flags)
{
    return (int)serp_call(SYS_GETKEY, flags, 0, 0);
}

static inline int serp_spawn(const char *path, const char *arguments)
{
    return (int)serp_call(SYS_SPAWN, (long)path, (long)arguments, 0);
}

static inline int serp_getpid(void)
{
    return (int)serp_call(SYS_GETPID, 0, 0, 0);
}

static inline int serp_chdir(const char *path)
{
    return (int)serp_call(SYS_CHDIR, (long)path, 0, 0);
}

static inline int serp_getcwd(char *buffer, size_t length)
{
    return (int)serp_call(SYS_GETCWD, (long)buffer, (long)length, 0);
}

static inline unsigned long serp_ticks(void)
{
    return (unsigned long)serp_call(SYS_GETTICKS, 0, 0, 0);
}

static inline void serp_yield(void)
{
    serp_call(SYS_YIELD, 0, 0, 0);
}

static inline int serp_waitpid(int pid, int flags)
{
    return (int)serp_call(SYS_WAITPID, pid, flags, 0);
}

static inline int serp_getuid(void)
{
    return (int)serp_call(SYS_GETUID, 0, 0, 0);
}

static inline int serp_spawnas(const char *path, const char *arguments, const char *user, const char *password)
{
    return (int)serp_call4(SYS_SPAWNAS, (long)path, (long)arguments, (long)user, (long)password);
}

static inline int serp_chmod(const char *path, int mode)
{
    return (int)serp_call(SYS_CHMOD, (long)path, mode, 0);
}

static inline int serp_chown(const char *path, int uid)
{
    return (int)serp_call(SYS_CHOWN, (long)path, uid, 0);
}

static inline int serp_passwd(const char *user, const char *old, const char *password)
{
    return (int)serp_call(SYS_PASSWD, (long)user, (long)old, (long)password);
}

static inline long serp_con_info(void)
{
    return serp_call(SYS_CON_INFO, 0, 0, 0);
}

static inline int serp_con_draw(int column, int row, const uint16_t *cells, int count)
{
    return (int)serp_call4(SYS_CON_DRAW, column, row, (long)cells, count);
}

static inline int serp_con_restore(void)
{
    return (int)serp_call(SYS_CON_RESTORE, 0, 0, 0);
}

static inline int serp_fb_blit(const void *pixels, int stride, int x, int y, int width, int height)
{
    return (int)serp_call4(SYS_FB_BLIT, (long)pixels, stride, ((long)y << 16) | x, ((long)height << 16) | width);
}

static inline int serp_event(struct serp_event *event, int flags)
{
    return (int)serp_call(SYS_EVENT, (long)event, flags, 0);
}

static inline int serp_send(int pid, const struct serp_message *message)
{
    return (int)serp_call(SYS_SEND, pid, (long)message, 0);
}

static inline int serp_shm_attach(void)
{
    return (int)serp_call(SYS_SHM_ATTACH, 0, 0, 0);
}

static inline int serp_service(int operation, void *argument)
{
    return (int)serp_call(SYS_SERVICE, operation, (long)argument, 0);
}

static inline int serp_tty_open(void)
{
    return (int)serp_call(SYS_TTY_OPEN, 0, 0, 0);
}

static inline long serp_tty_read(int tty, void *buffer, size_t length)
{
    return serp_call(SYS_TTY_READ, tty, (long)buffer, (long)length);
}

static inline int serp_tty_input(int tty, int key)
{
    return (int)serp_call(SYS_TTY_INPUT, tty, key, 0);
}

static inline long serp_kcommand(const char *line, char *output, size_t length)
{
    return serp_call(SYS_KCOMMAND, (long)line, (long)output, (long)length);
}

static inline long serp_log(const char *text, size_t length)
{
    return serp_call(SYS_LOG, (long)text, (long)length, 0);
}

static inline int serp_dup2(int from, int to)
{
    return (int)serp_call(SYS_DUP2, from, to, 0);
}

static inline void *serp_alloc(size_t length)
{
    return (void *)serp_call(SYS_ALLOC, (long)length, 0, 0);
}

static inline int serp_sysinfo(struct serp_sysinfo *info)
{
    return (int)serp_call(SYS_SYSINFO, (long)info, 0, 0);
}

static inline int serp_kill(int pid)
{
    return (int)serp_call(SYS_KILL, pid, 0, 0);
}

static inline int serp_pipe(int fds[2])
{
    return (int)serp_call(SYS_PIPE, (long)fds, 0, 0);
}

static inline int serp_time(struct serp_time *now)
{
    return (int)serp_call(SYS_TIME, (long)now, 0, 0);
}

static inline int serp_theme(int operation, int value, void *buffer)
{
    return (int)serp_call(SYS_THEME, operation, value, (long)buffer);
}

static inline int serp_palette(int first, int count, const uint8_t *rgb)
{
    return (int)serp_call(SYS_PALETTE, first, count, (long)rgb);
}

static inline int serp_isatty(int fd)
{
    return (int)serp_call(SYS_ISATTY, fd, 0, 0);
}

void serp_sleep(unsigned long ticks);

#endif
