/*
 *  dblogger.c
 *    Library to send error/info logs to Fågelmatare Server
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include <mysql.h>
#include <my_global.h>

#include <log_entry.h>

// mysql specific structs initialized in connect_to_database
static MYSQL *mysql = NULL;
static MYSQL_STMT *stmt = NULL;
static char *stmt_prepare;
static int len;

/*
 * Function to print error messages to stdout with formatted date.
 */
static void log_msg_vargs(const char *format, const va_list args) {
  char buffer[20];
  time_t rawtime;

  // fetch current time
  time(&rawtime);
  strftime(buffer, 20, "%F %H:%M:%S", localtime(&rawtime));
  fprintf(stdout, "%s : ", buffer);
  vfprintf(stdout, format, args);
}

/*
 * Variadic function calling log_msg_vargs
 */
static void log_msg(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_msg_vargs(format, args);
  va_end(args);
}

int connect_to_database(const char *address, const char *user, const char *pwd) {
  // initialize mysql library
  if (!mysql) mysql = mysql_init(NULL);
  if (!mysql) {
    log_msg("in dblogger:connect_to_database: mysql_init failed: %s\n", mysql_error(mysql));
    return mysql_errno(mysql);
  }

  // set auto-reconnect option for mysql
  mysql_options(mysql, MYSQL_OPT_RECONNECT, &(int){ 1 });
  if (!mysql_real_connect(mysql, address, user, pwd, "fagelmatare", 0, NULL, 0)) {
    log_msg("in dblogger:connect_to_database: mysql_real_connect failed: %s\n", mysql_error(mysql));
    return mysql_errno(mysql);
  }

  // initialize prepared statement
  stmt = mysql_stmt_init(mysql);
  if (!stmt) {
    log_msg("in dblogger:connect_to_database: mysql_stmt_init failed: %s\n", strerror(CR_OUT_OF_MEMORY));
    return CR_OUT_OF_MEMORY;
  }

  len = asprintf(&stmt_prepare, "INSERT INTO `logg` (severity,event,source,datetime)"
						     "VALUES(?,?,?,?)");
  if (len < 0) {
    log_msg("in dblogger:connect_to_database: asprintf failed: %s\n", strerror(errno));
    return errno;
  }

  // prepare stmt
  if (mysql_stmt_prepare(stmt, stmt_prepare, len)) {
    log_msg("in dblogger:connect_to_database: mysql_stmt_prepare failed: %s\n", mysql_stmt_error(stmt));
    return mysql_stmt_errno(stmt);
  }

  return 0;
}

int log_to_database(log_entry *ent) {
  MYSQL_TIME ts;
  MYSQL_BIND sbind[4];
  struct tm *tm_info;

  if (!ent) return EFAULT;
  if (!mysql || !stmt) return -1;

  // ping mysql and reconnect to server if necessary
  // if connection_id changed, we need to reinitialize
  // server resources (i.e prepare stmt)
  unsigned long connection_id = mysql_thread_id(mysql);
  if (mysql_ping(mysql)) {
    log_msg("in dblogger:log_to_database: mysql_ping failed: %s\n", mysql_error(mysql));
    return mysql_errno(mysql);
  }
  if (mysql_thread_id(mysql) != connection_id) {
    log_msg("in dblogger:log_to_database: auto-reconnect established\n");
    if (mysql_stmt_prepare(stmt, stmt_prepare, len)) {
      log_msg("in dblogger:log_to_database: mysql_stmt_prepare failed: %s\n", mysql_stmt_error(stmt));
      return mysql_stmt_errno(stmt);
    }
  }

  // insert data into sbind from log_entry
  memset(sbind,0,sizeof(sbind));
  sbind[0].buffer_type = MYSQL_TYPE_TINY;
  sbind[0].buffer = &ent->severity;
  sbind[0].is_null = 0;
  sbind[0].length = 0;

  sbind[1].buffer_type = MYSQL_TYPE_STRING;
  sbind[1].buffer = ent->event;
  sbind[1].buffer_length = strlen(ent->event);
  sbind[1].is_null = 0;
  sbind[1].length = 0;

  sbind[2].buffer_type = MYSQL_TYPE_STRING;
  sbind[2].buffer = ent->source;
  sbind[2].buffer_length = strlen(ent->source);
  sbind[2].is_null = 0;
  sbind[2].length = 0;

  sbind[3].buffer_type = MYSQL_TYPE_DATETIME;
  sbind[3].buffer = (char *)&ts;
  sbind[3].is_null = 0;
  sbind[3].length = 0;

  // bind parameters to prepare stmt
  mysql_stmt_bind_param(stmt, sbind);

  tm_info = localtime(ent->rawtime);

  ts.year = 1900+tm_info->tm_year;
  ts.month = 1+tm_info->tm_mon;
  ts.day = tm_info->tm_mday;

  ts.hour = tm_info->tm_hour;
  ts.minute = tm_info->tm_min;
  ts.second = tm_info->tm_sec;

  if (mysql_stmt_execute(stmt)) {
    log_msg("in dblogger:log_to_database: mysql_stmt_execute failed: %s\n", mysql_stmt_error(stmt));
    return mysql_stmt_errno(stmt);
  }

  return 0;
}

int disconnect(void) {
  int err = 0;

  if (!mysql) return 0;

  if (stmt != NULL && mysql_stmt_close(stmt)) {
    log_msg("in dblogger:log_to_database: mysql_stmt_close failed: %s\n", mysql_stmt_error(stmt));
    err = mysql_stmt_errno(stmt);
  }

  mysql_close(mysql);
  mysql = NULL;
  stmt = NULL;
  return err;
}
