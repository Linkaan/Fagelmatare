/*
 *  log.c
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <lstack.h>
#include <log.h>

/*
 * Mutexes used for locking resorces
 */
static pthread_mutex_t mxq = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mxs = PTHREAD_MUTEX_INITIALIZER;

static FILE *log_stream;
static pthread_t log_thread;
static lstack_t log_stack;

static int need_quit(pthread_mutex_t *);
void *log_func(void *);

void log_level_string(char *lls_buffer, int msg_log_level) {
  switch (msg_log_level) {
    default:
    case LOG_LEVEL_NONE:
      strcpy(lls_buffer, "NONE");
      break;
    case LOG_LEVEL_DEBUG:
      strcpy(lls_buffer, "DEBUG");
      break;
    case LOG_LEVEL_INFO:
      strcpy(lls_buffer, "INFO");
      break;
    case LOG_LEVEL_WARN:
      strcpy(lls_buffer, "WARNING");
      break;
    case LOG_LEVEL_ERROR:
      strcpy(lls_buffer, "ERROR");
      break;
    case LOG_LEVEL_FATAL:
      strcpy(lls_buffer, "FATAL");
      break;
  }
}

int log_init(struct user_data_log *userdata) {
  int err;

  // open log file in write mode
  log_stream = fopen(userdata->configs->fagelmatare_log, "w");
  if (log_stream == NULL) {
     return -1;
  }

  // disable buffering for file stream
  setvbuf(log_stream, NULL, _IONBF, 0);

  // redirect stdout and stderr to file
  dup2(fileno(log_stream), STDOUT_FILENO);
  dup2(fileno(log_stream), STDERR_FILENO);

  // init lock-free stack used for buffering queries with size 10
  // + number of threads using this module
  if ((err = lstack_init(&log_stack, 10 + 5)) != 0) {
    errno = err;
    return -1;
  }

  pthread_mutex_lock(&mxq);
  if (pthread_create(&log_thread, NULL, log_func, userdata)) {
    return -1;
  }

  return 0;
}

void log_exit() {
  pthread_mutex_unlock(&mxq);
  pthread_join(log_thread, NULL);
  lstack_free(&log_stack);
  fclose(log_stream);
}

/*
 * log_func function runs in another thread and is used for pooling
 * queries from queue using a FIFO stack
 */
void *log_func(void *param) {
  int err;
  struct user_data_log *userdata = param;
  log_entry* ent = NULL;

  connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd);

  while (!need_quit(&mxq)) {
    // TODO add proper polling system

    // if lstack entry is available in queue log it to database
    // using MySQL-Logger library
    if ((ent = lstack_pop(&log_stack)) != NULL) {
      if (ent->severity >= userdata->log_level) {
        char buffer[20], lls_buffer[10];

        // fetch current time in format YYYY-MM-DD HH-MM-SS
        strftime(buffer, 20, "%F %H:%M:%S", localtime(ent->rawtime));
        log_level_string(lls_buffer, ent->severity);

        // print query to log file
        pthread_mutex_lock(&mxs);
        fprintf(log_stream, "[%s: %s] %s", lls_buffer, buffer, ent->event);
        pthread_mutex_unlock(&mxs);
      }

      // attempt to log to database
      if ((err = log_to_database(ent)) != 0) {
        disconnect();
        if ((err = connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd)) != 0) {
          pthread_mutex_lock(&mxs);
          fprintf(log_stream, "could not connect to database (%d)\n", err);
          pthread_mutex_unlock(&mxs);
        } else if ((err = log_to_database (ent)) != 0) {
          pthread_mutex_lock(&mxs);
          fprintf(log_stream, "could not log to database (%d)\n", err);
          pthread_mutex_unlock(&mxs);
        }
      }
      free(ent->rawtime);
      free(ent);
    }

    usleep(50000);
  }

  while ((ent = lstack_pop(&log_stack)) != NULL) {
    if (ent->severity >= userdata->log_level) {
      char buffer[20], lls_buffer[10];

      strftime(buffer, 20, "%F %H:%M:%S", localtime(ent->rawtime));
      log_level_string(lls_buffer, ent->severity);

      pthread_mutex_lock(&mxs);
      fprintf(log_stream, "[%s: %s] %s", lls_buffer, buffer, ent->event);
      pthread_mutex_unlock(&mxs);
    }
    if ((err = log_to_database(ent)) != 0) {
      disconnect();
      if ((err = connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd)) != 0) {
        pthread_mutex_lock(&mxs);
        fprintf(log_stream, "could not connect to database (%d)\n", err);
        pthread_mutex_unlock(&mxs);
      } else if ((err = log_to_database (ent)) != 0) {
        pthread_mutex_lock(&mxs);
        fprintf(log_stream, "could not log to database (%d)\n", err);
        pthread_mutex_unlock(&mxs);
      }
    }
    free(ent->rawtime);
    free(ent);
  }

  if ((err = disconnect()) != 0) {
    fprintf(log_stream, "error while disconnecting from database (%d)\n", err);
  }
  return NULL;
}

