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

char* make_message(const char *fmt, ...);

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
    msg = make_message("%d;%s", argc > 2 ? atoi(argv[2]) : 0, argv[1]);
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

char* make_message(const char *fmt, ...) {
  int n;
  int size = 16;
  char *p, *np;
  va_list ap;

  if((p = malloc(size)) == NULL)
    return NULL;

  while(1) {
    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if(n < 0)
      return NULL;

    if(n < size)
      return p;

    size = n + 1;

    if((np = realloc(p, size)) == NULL) {
      free(p);
      return NULL;
    }else {
      p = np;
    }
  }
}
