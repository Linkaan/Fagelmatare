/*
 *  fagelmatare.c
 *    Handle sensor data and log to database and run watchdog to check
 *    connection to Fagelmatare-Core and reboot network if necessary.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <strings.h>
#include <errno.h>

// I/O, threads and networking
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <pthread.h>

// Time
#include <sys/timerfd.h>
#include <sys/time.h>
#include <time.h>

// Signal, atomic and semaphore
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>

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
  int *pipefd;
  struct config *configs;
};

/*
 * Semaphores used for cleanup
 */
static sem_t wakeup_main;
static sem_t cleanup_done;

static int is_atexit_enabled;
static int is_sensors_enabled;

void *network_func(void *param);
int send_issue(char **response, struct config *configs, char *issue);

void *watchdog_func(void *param);
void reset_timer(int timerfd);

void die(int sig);
void quit(int sig);
void cleanup(void);
void exit_handler(void);

int sem_posted(sem_t *sem) {
  int sval = 0;
  sem_getvalue(sem, &sval);
  return sval;
}

int main(void) {
  int watchdogfd;
  int pipefd[2];
  struct config configs;
  pthread_t watchdog_thread;
  pthread_t network_thread;

  /* read and parse configuration file */
  if (get_config(CONFIG_PATH, &configs)) {
    printf("could not parse configuration file: %s\n", strerror(errno));
    exit(1);
  }

  struct user_data_log userdata_log = {
    .log_level = LOG_LEVEL_WARN,
    .configs = &configs,
  };

  /* initialize log library */
  if (log_init(&userdata_log)) {
    printf("error initalizing log thread: %s\n", strerror(errno));
    exit(1);
  }

  /* initialize new file descriptor timer used as timeout for watchdog */
  if ((watchdogfd = timerfd_create(CLOCK_REALTIME, 0)) < 0) {
    log_fatal("timerfd_create failed: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  /* initialize sensors library */
  if (sensors_init()) {
    log_error("error initalizing sensors library.");
    is_sensors_enabled = 0;
  } else {
    is_sensors_enabled = 1;
  }

  /* init user_data struct used by network_func and watchdog */
  struct user_data userdata = {
    .configs = &configs,
    .pipefd = &pipefd[0],
  };

  if (pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if (pthread_create(&watchdog_thread, NULL, watchdog_func, &userdata)) {
    log_fatal("creating watchdog thread: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if (pthread_create(&network_thread, NULL, network_func, &userdata)) {
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

  // block this thread until a signal is received
  sem_wait(&wakeup_main);

  /* inform network thread to quit, then join the thread */
  write(pipefd[1], NULL, 8);
  close(pipefd[1]);
  close(watchdogfd);

  pthread_join(network_thread, NULL);

  log_exit();
  free_config(&configs);
  alarm(0);

  sem_post(&cleanup_done);
}

/*
 * network_func function runs in another thread and is used for handling
 * events from Serial Handler
 */
void *network_func(void *param) {
  struct user_data *userdata = param;
  int sockfd;
  int rc;
  int len;
  int valopt;
  int flags;
  char *msg;
  char buf[144];
  struct sockaddr_in addr;
  socklen_t addrlen;
  fd_set myset;
  struct timeval tv;
  struct pollfd p[2];
  time_t *rawtime;

  // populate addr struct configured with inet sockets
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(userdata->configs->inet_addr);
  addr.sin_port = htons(userdata->configs->inet_port);
  addrlen = sizeof(struct sockaddr_in);

  // attempt to connect to Serial Handler
  ConnectToPeer:
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    log_error("in network_func: socket(AF_INET, SOCK_STREAM, 0) error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  // set sockfd to be non-blocking
  flags = fcntl(sockfd, F_GETFL, 0);
  if ((fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)) < 0) {
    log_fatal("in network_func: fnctl failed: %s\n", strerror(errno));
    close(sockfd);
    log_exit();
    exit(1);
  }

  // connect in non-blocking mode
  if (connect(sockfd, (struct sockaddr*) &addr, addrlen) < 0) {
    if (errno == EINPROGRESS) {
      _log_debug("EINPROGRESS in connect() - selecting\n");
      do {
        tv.tv_sec = 15;
        tv.tv_usec = 0;
        FD_ZERO(&myset);
        FD_SET(sockfd, &myset);
        rc = select(sockfd+1, NULL, &myset, NULL, &tv);
        if (rc < 0 && errno != EINTR) {
          log_error("in network_func: select failed: %s\n", strerror(errno));
          close(sockfd);
          log_exit();
          exit(1);
        } else if(rc > 0) {
          addrlen = sizeof(int);
          if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &addrlen) < 0) {
            log_error("in network_func: getsockopt failed: %s\n", strerror(errno));
            close(sockfd);
            log_exit();
            exit(1);
          }
          if (valopt) {
            log_error("in network_func: delayed connect() failed: %s\n", strerror(valopt));
            close(sockfd);
            log_exit();
            exit(1);
          }
          break;
        } else {
           log_warn("in network_func: select timed out, reconnecting.\n");
           close(sockfd);
           sleep(5);
           goto ConnectToPeer;
         }
      } while (1);
    } else {
      log_error("in network_func: connect error: %s\n", strerror(errno));
      close(sockfd);
      log_exit();
      exit(1);
    }
  }

  len = asprintf(&msg, "/S/core_watchdog");
  if (len < 0) {
    log_error("in network_func: asprintf error: %s\n", strerror(errno));
    close(sockfd);
    free(msg);
    log_exit();
    exit(1);
  }

  // send subscription request of motion, temp, open_shutter and close_shutter events to Serial Handler
  if ((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
    if (rc > 0) log_error("in network_func: partial write (%d of %d)\n", rc, len);
    else {
      log_error("failed to subscribe to event (send error: %s)\n", strerror(errno));
      close(sockfd);
      free(msg);
      sleep(5);
      goto ConnectToPeer;
    }
  }
  free(msg);

  memset(&p, 0, sizeof(p));
  p[0].fd = sockfd;
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = *userdata->pipefd;

  while (1) {
    if (poll(p, 2, -1)) {
      // if POLLIN or POLLPRI bit is set in revents, it is time to exit
      if (p[1].revents & (POLLIN|POLLPRI)) {
        break;
      }

      // clear memory in buf to prevent old data persisting
      memset(&buf, 0, sizeof(buf));
      if ((rc = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        if (strncasecmp("/E/", buf, 3)) {
          log_error("in network_func: read string violating protocol (%s)\n", buf);
          continue;
        }

        for (char *p = strtok(buf, "/E/"); p != NULL; p = strtok(NULL, "/E/")) {
          size_t len = strlen(p);
          char *event = strtok(p, ":");
          char *data = strtok(NULL, ":");
          if (!strncasecmp("core_watchdog", event, len)) { // handle core_watchdog event
            // allocate memory and fetch current time and put into rawtime
            rawtime = malloc(sizeof(time_t));
            if (rawtime == NULL) {
              log_fatal("in network_func: memory allocation failed: (%s)\n", strerror(errno));
              continue;
            }
            time(rawtime);
            log_msg_level(LOG_LEVEL_INFO, rawtime, "ping sensor", "motion\n");
          } else if (!strncasecmp("subscribed", event, len)) { // handle subscribed event
            _log_debug("received message \"/E/subscribed\", sending \"/R/subscribed\" back.\n");
            len = asprintf(&msg, "/R/subscribed");
            if (len < 0) {
              log_error("in network_func: asprintf error: %s\n", strerror(errno));
              close(sockfd);
              free(msg);
              log_exit();
              exit(1);
            }
            if ((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
              if (rc > 0) log_error("in network_func: partial write (%d of %d)\n", rc, len);
              else {
                log_error("failed to subscribe to event (write error: %s)\n", strerror(errno));
                close(sockfd);
                free(msg);
                log_exit();
                exit(1);
              }
            }
            free(msg);
          } else {
            log_error("in network_func: unknown event (%s)\n", buf);
          }
        }
      } else if (rc < 0) {
        if (errno == EWOULDBLOCK || EAGAIN == errno) {
          log_warn("socket is non-blocking and recv was called with no data available: %s\n", strerror(errno));
          continue;
        }
        log_error("in network_func: recv error: %s\n", strerror(errno));
        log_exit();
        exit(1);
      } else if (rc == 0) {
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

/*
 * send_issue function sends a query to Serial Handler and if response is
 * non-zero wait for response, allocate memory and populate response
 */
int send_issue(char **response, struct config *configs, char *issue) {
  int fd;
  int rc;
  int len;
  struct sockaddr_in addr;
  char buf[BUFSIZ];
  char *tmp;
  char *result;
  socklen_t addrlen;
  size_t total_size, offset;

  if (issue == NULL || issue[0] == '\0') return 0;

  // create new socket and set returned file descriptor to fd
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    log_error("in send_issue: socket error: %s\n", strerror(errno));
    return 1;
  }

  // populate addr struct configured as inet socket
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(configs->inet_addr);
  addr.sin_port = htons(configs->inet_port);
  addrlen = sizeof(struct sockaddr_in);

  // connect in blocking mode
  if (connect(fd, (struct sockaddr*) &addr, addrlen) == -1) {
    log_error("in send_issue: connect error: %s\n", strerror(errno));
    close(fd);
    return 1;
  }

  // send query to Serial Handler
  len = strlen(issue);
  if ((rc = send(fd, issue, len, MSG_NOSIGNAL)) != len) {
    if (rc > 0) log_error("in send_issue: partial write (%d of %d)\n", rc, len);
    else {
      log_error("in send_issue: send error: %s\n", strerror(errno));
      close(fd);
      return 1;
    }
  }

  // if response pointer is invalid, we do not have to wait for response
  // tell peer to close connection
  if (response == NULL) {
    close(fd);
    return 0;
  }

  // receive response and allocate memory for it
  total_size = 0;
  offset = 0;
  result = NULL;
  while ((rc = recv(fd, buf, BUFSIZ-1, 0)) > 0) {
    buf[rc] = '\0';
    total_size += rc+1;

    // we use another pointer tmp to not "lose" the old pointer to previous data
    // (if realloc fails) thus causing a memory leak
    tmp = realloc(result, total_size * sizeof(char));
    if (tmp == NULL) {
      log_error("in send_issue: realloc error: %s\n", strerror(errno));
      free(result);
      close(fd);
      return 1;
    } else {
      result = tmp;
    }

    // concat memory block in buf to result
    memcpy(result + offset, buf, rc+1);
    offset += strnlen(buf, rc);
  }
  if (rc < 0) {
    log_error("in send_issue: recv error: %s\n", strerror(errno));
    free(result);
    close(fd);
    return 1;
  }
  *response = result;

  close(fd);
  return 0;
}

/*
 * Watchdog function is called when watchdog timer runs out (e.g interval time reached).
 * Depending on the value of watchdog_fails different actions will be issued.
 * If the value is equal to three hostapd will be restarted. If the value is
 * equal to five a full system reboot will be issued otherwise a check_watchdog
 * event will be attempted to be sent to Fagelmatare Core.
 */
void *watchdog_func(void *param) {
  struct user_data *userdata = param;
  struct pollfd p[2];

  memset(&p, 0, sizeof(p));

  p[0].fd = *userdata->watchdogfd;
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = *userdata->pipefd;

  while (1) {
    if (poll(p, 2, -1)) {
      // if POLLIN or POLLPRI bit is set in revents, it is time to exit
      if (p[1].revents & (POLLIN|POLLPRI)) {
        break;
      }

      read(*userdata->watchdogfd, NULL, 8);
      int prev_value = atomic_fetch_add_explicit(&watchdog_fails, 1, memory_order_relaxed);
      switch (prev_value) {
        case 3: // Perform restart of hostapd
        {
          int sockfd;
          struct ifreq ifr;

          if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            log_error("in watchdog_func: socket(AF_INET, SOCK_DGRAM, 0) error: %s\n", strerror(errno));
            break;
          }

          memset(&ifr, 0, sizeof(ifr));
          strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ);

          // bring wlan0 interface down
          ifr.ifr_flags &= ~IFF_UP;
          ioctl(sockfd, SIOCSIFFLAGS, &ifr);

          // wait a bit
          sleep(5);

          // bring wlan0 interface up
          ifr.ifr_flags |= IFF_UP;
          ioctl(sockfd, SIOCSIFFLAGS, &ifr);
          close(sockfd);
        }
        break;
        case 5: // Perform full reboot of system
        {
          int reboot_status;
          pid_t reboot_pid;

          if ((reboot_pid = fork()) == 0) {
              execlp("/sbin/reboot", "/sbin/reboot", NULL);
              log_fatal("in watchdog_func: execlp error: %s\n", strerror(errno));
              log_exit();
              exit(1); /* never reached if execlp succeeds. */
          }

          if (reboot_pid < 0) {
            log_error("in watchdog_func: fork error: %s\n", strerror(errno));
            log_warn("could not spawn reboot process, performing hard reset.\n");
            log_exit();
            sync();
            reboot(RB_AUTOBOOT);
          }

          waitpid(reboot_pid, &reboot_status, 0);
          if (!WIFEXITED(reboot_status)) {
            log_warn("reboot process did not exit sanely, performing hard reset.\n");
            log_exit();
            sync();
            reboot(RB_AUTOBOOT);
          }

          if (WIFEXITSTATUS(reboot_status)) {
            log_warn("reboot process exited with error, performing hard reset.\n");
            log_exit();
            sync();
            reboot(RB_AUTOBOOT);
          } else {
            log_info("performing full system reboot.\n");
          }
        }
        break;
      }
    }
  }
  close(userdata->pipefd[0]); // remember to make this thread-safe

  return NULL;
}

/*
 * Very basic signal handler
 */
void die(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    log_fatal("received signal %d (%s), exiting.\n", sig, strsignal(sig));
  }
  cleanup();
}

/*
 * Quit function is called when alarm signal was caught because
 * the program failed to perform proper exit
 */
void quit(int sig) {
  printf("unclean exit, from signal %d (%s).\n", sig, strsignal(sig));
  is_atexit_enabled = 0;
  exit(1);
}

/*
 * Exit handler
 */
void exit_handler() {
  int atexit_enabled = is_atexit_enabled;
  cleanup();
  if (atexit_enabled) {
    sem_wait(&cleanup_done);
  }
  sem_destroy(&wakeup_main);
  sem_destroy(&cleanup_done);
}

/*
 * Cleanup function
 */
void cleanup() {
  if (!is_atexit_enabled) return;
  is_atexit_enabled = 0;
  sem_post(&wakeup_main);
  alarm(5);
}
