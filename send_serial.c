/*
 *  send_serial.c
 *    Utility program to interact with Serial Handler
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
  int await, fd, rc, len;
  char *msg, buf[129];
  struct sockaddr_un addr;
  socklen_t addrlen;

  memset(&buf, 0, sizeof(buf));

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

  if(argc > 1) {
    asprintf(&msg, "%d;%s", argc > 2 ? atoi(argv[2]) : 0, argv[1]);
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
    if(argc < 3 || !atoi(argv[2])) {
      close(fd);
      exit(EXIT_SUCCESS);
    }
    printf("now waiting for serial handler to write result back!\n");
    if((rc = recv(fd, buf, sizeof(buf), 0)) > 0) {
      printf("read %u bytes: %.*s\n", rc, rc, buf);
    }
    if(rc < 0) {
      perror("recv failed");
    }
  }else while(fgets(buf, sizeof(buf), stdin) != NULL) {
    if(strcmp(buf, "exit") || strcmp(buf, "quit")) {
      break;
    }
    len = strlen(buf);
    if((rc = send(fd, buf, len, MSG_NOSIGNAL)) != len) {
      if(rc > 0) fprintf(stderr, "partial write");
      else {
        perror("send error");
        close(fd);
        exit(EXIT_FAILURE);
      }
    }
    if(sscanf(buf, "%d;",&await) < 0 || !await) continue;
    if((rc = recv(fd, buf, sizeof(buf), MSG_WAITALL)) > 0) {
      printf("read %u bytes: %.*s\n", rc, rc, buf);
    }
    if(rc < 0) {
      perror("recv failed");
    }
  }
  close(fd);
  return 0;
}
