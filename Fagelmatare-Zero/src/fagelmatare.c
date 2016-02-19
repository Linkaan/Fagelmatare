/*
 *  fagelmatare.c
 *    Handle temperature/pressure/humidity sensor readings using sensehat libraries
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <pthread.h>
#include <errno.h>
#include <config.h>
#include <log.h>

#define CONFIG_PATH "/etc/fagelmatare.conf"

#define DEBUG
#ifndef DEBUG
#define _log_debug(format, ...)
#else
#define _log_debug(format, ...) log_debug(format, ##__VA_ARGS__)
#endif

struct user_data {
  lstack_t *results;
  pthread_mutex_t *mxq;
  int *pipefd;
  struct config *configs;
};
static pthread_mutex_t userdata_mutex = PTHREAD_MUTEX_INITIALIZER;

static sem_t wakeup_main;
static sem_t cleanup_done;

static int is_atexit_enabled;

int need_quit       (pthread_mutex_t *mtx);
void *network_func	(void *param);

void die		        (int sig);
void quit           (int sig);
void cleanup        ();
void exit_handler   ();

int sem_posted(sem_t *sem) {
  int sval = 0;
  sem_getvalue(sem, &sval);
  return sval;
}

int main(void) {
  int err;
  int pipefd[2];
  struct config configs;
  pthread_t network_thread;

  /* parse configuration file */
  if(get_config(CONFIG_PATH, &configs)) {
    printf("could not parse configuration file: %s\n", strerror(errno));
    exit(1);
  }

  struct user_data_log userdata_log = {
    .log_level= LOG_LEVEL_WARN,
    .configs  = &configs,
  };

  if(log_init(&userdata_log)) {
    printf("error initalizing log thread: %s\n", strerror(errno));
    exit(1);
  }

  /* init user_data struct used by network_func */
  struct user_data userdata = {
    .configs = &configs,
    .pipefd  = &pipefd[0],
  };

  if(pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if(pthread_create(&network_thread, NULL, network_func, &userdata)) {
    log_fatal("creating network thread: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  // initialize semaphores and register signal handlers
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

  // disable buffering on stdout
  setvbuf(stdout, NULL, _IONBF, 0);

  // block this thread until process is interrupted
  sem_wait(&wakeup_main);

  /* tell network thread to quit, then join the thread */
  write(pipefd[1], NULL, 8);
  close(pipefd[1]);

  pthread_join(network_thread, NULL);

  log_exit();
  free_config(&configs);
  alarm(0);

  sem_post(&cleanup_done);
}

void *network_func(void *param) {
  struct user_data *userdata = param;
  int sockfd, rc, len;
  int flags;
  char *msg, buf[144];
  struct sockaddr_in addr;
  socklen_t addrlen;
  struct pollfd p[2];

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_UNIX;
  addr.sin_addr = inet_addr(configs.inet_addr);
  addr.sin_port = htons(configs.inet_port);
  addrlen = sizeof(struct sockaddr_un);

  ConnectToPeer:
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    log_error("in network_func: socket(AF_INET, SOCK_STREAM, 0) error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  flags = fcntl(sockfd, F_GETFL, 0);
  if((fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)) < 0) {
    log_fatal("in network_func: fnctl failed: %s\n", strerror(errno));
    close(sockfd);
    log_exit();
    exit(1);
  }

  if(connect(sockfd, (struct sockaddr*) &addr, addrlen) == -1) {
    log_error("in network_func: connect error: %s\n", strerror(errno));
    close(sockfd);
    log_exit();
    exit(1);
  }

  len = asprintf(&msg, "/S/rain");
  if(len < 0) {
    log_error("in network_func: asprintf error: %s\n", strerror(errno));
    close(sockfd);
    free(msg);
    log_exit();
    exit(1);
  }
  if((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
    if(rc > 0) log_error("partial write (%d of %d)\n", rc, len);
    else {
      log_error("failed to subscribe to event (send error: %s)\n", strerror(errno));
      close(sockfd);
      free(msg);
      log_exit(); // TODO: handle this error better
      exit(1);
    }
  }
  free(msg);

  memset(&p, 0, sizeof(p));
  p[0].fd = sockfd;
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = userdata->pipefd[0];

  while(1) {
    if(poll(p, 2, -1)) {
      if(p[1].revents & POLLIN|POLLPRI) {
        break;
      }

      memset(&buf, 0, sizeof(buf));
      if((rc = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        if(strncasecmp("/E/", buf, 3)) {
          log_error("in network_func: read string violating protocol (%s)\n", buf);
          continue;
        }
        size_t len = strlen(buf);
        memmove(buf, buf+3, len - 3 + 1);
        for(char *p = strtok(buf, "/E/");p != NULL;p = strtok(NULL, "/E/")) {
          if(!strncasecmp("rain", buf, strlen(buf))) {
            rawtime = malloc(sizeof(time_t));
            if(rawtime == NULL) {
              log_fatal("in network_func: memory allocation failed: (%s)\n", strerror(errno));
              // abort() ???
              continue;
            }
            time(rawtime);
            log_msg_level(LOG_LEVEL_INFO, rawtime, "ping sensor", "rain\n");
          }else if(!strncasecmp("subscribed", buf, strlen(buf))) {
            _log_debug("received message \"/E/subscribed\", sending \"/R/subscribed\" back.\n");
            len = asprintf(&msg, "/R/subscribed");
            if(len < 0) {
              log_error("in network_func: asprintf error: %s\n", strerror(errno));
              close(sockfd);
              free(msg);
              log_exit();
              exit(1);
            }
            if((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
              if(rc > 0) log_error("partial write (%d of %d)\n", rc, len);
              else {
                log_error("failed to subscribe to event (write error: %s)\n", strerror(errno));
                close(sockfd);
                free(msg);
                log_exit();
                exit(1);
              }
            }
            free(msg);
          }else {
            log_error("in network_func: unknown event (%s)\n", buf);
          }
        }
      }else if(rc < 0) {
        if(errno == EWOULDBLOCK || EAGAIN == errno) {
          log_warn("socket is non-blocking and recv was called with no data available: %s\n", strerror(errno));
          continue;
        }
        log_error("in network_func: recv error: %s\n", strerror(errno));
        log_exit();
        exit(1);
      }else if(rc == 0) {
        is_ultrasonic_enabled = 0;
        log_warn("the subscription of event \"rain\" was reset by peer.\n");
        close(sockfd);
        sleep(5);
        goto ConnectToPeer;
      }
    }
  }

  close(userdata->pipefd[0]);
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
