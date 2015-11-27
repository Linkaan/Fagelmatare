#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <libdblogger/dblogger.h>
#include <libdblogger/log_entry.h>

enum {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_INFO = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_ERROR= 3,
  LOG_LEVEL_FATAL= 4,
};

void log_set_level(int level);
int log_get_level(void);
void log_msg(int msg_log_level, time_t *rawtime, const char *source, const char *format, va_list args);
void log_msg_level(int msg_log_level, time_t *rawtime, const char *source, const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);
void log_fatal(const char *format, ...);

#endif
