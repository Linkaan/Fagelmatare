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

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#define SOCKET_PATH "/tmp/shandler.sock"

int main(int argc, char **argv) {
  int fd, rc, len;
  int cpu_temp;
  char buf[8];
  char *msg;
  struct sockaddr_un addr;
  socklen_t addrlen;

  fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
  if(fd < 0) {
    perror("failed to read temperature file");
    cpu_temp = -1;
  }else {
    if((rc = read(fd, buf, sizeof(buf))) > 0) {
      char *end;

      strtok(buf, " \n");
      cpu_temp = (int) strtol(buf, &end, 10);
      if (*end || errno == ERANGE) {
        fprintf(stderr, "error parsing temperature file: %s\n", buf);
        cpu_temp = -1;
      }else {
        cpu_temp = (cpu_temp + 50) / 100;
      }
    }else if(rc < 0) {
      perror("read failed");
    }
  }
  close(fd);

  if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket error");
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
  addrlen = sizeof(struct sockaddr_un);

  if(connect(fd, (struct sockaddr*)&addr, addrlen) == -1) {
    perror("connect error");
    close(fd);
    exit(EXIT_FAILURE);
  }
  if(cpu_temp != -1) {
    len = asprintf(&msg, "/E/temp:%d", cpu_temp);
  }else {
    len = asprintf(&msg, "/E/temp");
  }
  if(len < 0) {
    perror("asprintf failed");
    close(fd);
    free(msg);
    exit(EXIT_FAILURE);
  }

  if((rc = send(fd, msg, len, MSG_NOSIGNAL)) != len) {
    if(rc > 0) fprintf(stderr, "partial write");
    else {
      perror("send error");
      close(fd);
      free(msg);
      exit(EXIT_FAILURE);
    }
  }
  free(msg);
  close(fd);
  return 0;
}
