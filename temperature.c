/*
 *  temperature.c
 *    Program to log temperature to server
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
#include <math.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <my_global.h>
#include <mysql.h>

#define CONFIG_PATH "/etc/fagelmatare.conf"

struct config {
 char *serv_addr;
 char *username;
 char *passwd;
 char *sock_path;
 char *subtitle_hook;
};

int get_config(char *filename, struct config *configuration);
void free_config(struct config *configuration);

int main(void) {
  MYSQL *mysql;
  FILE *subtitles;
  struct sockaddr_un addr;
  struct config configs;
  struct timeval timeout;
  char *msg, buf[8];
  char *query;
  int cpu_temperature, out_temperature;
  int len,fd,rc, i;
  fd_set set;

  /* parse configuration file */
  if(get_config(CONFIG_PATH, &configs)) {
    perror("could not parse configuration file");
    exit(EXIT_FAILURE);
  }

  memset(&buf, 0, sizeof(buf));

  mysql = mysql_init(NULL);
  if(NULL == mysql) {
    fprintf(stderr, "%s\n", mysql_error(mysql));
    free_config(&configs);
    exit(EXIT_FAILURE);
  }

  if(NULL == mysql_real_connect(mysql, configs.serv_addr, configs.username, configs.passwd, "fagelmatare", 0, NULL, 0)) {
    mysql_close(mysql);
    fprintf(stderr, "%s\n", mysql_error(mysql));
    free_config(&configs);
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, configs.sock_path, sizeof(addr.sun_path)-1);

  for(i = 0; i < 2; ++i) {
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      mysql_close(mysql);
      break;
    }

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      perror("connect error");
      mysql_close(mysql);
      close(fd);
      break;
    }

    asprintf(&msg, "1;temperature");
    len = strlen(msg);
    if((rc = send(fd, "1;temperature", len, MSG_NOSIGNAL)) != len) {
      if(rc > 0) fprintf(stderr, "partial write");
      else {
        if(rc < 0) {
          perror("write error");
        }else {
          fprintf(stderr, "connection was closed unexpectedly.\n");
        }
        mysql_close(mysql);
        close(fd);
        free(msg);
        break;
      }
    }
    free(msg);

    FD_ZERO(&set);
    FD_SET(fd, &set);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    rc = select(fd+1, &set, NULL, NULL, &timeout);
    if(rc == -1) {
      perror("select failed");
      out_temperature = -1;
    }else if(rc > 0) {
      if((rc = recv(fd, buf, sizeof(buf), 0)) > 0) {
        char *end;

        if(!strncmp("-1", buf, 2)) { // TODO change protocol to add prefix temp=
          out_temperature = -1;
          fprintf(stderr, "SerialHandler returned negative one, resending request...\n");
          close(fd);
          continue;
        }
        out_temperature = (int) strtol(buf, &end, 10);
        if (*end || errno == ERANGE) {
          out_temperature = -1;
          fprintf(stderr, "error parsing SerialHandler return: %s\n", buf);
          close(fd);
          continue;
        }
        asprintf(&query,
          "INSERT INTO `temperatur` ("
          "`source`,`temperature`, `datetime`"
          ") VALUES ("
          "'Outside', '%s', NOW()"
          ")", buf);

        if(mysql_query(mysql, query)) {
          fprintf(stderr, "%s\n", mysql_error(mysql));
        }
        free(query);
      }else if(rc < 0) {
        out_temperature = -1;
        perror("recv failed");
        close(fd);
        break;
      }
    }else {
      fprintf(stderr, "select timeout expired\n");
      close(fd);
      continue;
    }
    close(fd);
    break;
  }
  fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
  if(fd < 0) {
    perror("failed to read temperature file");
    cpu_temperature = -1;
  }else {
    if((rc = read(fd, buf, sizeof(buf))) > 0) {
      char *end;

      strtok(buf, " \n");
      cpu_temperature = (int) strtol(buf, &end, 10);
      if (*end || errno == ERANGE) {
        fprintf(stderr, "error parsing temperature file: %s\n", buf);
        cpu_temperature = -1;
      }else {
        cpu_temperature = (cpu_temperature + 50) / 100;
        asprintf(&query,
          "INSERT INTO `temperatur` ("
          "`source`,`temperature`, `datetime`"
          ") VALUES ("
          "'CPU', '%d', NOW()"
          ")", cpu_temperature);

        if(mysql_query(mysql, query)) {
          fprintf(stderr, "%s\n", mysql_error(mysql));
        }
        free(query);
      }
    }else if(rc < 0) {
      perror("read failed");
    }
  }
  close(fd);
  mysql_close(mysql);
  free_config(&configs);

  subtitles = fopen(configs.subtitle_hook, "w");
  if(subtitles == NULL) {
    perror("failed to open subtitles hook for writing");
  }else {
    fprintf(subtitles,
      "text=OUTSIDE %.1f'C\\nCPU %.1f'C\n"
      "font_name=FreeMono:style=Bold\n"
      "pt=12\n"
      "layout_align=top,left\n"
      "text_align=left\n"
      "horizontal_margin=30\n"
      "vertical_margin=30\n"
      "duration=0" , out_temperature / 10.0f, cpu_temperature / 10.0f);
    fclose(subtitles);
  }
  return 0;
}

int get_config(char *filename, struct config *configuration) {
  FILE *stream;

  if((stream = fopen(filename, "r")) == NULL) {
    return 1;
  }else {
    char *line;
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, stream)) != -1) {
      char *pch;

      pch = strtok(line, " \n");
      if(pch != NULL) {
        if(!strcmp(pch, "address") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->serv_addr = strdup(pch);
        }else if(!strcmp(pch, "username") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->username = strdup(pch);
        }else if(!strcmp(pch, "passwd") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->passwd = strdup(pch);
        }else if(!strcmp(pch, "socket_path") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->sock_path = strdup(pch);
        }else if(!strcmp(pch, "subtitle_hook") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->subtitle_hook = strdup(pch);
        }
      }
    }
    fclose(stream);
  }

  return 0;
}

void free_config(struct config *configuration) {
  free(configuration->serv_addr);
  free(configuration->username);
  free(configuration->passwd);
  free(configuration->sock_path);
  free(configuration->subtitle_hook);
  configuration->serv_addr = NULL;
  configuration->username = NULL;
  configuration->passwd = NULL;
  configuration->sock_path = NULL;
  configuration->subtitle_hook = NULL;
}
