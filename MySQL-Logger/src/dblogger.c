/*
 *  dblogger.c
 *    Library to send error/info logs to Fågelmatare Server
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
#include <string.h>
#include <pthread.h>
#include <mysql.h>
#include <my_global.h>
#include <log_entry.h>

#define QUERY_BUFFER_SIZE 589

static MYSQL *mysql = NULL;
static MYSQL_STMT *stmt = NULL;

int connect_to_database(const char *address, const char *user, const char *pwd) {
  char query[QUERY_BUFFER_SIZE];

  if(!mysql) mysql = mysql_init(NULL);
  if(!mysql) {
    return mysql_errno(mysql);
  }

  mysql_options(mysql, MYSQL_OPT_RECONNECT, &(int){ 1 });
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &(unsigned int){ 60 });
  if(!mysql_real_connect(mysql, address, user, pwd, "fagelmatare", 0, NULL, 0)) {
    printf("in dblogger mysql_real_connect failed: %s\n", mysql_error(mysql));
    return mysql_errno(mysql);
  }

  stmt = mysql_stmt_init(mysql);
  if(!stmt) {
    printf("in dblogger mysql_stmt_init failed: %s\n", strerror(CR_OUT_OF_MEMORY));
    return CR_OUT_OF_MEMORY;
  }

  strcpy(query, "INSERT INTO `logg` (severity,event,source,datetime)"
						    "VALUES(?,?,?,?)");
  if(mysql_stmt_prepare(stmt, query, strlen(query))) {
    printf("in dblogger mysql_stmt_prepare failed: %s\n", mysql_stmt_error(stmt));
    return mysql_stmt_errno(stmt);
  }

  return 0;
}

int log_to_database(log_entry *ent) {
  MYSQL_TIME ts;
  MYSQL_BIND sbind[4];
  struct tm *tm_info;

  if(!ent) {
    return EFAULT;
  }

  if(!mysql || !stmt) {
    return -1;
  }

  if(mysql_ping(mysql)) {
    printf("in dblogger mysql_ping failed: %s\n", mysql_error(mysql));
    return mysql_errno(mysql);
  }

  memset(sbind,0,sizeof(sbind));
  sbind[0].buffer_type=MYSQL_TYPE_TINY;
  sbind[0].buffer=&ent->severity;
  sbind[0].is_null=0;
  sbind[0].length=0;

  sbind[1].buffer_type=MYSQL_TYPE_STRING;
  sbind[1].buffer=ent->event;
  sbind[1].buffer_length=strlen(ent->event);
  sbind[1].is_null=0;
  sbind[1].length=0;

  sbind[2].buffer_type=MYSQL_TYPE_STRING;
  sbind[2].buffer=ent->source;
  sbind[2].buffer_length=strlen(ent->source);
  sbind[2].is_null=0;
  sbind[2].length=0;

  sbind[3].buffer_type=MYSQL_TYPE_DATETIME;
  sbind[3].buffer= (char *)&ts;
  sbind[3].is_null= 0;
  sbind[3].length= 0;

  mysql_stmt_bind_param(stmt, sbind);

  tm_info = localtime(ent->rawtime);

  ts.year  = 1900+tm_info->tm_year;
  ts.month = 1+tm_info->tm_mon;
  ts.day   = tm_info->tm_mday;

  ts.hour  = tm_info->tm_hour;
  ts.minute= tm_info->tm_min;
  ts.second= tm_info->tm_sec;

  if(mysql_stmt_execute(stmt)) {
    printf("in dblogger mysql_stmt_execute failed: %s\n", mysql_stmt_error(stmt));
    return mysql_stmt_errno(stmt);
  }

  return 0;
}

int disconnect(void) {
  int err = 0;

  if(mysql_stmt_close(stmt)) {
    printf("in dblogger mysql_stmt_close failed: %s\n", mysql_stmt_error(stmt));
    err = mysql_stmt_errno(stmt);
  }

  mysql_close(mysql);
  return err;
}
