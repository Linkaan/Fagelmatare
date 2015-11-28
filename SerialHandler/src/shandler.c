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
#define _log_debug(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#endif

typedef struct {
  int sock;
  char msg[129];
} serial_args;

struct user_data {
  lstack_t *results;
  pthread_mutex_t *mxq;
  pthread_mutex_t *serial_mutex;
  int *sfd;
  int *pipefd;
  int *sock;
  struct sockaddr_un *addr;
  struct config *configs;
  socklen_t *addrlen;
};

static sem_t sem;

void send_serial(char *, const int, struct user_data *);
char *read_string_until(int, char, int);
void *listen_serial(void *);
void *network_func(void *);
void die(int);
void quit(int);
int sem_posted(sem_t *);
static int need_quit(pthread_mutex_t *);

int main(void) {
  int sock, flags;
  int sfd;
  int err;
  int pipefd[2];
  struct sockaddr_un addr;
  struct config configs;
  socklen_t addrlen;
  serial_args* sargs;
  lstack_t results;
  pthread_t network_thread, serial_thread;
  pthread_mutex_t mxq, serial_mutex;

  log_set_level(LOG_LEVEL_WARN);

  /* parse configuration file */
  if(get_config(CONFIG_PATH, &configs)) {
    log_fatal("could not parse configuration file: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* attempt to connect to database to instantiate dblogger for use */
  if((err = connect_to_database(configs.serv_addr, configs.username, configs.passwd)) != 0) { // "pi", "jF9bHN7ACY7CwD3Q"
    log_warn("could not connect to database (%d)\n", err);
  }

  if((sfd = serialOpen("/dev/ttyAMA0", 9600)) < 0) {
    log_fatal("open serial device failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if((err = lstack_init(&results, 10 + 2)) != 0) {
    log_fatal("could not initialize lstack (%d)\n", err);
    exit(EXIT_FAILURE);
  }

  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    log_fatal("socket error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
    log_fatal("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }

  flags = fcntl(sock, F_GETFL, 0);
  if((fcntl(sock, F_SETFL, flags | O_NONBLOCK)) < 0) {
    log_fatal("fnctl failed: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, configs.sock_path, sizeof(addr.sun_path)-1);
  addrlen = sizeof(struct sockaddr_un);

  unlink(configs.sock_path);

  if(bind(sock, (struct sockaddr *) &addr, addrlen) < 0) {
    log_fatal("bind error: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }

  if(listen(sock, 5) < 0) {
    log_fatal("listen error: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }

  if(ehandler_init(5) < 0) {
    log_fatal("ehandler initialization error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(ehandler_insert("motion") == NULL) {
    log_fatal("ehandler_insert error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct user_data userdata = {
    .results = &results,
    .mxq = &mxq,
    .serial_mutex = &serial_mutex,
    .sfd = &sfd,
    .pipefd = &pipefd[0],
    .sock = &sock,
    .addr = &addr,
    .configs = &configs,
    .addrlen = &addrlen,
  };

  pthread_mutex_init(&mxq,NULL);
  pthread_mutex_init(&serial_mutex, NULL);
  pthread_mutex_lock(&mxq);
  if(pthread_create(&network_thread, NULL, network_func, &userdata)) {
    log_fatal("creating network thread: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }
  if(pthread_create(&serial_thread, NULL, listen_serial, &userdata)) {
    log_fatal("creating serial thread: %s\n", strerror(errno));
    close(sock);
    exit(EXIT_FAILURE);
  }

  sem_init(&sem, 0, 0);
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

  sargs = NULL;
  while(!sem_posted(&sem)) {
    if((sargs = lstack_pop(&results)) != NULL) {
      if(*sargs->msg == '\0') continue;
      send_serial(sargs->msg, sargs->sock, &userdata);
      free(sargs);
    }
    usleep(50000);
  }
  sem_destroy(&sem); /* destroy semaphore */

  pthread_mutex_unlock(&mxq);

  write(pipefd[1], NULL, 8);
  close(pipefd[1]);

  pthread_join(network_thread,NULL);
  pthread_join(serial_thread, NULL);
  pthread_mutex_destroy(&mxq);
  pthread_mutex_destroy(&serial_mutex);
  lstack_free(&results);
  ehandler_cleanup();
  free_config(&configs);

  if((err = disconnect()) != 0) {
    _log_debug("error while disconnecting from database (%d)\n", err);
  }

  alarm(0);
  exit(EXIT_SUCCESS);
}

void send_serial(char *msg, const int sock, struct user_data *userdata) {
  int rc, len;
  char *str;
  _log_debug("now processing %s, %s wait for answer.\n", msg, !sock ? "will not" : "will");
  pthread_mutex_lock(userdata->serial_mutex);
  while(*msg) serialPutchar(*(userdata->sfd), *msg++);
  serialPutchar(*(userdata->sfd), '\0');
  //serialPuts(*(userdata->sfd), msg);
  _log_debug("sent request over serial bus.\n");
  if(!sock){
    pthread_mutex_unlock(userdata->serial_mutex);
    return;
  }
  while(1) {
    _log_debug("attempting to read answer...\n");
    if((str = read_string_until(*(userdata->sfd), '\0', 64)) == NULL || str[0] == '\0') {
      str = strdup("-1");
      break;
    }else {
      if(!strncasecmp("/E/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        event_t *event = ehandler_get(str);
        if(event == NULL || ehandler_handle(event)) {
          log_warn("unable to handle event (%s)", str);
        }
        _log_debug("caught event %s.\n", str);
        free(str);
      }else if(!strncasecmp("/R/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        break;
      }else {
        log_error("read string violating protocol (%s)", str);
        free(str);
      }
    }
  }
  pthread_mutex_unlock(userdata->serial_mutex);
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
  while(counter < n && (ch = serialGetchar(sfd)) > 0) {
    str[counter-1] = ch;
    tmp = realloc(str, (++counter) * sizeof(char));
    if(tmp == NULL) {
      log_fatal("failed to read serial string: %s\n", strerror(errno));
      return NULL;
    }else {
      str = tmp;
    }
    if(ch == terminator) break;
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
    pthread_mutex_lock(userdata->serial_mutex);
    if(!serialDataAvail(*(userdata->sfd))) {
      pthread_mutex_unlock(userdata->serial_mutex);
      usleep(50000);
      continue;
    }

    str = read_string_until(*(userdata->sfd), '\0', 64);
    pthread_mutex_unlock(userdata->serial_mutex);
    if(str == NULL || str[0] == '\0') {
      str = strdup("-1");
    }else {
      if(!strncasecmp("/E/", str, 3)) {
        size_t len = strlen(str);
        memmove(str, str+3, len - 3 + 1);
        event_t *event = ehandler_get(str);
        if(event == NULL || ehandler_handle(event)) {
          log_error("unable to handle event (%s)", str);
        }
        //_log_debug("caught event %s.\n", str);
      }else if(!strncasecmp("/R/", str, 3)) {
        log_error("read return string in wrong thread (%s)", str);
      }else {
        log_error("read string violating protocol (%s)", str);
      }
    }
    free(str);
  }
  return NULL;
}

void *network_func(void *param) {
  int cl, rc, await;
  char buf[129];
  struct pollfd p[2];
  struct user_data *userdata = param;
  int *pipefd = userdata->pipefd;
  pthread_mutex_t *mxq = (pthread_mutex_t*) userdata->mxq;

  bzero(&p, sizeof(p));
  p[0].fd = *userdata->sock;
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = pipefd[0];

  while (1) {
    if(poll(p, 2, -1) > 0) {
      if(need_quit(mxq)) break;
      if((cl = accept(*userdata->sock, (struct sockaddr *) userdata->addr, userdata->addrlen)) < 0) {
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
          event_t *event = ehandler_get(buf);
          if(event == NULL || ehandler_handle(event)) {
            log_error("unable to handle event (%s)", buf);
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
  close(*userdata->sock);
  unlink(userdata->configs->sock_path);
  return NULL;
}

void die(int sig) {
  static atomic_bool called = ATOMIC_VAR_INIT(false);
  if(atomic_compare_exchange_weak(&called, (_Bool[]) { false }, true)) {
    if(sig != SIGINT && sig != SIGTERM)
      log_fatal("received signal %d (%s), exiting.\n", sig, strsignal(sig));
    sem_post(&sem);
    alarm(5);
  }else {
    log_fatal("received signal %d (%s), during cleanup.\n", sig, strsignal(sig));
  }
}

void quit(int sig) {
  log_fatal("unclean exit, from signal %d (%s).\n", sig, strsignal(sig));
  exit(EXIT_FAILURE);
}
