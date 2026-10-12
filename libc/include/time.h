#ifndef TIME_H
#define TIME_H

#include <stddef.h>

#define CLOCKS_PER_SEC 100

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t *now);
clock_t clock(void);
struct tm *gmtime(const time_t *moment);
struct tm *localtime(const time_t *moment);
time_t mktime(struct tm *parts);
size_t strftime(char *buffer, size_t length, const char *format, const struct tm *parts);

#endif
