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

  len = asprintf(&msg, "/E/%s", argv[1]);
  if(len < 0) {
    perror("asprintf error");
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
