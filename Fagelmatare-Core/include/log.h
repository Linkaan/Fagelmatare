/*
 *  log.h
 *    Log messages to server with specified log level using MySQL-Logger
 *    library.
 *****************************************************************************
 *  This file is part of Fågelmataren, an advanced bird feeder equipped with
 *  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2016 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <libdblogger/dblogger.h>
#include <libdblogger/log_entry.h>
#include <config.h>

enum {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_DEBUG = 1,
  LOG_LEVEL_INFO = 2,
  LOG_LEVEL_WARN = 3,
  LOG_LEVEL_ERROR = 4,
  LOG_LEVEL_FATAL = 5,
};

struct user_data_log {
  int log_level;
  struct config *configs;
};

int log_init(struct user_data_log *userdata);
void log_exit(void);
void log_msg(int msg_log_level, time_t *rawtime, const char *source, const char *format, va_list args);
void log_msg_level(int msg_log_level, time_t *rawtime, const char *source, const char *format, ...);
void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);
void log_fatal(const char *format, ...);

#endif