void log_msg(int msg_log_level, time_t *rawtime, const char *source, const char *format, const va_list args) {
  char buffer[20], lls_buffer[10];

  // allocate memory for new log entry
  log_entry *ent = malloc(sizeof(log_entry));
  if (ent == NULL) {
    pthread_mutex_lock(&mxs);
    fprintf(log_stream, "in log_msg: memory allocation failed: (%s)\n", strerror(errno));
    pthread_mutex_unlock(&mxs);

    goto ON_ERROR;
  }

  memset(ent, 0, sizeof(log_entry));

  ent->severity = msg_log_level;
  vsprintf(ent->event, format, args);
  strcpy(ent->source, source);
  ent->rawtime = rawtime;

  // push log entry to queue
  if (lstack_push(&log_stack, ent) != 0) {
    pthread_mutex_lock(&mxs);
    fprintf(log_stream, "in log_msg: enqueue log entry failed (size %d)\n", lstack_size(&log_stack));
    pthread_mutex_unlock(&mxs);
    free(ent);
    goto ON_ERROR;
  }

  return;
  ON_ERROR:
  // on error just log message to log file without pushing to database
  strftime(buffer, 20, "%F %H:%M:%S", localtime(rawtime));
  log_level_string(lls_buffer, msg_log_level);

  pthread_mutex_lock(&mxs);
  fprintf(log_stream, "[%s: %s] ", lls_buffer, buffer);
  vfprintf(log_stream, format, args);
  pthread_mutex_unlock(&mxs);

  free(rawtime);
}

void log_msg_level(int msg_log_level, time_t *rawtime, const char *source, const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_msg(msg_log_level, rawtime, source, format, args);
  va_end(args);
}

void log_debug(const char *format, ...) {
  char buffer[20];
  time_t rawtime;

  // fetch current time
  time(&rawtime);
  va_list args;
  va_start(args, format);
  strftime(buffer, 20, "%F %H:%M:%S", localtime(&rawtime));

  pthread_mutex_lock(&mxs);
  fprintf(log_stream, "[DEBUG: %s] ", buffer);
  vfprintf(log_stream, format, args);
  pthread_mutex_unlock(&mxs);

  va_end(args);
}

void log_info(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_INFO, rawtime, "Fagelmatare Server", format, args);
  va_end(args);
}

void log_warn(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_WARN, rawtime, "Fagelmatare Server", format, args);
  va_end(args);
}

void log_error(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_ERROR, rawtime, "Fagelmatare Server", format, args);
  va_end(args);
}

void log_fatal(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_FATAL, rawtime, "Fagelmatare Server", format, args);
  va_end(args);
}

/*
 * need_quit will try to lock mxq and return EBUSY on fail or 0 on
 * success. It is used to see if quit mutex has been unlocked
 * indicating the queue thread should quit.
 */
static int need_quit(pthread_mutex_t *mxq) {
  switch(pthread_mutex_trylock(mxq)) {
    case 0:
      pthread_mutex_unlock(mxq);
      return 1;
    case EBUSY:
      return 0;
  }
  return 1;
}
