#include <serpentis.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct tm broken;
static const char *month_names[] = {"January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};
static const char *day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_month(int year, int month)
{
    return month_days[month] + (month == 1 && leap(year));
}

static long days_since_epoch(int year, int month, int day)
{
    long days = 0;
    for (int y = 1970; y < year; y++)
        days += leap(y) ? 366 : 365;
    for (int m = 0; m < month; m++)
        days += days_in_month(year, m);
    return days + day - 1;
}

time_t time(time_t *now)
{
    struct serp_time moment;
    time_t value = -1;
    if (serp_time(&moment) >= 0)
        value = days_since_epoch(moment.year, moment.month - 1, moment.day) * 86400L + moment.hour * 3600L +
                moment.minute * 60L + moment.second;
    if (now)
        *now = value;
    return value;
}

clock_t clock(void)
{
    return (clock_t)serp_ticks();
}

struct tm *gmtime(const time_t *moment)
{
    long seconds = *moment;
    long days = seconds / 86400;
    long rest = seconds % 86400;
    broken.tm_hour = (int)(rest / 3600);
    broken.tm_min = (int)(rest / 60 % 60);
    broken.tm_sec = (int)(rest % 60);
    broken.tm_wday = (int)((days + 4) % 7);
    int year = 1970;
    while (days >= (leap(year) ? 366 : 365)) {
        days -= leap(year) ? 366 : 365;
        year++;
    }
    broken.tm_year = year - 1900;
    broken.tm_yday = (int)days;
    int month = 0;
    while (days >= days_in_month(year, month)) {
        days -= days_in_month(year, month);
        month++;
    }
    broken.tm_mon = month;
    broken.tm_mday = (int)days + 1;
    broken.tm_isdst = 0;
    return &broken;
}

struct tm *localtime(const time_t *moment)
{
    return gmtime(moment);
}

time_t mktime(struct tm *parts)
{
    int year = parts->tm_year + 1900 + parts->tm_mon / 12;
    int month = parts->tm_mon % 12;
    if (month < 0) {
        month += 12;
        year--;
    }
    time_t value = days_since_epoch(year, month, 1) * 86400L + (parts->tm_mday - 1) * 86400L +
                   parts->tm_hour * 3600L + parts->tm_min * 60L + parts->tm_sec;
    *parts = *gmtime(&value);
    return value;
}

size_t strftime(char *buffer, size_t length, const char *format, const struct tm *parts)
{
    size_t used = 0;
    char piece[32];
    for (; *format; format++) {
        const char *text = piece;
        if (*format != '%') {
            piece[0] = *format;
            piece[1] = 0;
        } else {
            format++;
            switch (*format) {
            case 'Y': snprintf(piece, sizeof piece, "%d", parts->tm_year + 1900); break;
            case 'y': snprintf(piece, sizeof piece, "%02d", parts->tm_year % 100); break;
            case 'm': snprintf(piece, sizeof piece, "%02d", parts->tm_mon + 1); break;
            case 'd': snprintf(piece, sizeof piece, "%02d", parts->tm_mday); break;
            case 'e': snprintf(piece, sizeof piece, "%2d", parts->tm_mday); break;
            case 'H': snprintf(piece, sizeof piece, "%02d", parts->tm_hour); break;
            case 'I': snprintf(piece, sizeof piece, "%02d", (parts->tm_hour + 11) % 12 + 1); break;
            case 'M': snprintf(piece, sizeof piece, "%02d", parts->tm_min); break;
            case 'S': snprintf(piece, sizeof piece, "%02d", parts->tm_sec); break;
            case 'j': snprintf(piece, sizeof piece, "%03d", parts->tm_yday + 1); break;
            case 'p': text = parts->tm_hour < 12 ? "AM" : "PM"; break;
            case 'A': text = day_names[parts->tm_wday]; break;
            case 'a': snprintf(piece, sizeof piece, "%.3s", day_names[parts->tm_wday]); break;
            case 'B': text = month_names[parts->tm_mon]; break;
            case 'b': snprintf(piece, sizeof piece, "%.3s", month_names[parts->tm_mon]); break;
            case '%': text = "%"; break;
            default: return 0;
            }
        }
        size_t count = strlen(text);
        if (used + count >= length)
            return 0;
        memcpy(buffer + used, text, count);
        used += count;
    }
    buffer[used] = 0;
    return used;
}
