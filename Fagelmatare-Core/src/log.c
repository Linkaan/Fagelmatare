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
  char buffer[20];
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
  strftime(buffer, 20, "%F %H:%M:%S", localtime(rawtime));
  printf("freeing rawtime (%p) for source \"%s\" --> '%s'\n", rawtime, source, buffer);
  free(rawtime);
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
