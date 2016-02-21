/*
 *  send_serial.c
 *    Utility program to interact with Serial Handler
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
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define SOCKET_PATH "/tmp/shandler.sock"

int main(int argc, char **argv) {
  int fd, rc, len;
  char *msg;
  struct sockaddr_un addr;
  socklen_t addrlen;

  if(argc == 0) {
    printf("Usage: %s <event>\n", argv[0]);
    return 0;
  }

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

  asprintf(&msg, "/E/%s", argv[1]);
  len = strlen(msg);
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
