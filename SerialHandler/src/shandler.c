/*
 *  shandler.c
 *    Listen for incoming UNIX socket connections, events and send requests.
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
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <pthread.h>
#include <errno.h>
#include <wiringSerial.h>
#include <lstack.h>
#include <ehandler.h>
#include <config.h>
#include <log.h>

#define CONFIG_PATH "/etc/fagelmatare.conf"

#define DEBUG
#ifndef DEBUG
#define _log_debug(format, ...)
#else
#define _log_debug(format, ...) log_debug(format, ##__VA_ARGS__)
#endif

typedef struct {
  int sock;
  char msg[129];
} serial_args;

struct user_data {
  lstack_t *results;
  pthread_mutex_t *mxq;
  pthread_mutex_t *mxs;
  int *sfd;
  int *pipefd;
  int (*sock)[2];
  struct config *configs;
};

static int is_atexit_enabled;

static sem_t wakeup_main;
static sem_t cleanup_done;

void send_serial    (char *, const int, struct user_data *);
void *listen_serial (void *);
void *network_func  (void *);

char *read_string_until(int, char, int);

int sem_posted      (sem_t *);
static int need_quit(pthread_mutex_t *);

void die            (int);
void quit           (int);
void cleanup        ();
void exit_handler   ();

int main(void) {
  int unixsock;
  int inetsock;
  int flags;
  int sfd;
  int err;
  int pipefd[2];
  struct sockaddr_un addr_un;
  struct sockaddr_in addr_in;
  struct config configs;
  socklen_t addrlen;
  serial_args* sargs;
  lstack_t results;
  pthread_t network_thread, serial_thread;
  pthread_mutex_t mxq, mxs;

  /* parse configuration file */
  if(get_config(CONFIG_PATH, &configs)) {
    printf("could not parse configuration file: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  struct user_data_log userdata_log = {
    .log_level= LOG_LEVEL_WARN,
    .configs  = &configs,
  };

  if(log_init(&userdata_log)) {
    printf("error initalizing log thread: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if((sfd = serialOpen("/dev/ttyAMA0", 9600)) < 0) {
    log_fatal("open serial device failed: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if(pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if((err = lstack_init(&results, 10 + 2)) != 0) {
    log_fatal("could not initialize lstack (%d)\n", err);
    log_exit();
    exit(1);
  }

  if((unixsock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    log_fatal("socket(AF_UNIX, SOCK_STREAM, 0) error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if((inetsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    log_fatal("socket(AF_INET, SOCK_STREAM, 0) error: %s\n", strerror(errno));
    close(unixsock);
    log_exit();
    exit(1);
  }

  if(setsockopt(unixsock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
    log_fatal("AF_UNIX: setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  if(setsockopt(inetsock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
    log_fatal("AF_INET: setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  flags = fcntl(unixsock, F_GETFL, 0);
  if((fcntl(unixsock, F_SETFL, flags | O_NONBLOCK)) < 0) {
    log_fatal("AF_UNIX: fnctl failed: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  flags = fcntl(inetsock, F_GETFL, 0);
  if((fcntl(inetsock, F_SETFL, flags | O_NONBLOCK)) < 0) {
    log_fatal("AF_INET: fnctl failed: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  addrlen = sizeof(struct sockaddr_un);
  memset(&addr_un, 0, addrlen);
  addr_un.sun_family = AF_UNIX;
  strncpy(addr_un.sun_path, configs.sock_path, sizeof(addr_un.sun_path)-1);

  unlink(configs.sock_path);

  if(bind(unixsock, (struct sockaddr *) &addr_un, addrlen) < 0) {
    log_fatal("AF_UNIX: bind error: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  addrlen = sizeof(struct sockaddr_in);
  memset(&addr_in, 0, addrlen);
  addr_in.sin_family = AF_INET;
  addr_in.sin_addr.s_addr = INADDR_ANY;
  addr_in.sin_port = htons(configs.inet_port);

  if(bind(inetsock, (struct sockaddr *) &addr_in, addrlen) < 0) {
    log_fatal("AF_INET: bind error: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  if(listen(unixsock, 5) < 0) {
    log_fatal("AF_UNIX: listen error: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);

    log_exit();
    exit(1);
  }

  if(listen(inetsock, 5) < 0) {
    log_fatal("AF_INET: listen error: %s\n", strerror(errno));
    close(unixsock);
    close(inetsock);
    log_exit();
    exit(1);
  }

  if(ehandler_init(5) < 0) {
    log_fatal("ehandler initialization error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  int sock[2] = {unixsock, inetsock};
  struct user_data userdata = {
    .results = &results,
    .mxq = &mxq,
    .mxs = &mxs,
    .sfd = &sfd,
    .pipefd = &pipefd[0],
    .sock = &sock,
    .configs = &configs,
  };

  pthread_mutex_init(&mxq, NULL);
  pthread_mutex_init(&mxs, NULL);
  pthread_mutex_lock(&mxq);
  if(pthread_create(&network_thread, NULL, network_func, &userdata)) {
    log_fatal("creating network thread: %s\n", strerror(errno));
    close(unixsock);
    log_exit();
    exit(1);
  }
  if(pthread_create(&serial_thread, NULL, listen_serial, &userdata)) {
    log_fatal("creating serial thread: %s\n", strerror(errno));
    close(unixsock);
    log_exit();
    exit(1);
  }

  sem_init(&wakeup_main, 0, 0);
  sem_init(&cleanup_done, 0, 0);
  signal(SIGALRM, quit);
  signal(SIGHUP, die);
  signal(SIGINT, die);
  signal(SIGQUIT, die);
  signal(SIGILL, die);
  signal(SIGTRAP, die);
  signal(SIGABRT, die);
  signal(SIGIOT, die);
  signal(SIGFPE, die);
  signal(SIGKILL, die);
  signal(SIGUSR1, die);
  signal(SIGSEGV, die);
  signal(SIGUSR2, die);
  signal(SIGPIPE, die);
  signal(SIGTERM, die);
#ifdef SIGSTKFLT
  signal(SIGSTKFLT, die);
#endif
  signal(SIGCHLD, die);
  signal(SIGCONT, die);
  signal(SIGSTOP, die);
  signal(SIGTSTP, die);
  signal(SIGTTIN, die);
  signal(SIGTTOU, die);

  is_atexit_enabled = 1;
  atexit(exit_handler);

  sargs = NULL;
  while(!sem_posted(&wakeup_main)) {
    if((sargs = lstack_pop(&results)) != NULL) {
      if(*sargs->msg == '\0') continue;
      send_serial(sargs->msg, sargs->sock, &userdata);
      free(sargs);
    }
    usleep(50000); // TODO add proper polling system here
  }

  pthread_mutex_unlock(&mxq);

  write(pipefd[1], NULL, 8);
  close(pipefd[1]);

  pthread_join(network_thread,NULL);
  pthread_join(serial_thread, NULL);
  pthread_mutex_destroy(&mxq);
  pthread_mutex_destroy(&mxs);
  lstack_free(&results);
  ehandler_cleanup();

  log_exit();
  free_config(&configs);
  alarm(0);

  sem_post(&cleanup_done);
}

void send_serial(char *msg, const int sock, struct user_data *userdata) {
  int rc, len;
  char *str;
  _log_debug("now processing %s, %s wait for answer.\n", msg, !sock ? "will not" : "will");
  pthread_mutex_lock(userdata->mxs);
  while(*msg) serialPutchar(*(userdata->sfd), *msg++);
  serialPutchar(*(userdata->sfd), '\0');
  _log_debug("sent request over serial bus.\n");
  if(!sock){
    pthread_mutex_unlock(userdata->mxs);
    return;
  }
  while(1) {
    _log_debug("attempting to read answer...\n");
    if((str = read_string_until(*(userdata->sfd), '\0', 64)) == NULL || str[0] == '\0') {
      str = strdup("NaN");
      break;
    }else {
      if(!strncasecmp("/E/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        event_t *event = ehandler_get(str);
        if(event == NULL || ehandler_handle(event, NULL)) {
          log_warn("unable to handle event (%s)\n", str);
        }
        _log_debug("caught event %s.\n", str);
        free(str);
      }else if(!strncasecmp("/R/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        break;
      }else {
        log_error("read string violating protocol (%s)\n", str);
        free(str);
      }
    }
  }
  pthread_mutex_unlock(userdata->mxs);
  _log_debug("now done processing %s, answer was %s.\n", msg, str);
  len = strlen(str);
  if((rc = send(sock, str, len, MSG_NOSIGNAL)) != len) {
    if(rc > 0) log_error("partial write (%d of %d)\n", rc, len);
    else {
      log_error("send error to socket %d: %s\n", sock, strerror(errno));
    }
  }
  free(str);
  close(sock);
}

int sem_posted(sem_t *sem) {
  int sval = 0;
  sem_getvalue(sem, &sval);
  return sval;
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

char *read_string_until(int sfd, char terminator, int n) {
  char *str, *tmp;
  int ch;
  int counter = 1;

  str = malloc(counter * sizeof(char));
  if(str == NULL) {
    log_fatal("failed to read serial string: %s\n", strerror(errno));
    return NULL;
  }
  ch = -1;

  while(counter < n && (ch = serialGetchar(sfd)) >= 0) {
    str[counter-1] = ch;
    tmp = realloc(str, (++counter) * sizeof(char));
    if(tmp == NULL) {
      log_fatal("failed to read serial string: %s\n", strerror(errno));
      return NULL;
    }else {
      str = tmp;
    }
    if(ch == terminator) {
      return str;
    }
  }
  if(ch < 0) {
    free(str);
    return NULL;
  }else{
    str[counter-1] = '\0';
    return str;
  }
}

void *listen_serial(void *param) {
  struct user_data *userdata = param;
  char *str;

  while(!need_quit(userdata->mxq)) {
    pthread_mutex_lock(userdata->mxs);
    if(!serialDataAvail(*(userdata->sfd))) {
      pthread_mutex_unlock(userdata->mxs);
      usleep(50000); // TODO: do proper polling system here
      continue;
    }

    str = read_string_until(*(userdata->sfd), '\0', 64);
    pthread_mutex_unlock(userdata->mxs);
    if(str == NULL || str[0] == '\0') {
      str = strdup("-1");
    }else {
      if(!strncasecmp("/E/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        event_t *event = ehandler_get(str);
        if(event == NULL || ehandler_handle(event, NULL)) {
          log_error("unable to handle event (%s)\n", str);
        }
        //_log_debug("caught event %s.\n", str);
      }else if(!strncasecmp("/R/", str, 3)) {
        log_error("read return string in wrong thread (%s)\n", str);
      }else {
        log_error("read string violating protocol (%s)\n", str);
      }
    }
    free(str);
  }
  return NULL;
}

void *network_func(void *param) {
  int cl, rc, await;
  int sock = 0;
  char buf[129];
  struct pollfd p[3];
  struct sockaddr addr;
  struct user_data *userdata = param;
  int *pipefd = userdata->pipefd;
  socklen_t addrlen = 0;

  memset(&p, 0, sizeof(p));
  p[0].fd = (*userdata->sock)[0];
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = (*userdata->sock)[1];
  p[2] = p[0];
  p[2].fd = pipefd[0];

  while (1) {
    if(poll(p, 3, -1) > 0) {
      if(p[0].revents & (POLLIN|POLLPRI)) {
        sock = p[0].fd;
        addrlen = sizeof(struct sockaddr_un);
      }else if(p[1].revents & (POLLIN|POLLPRI)) {
        sock = p[1].fd;
        addrlen = sizeof(struct sockaddr_in);
      }else if(p[2].revents & (POLLIN|POLLPRI)) {
        break;
      }
      if((cl = accept(sock, &addr, &addrlen)) < 0) {
        log_error("accept error: %s\n", strerror(errno));
        continue;
      }

      memset(&buf, 0, sizeof(buf));
      await = 0;
      if((rc = recv(cl, buf, sizeof(buf), 0)) > 0) {
        _log_debug("new connection from socket %d, read message \"%s\".\n", cl, buf);
        if(!strncasecmp("/S/", buf, 3)) {
          size_t len = strlen(buf);
          memmove(buf, buf+3, len - 3 + 1);
          for(char *p = strtok(buf, "|");p != NULL;p = strtok(NULL, "|")) {
            if(ehandler_subscribe(p, cl)) {
              log_error("unable to subscribe to event (%s)\n", p);
            }
            _log_debug("handling subscription to event \"%s\" from socket %d.\n", p, cl);
          }
          continue;
        }else if(!strncasecmp("/E/", buf, 3)) {
          size_t len = strlen(buf);
          memmove(buf, buf+3, len - 3 + 1);
          char *type = strtok(buf, ":");
          event_t *event = ehandler_get(type);
          char *data = strtok(NULL, ":");
          if(event == NULL || (data ? ehandler_handle(event, data) : ehandler_handle(event, NULL))) {
            log_error("unable to handle event (%s)\n", buf);
          }
          _log_debug("caught event %s.\n", buf);
          close(cl);
          continue;
        }

        serial_args *sargs = malloc(sizeof(serial_args));
        if(sargs == NULL) {
          log_fatal("in network_func: memory allocation failed: (%s)\n", strerror(errno));
          // abort() ???
          continue;
        }
        memset(sargs, 0, sizeof(serial_args));

        if(sscanf(buf, "%d;%s", &await, sargs->msg) < 0) {
          strcpy(sargs->msg, buf);
          sargs->sock = 0;
          _log_debug("closing connection.");
          close(cl);
        }else if(await) {
          sargs->sock = cl;
        }else {
          sargs->sock = 0;
          _log_debug("closing connection.");
          close(cl);
        }
        if(lstack_push(userdata->results, sargs) != 0) {
          log_error("enqueue serial entry failed\n");
          _log_debug("closing connection.");
          close(cl);
        }
      }else if(rc < 0) {
        log_error("recv error: %s\n", strerror(errno));
        _log_debug("closing connection.");
        close(cl);
      }else {
        _log_debug("closing connection.");
        close(cl);
      }
    }
  }
  close((*userdata->sock)[0]);
  close((*userdata->sock)[1]);
  unlink(userdata->configs->sock_path);
  return NULL;
}

void die(int sig) {
  if(sig != SIGINT && sig != SIGTERM)
    log_fatal("received signal %d (%s), exiting.\n", sig, strsignal(sig));
  cleanup();
}

void quit(int sig) {
  printf("unclean exit, from signal %d (%s).\n", sig, strsignal(sig));
  is_atexit_enabled = 0;
  exit(1);
}

void exit_handler() {
  int atexit_enabled = is_atexit_enabled;
  cleanup();
  if(atexit_enabled) sem_wait(&cleanup_done);
  sem_destroy(&wakeup_main);
  sem_destroy(&cleanup_done);
}

void cleanup() {
  if(!is_atexit_enabled) return;
  is_atexit_enabled = 0;
  sem_post(&wakeup_main);
  alarm(5);
}
