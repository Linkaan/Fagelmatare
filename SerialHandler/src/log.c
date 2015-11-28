/*
 *  log.c
 *    Log messages to server with specified log level using MySQL-Logger
 *    library.
 *    Copyright (C) 2015 Linus Styrén
 *****************************************************************************
 *  This file is part of Fågelmataren:
 *    https://github.com/Linkaan/Fagelmatare/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <log.h>

static int log_level = LOG_LEVEL_NONE;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_set_level(int level) {
  log_level = level;
}

int log_get_level(void) {
  return log_level;
}

void log_msg(int msg_log_level, time_t *rawtime, const char *source, const char *format, const va_list args) {
  int err;
  log_entry ent;

  memset(&ent, 0, sizeof(log_entry));

  ent.severity = (signed char) msg_log_level;
  vsprintf(ent.event, format, args);
  strcpy(ent.source, source);
  ent.tm_info = localtime(rawtime);

  pthread_mutex_lock(&log_mutex);
  if(msg_log_level >= log_level) {
    vfprintf(stdout, format, args);
  }
  if((err = log_to_database(&ent)) != 0) {
    if((err != CR_SERVER_GONE_ERROR && err != -1) ||
      (err = connect_to_database(NULL, NULL, NULL)) != 0 ||
      (err = log_to_database (&ent)) != 0) {
      fprintf(stderr, "could not log to database (%d)\n", err);
    }
  }
  free(rawtime);
  rawtime = NULL;
  pthread_mutex_unlock(&log_mutex);
}

void log_msg_level(int msg_log_level, time_t *rawtime, const char *source, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_msg(msg_log_level, rawtime, source, format, args);
  va_end(args);
}

void log_info(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_INFO, rawtime, "Serial Handler", format, args);
  va_end(args);
}

void log_warn(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_WARN, rawtime, "Serial Handler", format, args);
  va_end(args);
}

void log_error(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_ERROR, rawtime, "Serial Handler", format, args);
  va_end(args);
}

void log_fatal(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_FATAL, rawtime, "Serial Handler", format, args);
  va_end(args);
}
