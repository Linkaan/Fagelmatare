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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <lstack.h>
#include <log.h>

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

  log_stream = fopen(userdata->configs->fagelmatare_log, "w");
  if(log_stream == NULL) {
     return -1;
  }
  setvbuf(log_stream, NULL, _IONBF, 0);

  if((err = lstack_init(&log_stack, 10 + 5)) != 0) {
    errno = err;
    return -1;
  }

  pthread_mutex_lock(&mxq);
  if(pthread_create(&log_thread, NULL, log_func, userdata)) {
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

void *log_func(void *param) {
  int err;
  struct user_data_log *userdata = param;
  log_entry* ent = NULL;

  connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd);

  while(!need_quit(&mxq)) {
    // TODO add proper polling system
    if((ent = lstack_pop(&log_stack)) != NULL) {
      if(ent->severity >= userdata->log_level) {
        char buffer[20], lls_buffer[10];

        strftime(buffer, 20, "%F %H:%M:%S", localtime(ent->rawtime));
        log_level_string(lls_buffer, ent->severity);

        pthread_mutex_lock(&mxs);
        fprintf(log_stream, "[%s: %s] %s", lls_buffer, buffer, ent->event);
        pthread_mutex_unlock(&mxs);
      }
      if((err = log_to_database(ent)) != 0) {
        if((err != CR_SERVER_GONE_ERROR && err != -1) ||
          (err = connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd)) != 0 ||
          (err = log_to_database (ent)) != 0) {

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

  while((ent = lstack_pop(&log_stack)) != NULL) {
    if(ent->severity >= userdata->log_level) {
      char buffer[20], lls_buffer[10];

      strftime(buffer, 20, "%F %H:%M:%S", localtime(ent->rawtime));
      log_level_string(lls_buffer, ent->severity);

      pthread_mutex_lock(&mxs);
      fprintf(log_stream, "[%s: %s] %s", lls_buffer, buffer, ent->event);
      pthread_mutex_unlock(&mxs);
    }
    if((err = log_to_database(ent)) != 0) {
      if((err != CR_SERVER_GONE_ERROR && err != -1) ||
        (err = connect_to_database(userdata->configs->serv_addr, userdata->configs->username, userdata->configs->passwd)) != 0 ||
        (err = log_to_database (ent)) != 0) {

        pthread_mutex_lock(&mxs);
        fprintf(log_stream, "could not log to database (%d)\n", err);
        pthread_mutex_unlock(&mxs);
      }
    }
    free(ent->rawtime);
    free(ent);
  }
  return NULL;
}

void log_msg(int msg_log_level, time_t *rawtime, const char *source, const char *format, const va_list args) {
  char buffer[20], lls_buffer[10];

  log_entry *ent = malloc(sizeof(log_entry));
  if(ent == NULL) {
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

  if(lstack_push(&log_stack, ent) != 0) {
    pthread_mutex_lock(&mxs);
    fprintf(log_stream, "in log_msg: enqueue log entry failed\n");
    pthread_mutex_unlock(&mxs);

    goto ON_ERROR;
  }

  return;
  ON_ERROR:
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
  log_msg(LOG_LEVEL_INFO, rawtime, "Fagelmatare", format, args);
  va_end(args);
}

void log_warn(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_WARN, rawtime, "Fagelmatare", format, args);
  va_end(args);
}

void log_error(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_ERROR, rawtime, "Fagelmatare", format, args);
  va_end(args);
}

void log_fatal(const char *format, ...) {
  time_t *rawtime;
  va_list args;
  va_start(args, format);
  rawtime = malloc(sizeof(time_t));
  time(rawtime);
  log_msg(LOG_LEVEL_FATAL, rawtime, "Fagelmatare", format, args);
  va_end(args);
}

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
