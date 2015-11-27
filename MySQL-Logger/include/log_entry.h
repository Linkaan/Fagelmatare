#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <errmsg.h>
#include <time.h>

typedef struct {
    signed char severity;
    char event[129]; /* 128 characters plus NUL. Latin1 means 1 byte per character */
    char source[33];
    struct tm *tm_info;
} log_entry;

#endif
