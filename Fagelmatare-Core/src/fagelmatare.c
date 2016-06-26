/*
 *  fagelmatare.c
 *    Control picam and handle events dispatched from Serial Handler from slave
 *    Raspberry Pi and ATMega328-PU
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
#include <stdbool.h>
#include <unistd.h>
#include <strings.h>
#include <limits.h>
#include <errno.h>

// I/O, threads and networking
#include <fcntl.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

// Time
#include <sys/timerfd.h>
#include <sys/time.h>
#include <time.h>

// Signal, atomic and semaphore
#include <stdatomic.h>
#include <semaphore.h>
#include <signal.h>

// wiringPi
#include <wiringPi.h>

// lstack
#include <lstack.h>

#include <state.h>
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
  int *pipefd;
  int *timerfd;
  int *watchdogfd;
  struct config *configs;
};

/*
 * Semaphores used for cleanup
 */
static sem_t wakeup_main;
static sem_t cleanup_done;

static atomic_bool rec = ATOMIC_VAR_INIT(false);
static atomic_int watchdog_fails = ATOMIC_VAR_INIT(0);

#ifdef DEBUG
static struct timespec start;
static struct timespec end;
#endif

static int is_atexit_enabled;

void *network_func(void *param);
void interrupt_callback(void *param);
int send_issue(char **response, struct config *configs, char *issue);

int touch(const char *file);
void *queue_func(void *param);
void *timer_func(void *param);
void *watchdog_func(void *param);
void reset_timer(int timerfd, int secs, int isecs);

int fdutimensat(int fd, int dir, char const *file, struct timespec const ts[2], int atflag);
int on_file_create(char *filename, char *content);

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
  int err;
  int timerfd;
  int watchdogfd;
  int pipefd[2];
  struct config configs;
  pthread_t timer_thread;
  pthread_t state_thread;
  pthread_t watchdog_thread;
  pthread_t network_thread;
  lstack_t results; /* lstack_t struct used by lstack */

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
    printf("error initalizing log library: %s\n", strerror(errno));
    exit(1);
  }

  /* initialize user_data struct used by threads for synchronization */
  struct user_data userdata = {
    .configs = &configs,
    .pipefd = &pipefd[0],
    .results = &results,
    .timerfd = &timerfd,
    .watchdogfd = &watchdogfd,
  };

  /* initialize wiringpi */
  wiringPiSetup();

  /*
   * initialize new file descriptor timer used as timeout for stopping
   * video recording
   */
  if ((timerfd = timerfd_create(CLOCK_REALTIME, 0)) < 0) {
    log_fatal("timerfd_create failed: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  /* initialize new file descriptor timer used as timeout for watchdog */
  if ((watchdogfd = timerfd_create(CLOCK_REALTIME, 0)) < 0) {
    log_fatal("timerfd_create failed: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if (pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  /* init lock-free stack used by threads for synchronization */
  if ((err = lstack_init(&results, 10 + 2)) != 0) {
    log_fatal("could not initialize lstack (%d)\n", err);
    log_exit();
    exit(1);
  }

#ifdef DEBUG
  memset(&start, 0, sizeof(struct timespec));
  memset(&end, 0, sizeof(struct timespec));
#endif

  if (pthread_create(&timer_thread, NULL, timer_func, &userdata)) {
    log_fatal("creating timer thread: %s\n", strerror(errno));
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

  /*
   * register a callback function (interrupt_callback) on wiringpi. The callback
   * will be called when a
   * iterrupt is received on the pir sensor gpio pin.
   */
  if (wiringPiISR(configs.pir_input, INT_EDGE_BOTH, &interrupt_callback, &userdata) < 0) {
    log_fatal("error in wiringPiISR: %s\n", strerror(errno));
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

  // watch state folder used by picam to indicate when its recording
  start_watching_state(&state_thread, configs.state_path, on_file_create, 1);

  // turn off buffering on stdout
  setvbuf(stdout, NULL, _IONBF, 0);

  // block this thread until process is interrupted
  sem_wait(&wakeup_main);

  /*
   * write to other end of pipe to tell the threads to terminate, then cleanup
   */
  write(pipefd[1], NULL, 8);
  close(pipefd[1]);
  close(timerfd);
  close(watchdogfd);

  stop_watching_state();
  pthread_join(timer_thread, NULL);
  pthread_join(watchdog_thread, NULL);
  pthread_join(network_thread, NULL);
  lstack_free(&results);

  // stop recording if it still running
  if (atomic_load(&rec)) {
    if (touch(configs.stop_hook)) {
      log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
      atomic_store(&rec, true);
    }
  }

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
  struct sockaddr_un addr;
  socklen_t addrlen;
  fd_set myset;
  struct timeval tv;
  struct pollfd p[2];

  // populate addr struct configured with unix sockets
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, userdata->configs->sock_path, sizeof(addr.sun_path)-1);
  addrlen = sizeof(struct sockaddr_un);

  // attempt to connect to Serial Handler
  ConnectToPeer:
  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    log_error("in network_func: socket(AF_UNIX, SOCK_STREAM, 0) error: %s\n", strerror(errno));
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

  len = asprintf(&msg, "/S/templog|check_watchdog");
  if (len < 0) {
    log_error("in network_func: asprintf error: %s\n", strerror(errno));
    close(sockfd);
    free(msg);
    log_exit();
    exit(1);
  }

  // send subscription request of templog event to Serial Handler
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
    if (poll(p, 2, -1) > 0) {

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

        /*
         * For each event available, parse event type and data and handle it
         * Event format is /E/<event>:<dataid> <data>,<dataid> <data>...
         */
        char *end_event;
        for (char *p = strtok_r(buf, "/E/", &end_event); p != NULL; p = strtok_r(NULL, "/E/", &end_event)) {
          char *end_p;

          size_t len = strlen(p);
          char *event = strtok_r(p, ":", &end_p);
          char *data = strtok_r(NULL, ":", &end_p);
          if (!strncasecmp("templog", event, len)) { // handle templog event
            size_t text_size = 0;
            char *text = NULL;
            if (data) {
              char *end, *end_data;

              for (char *d = strtok_r(data, ",", &end_data); d != NULL; d = strtok_r(NULL, ",", &end_data)) {
                char *end_delim;

                char *type = strtok_r(d, " ", &end_delim);
                char *data2 = strtok_r(NULL, " ", &end_delim);
                if (!strncasecmp("NaN", data2, 3)) continue;
                int i = (int) strtol(data2, &end, 10);
                if (*end || errno == ERANGE) {
                  log_warn("in network_func: error parsing integer: %s\n", d);
                  continue;
                }

                // data is divided by 10 because decimal numbers with one decimal are represented as integers
                float f = (float)(i) / 10.0f;

                /*
                 * Handle all available data and put it into a single text which will be
                 * sent to picam and displayed in the video feed.
                 */
                if (!strcasecmp("cpu", type)) {
                  // Check if text has not already been allocated
                  if (!text) {
                    ssize_t length = snprintf(NULL, 0, "CPU \\t\\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }
                    text = malloc((length + 1) * sizeof(char));
                    if (snprintf(text, length+1, "CPU \\t\\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size = length;
                  } else {
                    // If text has already been allocated copy old data and append new data to text
                    char *tmp;

                    ssize_t length = snprintf(NULL, 0, "CPU \\t\\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }

                    tmp = realloc(text, (text_size + length + 1) * sizeof(char));
                    if (tmp == NULL) {
                      log_error("in network_func: realloc error: %s\n", strerror(errno));
                      free(text);
                      continue;
                    } else {
                      text = tmp;
                    }

                    if (snprintf(text + text_size * sizeof(char), length+1, "CPU \\t\\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size += length;
                  }
                } else if (!strcasecmp("out", type)) {
                  if (!text) {
                    ssize_t length = snprintf(NULL, 0, "OUTSIDE \\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }
                    text = malloc((length + 1) * sizeof(char));
                    if (snprintf(text, length+1, "OUTSIDE \\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size = length;
                  } else {
                    char *tmp;

                    ssize_t length = snprintf(NULL, 0, "OUTSIDE \\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }

                    tmp = realloc(text, (text_size + length + 1) * sizeof(char));
                    if (tmp == NULL) {
                      log_error("in network_func: realloc error: %s\n", strerror(errno));
                      free(text);
                      continue;
                    } else {
                      text = tmp;
                    }

                    if (snprintf(text + text_size * sizeof(char), length+1, "OUTSIDE \\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size += length;
                  }
                } else if (!strcasecmp("hpa", type)) {
                  if (!text) {
                    ssize_t length = snprintf(NULL, 0, "PRESSURE \\t%.1f hPa\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }
                    text = malloc((length + 1) * sizeof(char));
                    if (snprintf(text, length+1, "PRESSURE \\t%.1f hPa\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size = length;
                  } else {
                    char *tmp;

                    ssize_t length = snprintf(NULL, 0, "PRESSURE \\t%.1f hPa\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }

                    tmp = realloc(text, (text_size + length + 1) * sizeof(char));
                    if (tmp == NULL) {
                      log_error("in network_func: realloc error: %s\n", strerror(errno));
                      free(text);
                      continue;
                    } else {
                      text = tmp;
                    }

                    if (snprintf(text + text_size * sizeof(char), length+1, "PRESSURE \\t%.1f hPa\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size += length;
                  }
                } else if (!strcasecmp("in", type)) {
                  if (!text) {
                    ssize_t length = snprintf(NULL, 0, "INSIDE \\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }
                    text = malloc((length + 1) * sizeof(char));
                    if (snprintf(text, length+1, "INSIDE \\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size = length;
                  } else {
                    char *tmp;

                    ssize_t length = snprintf(NULL, 0, "INSIDE \\t%.1f'C\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }

                    tmp = realloc(text, (text_size + length + 1) * sizeof(char));
                    if (tmp == NULL) {
                      log_error("in network_func: realloc error: %s\n", strerror(errno));
                      free(text);
                      continue;
                    } else {
                      text = tmp;
                    }

                    if (snprintf(text + text_size * sizeof(char), length+1, "INSIDE \\t%.1f'C\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size += length;
                  }
                } else if (!strcasecmp("rh", type)) {
                  if (!text) {
                    ssize_t length = snprintf(NULL, 0, "HUMIDITY \\t%.1f %% rH\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }
                    text = malloc((length + 1) * sizeof(char));
                    if (snprintf(text, length+1, "HUMIDITY \\t%.1f %% rH\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size = length;
                  } else {
                    char *tmp;

                    ssize_t length = snprintf(NULL, 0, "HUMIDITY \\t%.1f %% rH\\n", f);
                    if (length < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      continue;
                    }

                    tmp = realloc(text, (text_size + length + 1) * sizeof(char));
                    if (tmp == NULL) {
                      log_error("in network_func: realloc error: %s\n", strerror(errno));
                      free(text);
                      continue;
                    } else {
                      text = tmp;
                    }

                    if (snprintf(text + text_size * sizeof(char), length+1, "HUMIDITY \\t%.1f %% rH\\n", f) < 0) {
                      log_warn("in network_func: snprintf failed: %s", strerror(errno));
                      free(text);
                      continue;
                    }
                    text_size += length;
                  }
                }
              }
            } else {
              continue;
            }

            if (text == NULL || text[0] == '\0') {
              free(text);
              continue;
            }

            // write text to subtitle hook which will be handled by picam
            FILE *subtitles = fopen(userdata->configs->subtitle_hook, "w");
            if (subtitles == NULL) {
              log_error("in network_func: failed to open subtitles hook for writing");
            } else {
              fprintf(subtitles,
                "text=%s\n"
                "font_name=FreeMono:style=Bold\n"
                "pt=20\n"
                "layout_align=top,left\n"
                "text_align=left\n"
                "horizontal_margin=30\n"
                "vertical_margin=30\n"
                "duration=0", text);
              fclose(subtitles);
            }
            free(text);
          } else if (!strncasecmp("check_watchdog", event, len)) { // handle check_watchdog event
            // Set watchdog timer interval to 60 secs
            reset_timer(*userdata->watchdogfd, 0, 60);
            atomic_store_explicit(&watchdog_fails, 0, memory_order_release);

            // Send core_watchdog event to server to verifiy
            send_issue(NULL, userdata->configs, "/E/core_watchdog");
          } else if (!strncasecmp("subscribed", event, len)) { // handle subscribed event
            _log_debug("received message \"/E/subscribed\", sending \"/R/subscribed\" back.\n");
            len = asprintf(&msg, "/R/subscribed");
            if (len < 0) {
              log_fatal("in network_func: asprintf error: %s\n", strerror(errno));
              close(sockfd);
              free(msg);
              log_exit();
              exit(1);
            }
            if ((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
              if (rc > 0) log_error("in network_func: partial write (%d of %d)\n", rc, len);
              else {
                log_fatal("failed to subscribe to event (write error: %s)\n", strerror(errno));
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
      } else if(rc == 0) {
        log_warn("the subscription of event \"templog\" was reset by peer.\n");
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
  struct sockaddr_un addr;
  char buf[BUFSIZ];
  char *tmp;
  char *result;
  socklen_t addrlen;
  size_t total_size, offset;

  if (issue == NULL || issue[0] == '\0') return 0;

  // create new socket and set returned file descriptor to fd
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    log_error("in send_issue: socket error: %s\n", strerror(errno));
    return 1;
  }

  // populate addr struct configured with unix sockets
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, configs->sock_path, sizeof(addr.sun_path)-1);
  addrlen = sizeof(struct sockaddr_un);

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
 * Timer function is called when timer runs out. It will stop recording.
 * This happens only once timer has not been reset within
 * the specified timer_value (which is five seconds)
 */
void *timer_func(void *param) {
  struct user_data *userdata = param;
  struct pollfd p[2];

  memset(&p, 0, sizeof(p));

  p[0].fd = *userdata->timerfd;
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = *userdata->pipefd;

  while (1) {
    if (poll(p, 2, -1) > 0) {
      // if POLLIN or POLLPRI bit is set in revents, it is time to exit
      if (p[1].revents & (POLLIN|POLLPRI)) {
        break;
      }

      // create a stop hook which will make picam stop recording
      read(*userdata->timerfd, NULL, 8);
      if (atomic_load(&rec)) {
        if (touch(userdata->configs->stop_hook)) {
          log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
          atomic_store(&rec, true);
        }
      }
    }
  }
  close(userdata->pipefd[0]);

  if (atomic_load(&rec)) {
    if (touch(userdata->configs->stop_hook)) {
      log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
      atomic_store(&rec, true);
    }
  }
  return NULL;
}

/*
 * Reset timerfd, these timers are used in
 * motion detection system and watchdog system
 */
void reset_timer(int timerfd, int secs, int isecs) {
  struct itimerspec timer_value;

  /*
   * Set timer_value to a delay of secs seconds
   * If isecs is set to a value > 0 this timer will
   * be recurring
   */
  memset(&timer_value, 0, sizeof(timer_value));
  timer_value.it_value.tv_sec = secs;
  timer_value.it_value.tv_nsec = 0;
  timer_value.it_interval.tv_sec = isecs;
  timer_value.it_interval.tv_nsec = 0;

  if (timerfd_settime(timerfd, 0, &timer_value, NULL) != 0) {
    log_error("timerfd_settime failed: %s\n", strerror(errno));
    return;
  }
}

/*
 * Watchdog function is called when watchdog timer runs out.
 * Depending on the value of watchdog_fails different actions
 * will be issued. If the value is equal to three the network
 * connection will be restarted. If the value is equal to five
 * a full system reboot will be issued.
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
    if (poll(p, 2, -1) > 0) {
      // if POLLIN or POLLPRI bit is set in revents, it is time to exit
      if (p[1].revents & (POLLIN|POLLPRI)) {
        break;
      }

      read(*userdata->watchdogfd, NULL, 8);
      int prev_value = atomic_fetch_add_explicit(&watchdog_fails, 1, memory_order_relaxed);
      switch (prev_value) {
        case 3: // Perform restart of network
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

          if (WEXITSTATUS(reboot_status)) {
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
  close(userdata->pipefd[0]);

  return NULL;
}

/*
 * callback function for any interrupts received on registered pin with wiringPi
 */
void interrupt_callback(void *param) {
  time_t *rawtime;
  struct user_data *userdata = param;
  int do_log = 1;

  // fetch current time
  rawtime = malloc(sizeof(time_t));
  if (rawtime == NULL) {
    log_fatal("in interrupt_callback: memory allocation failed: (%s)\n", strerror(errno));
    do_log = 0;
  } else {
    time(rawtime);
  }

  // log motion event to database using log library
  if (digitalRead(userdata->configs->pir_input) == HIGH) {
    // Set timer to 5 seconds and non-recurring
    reset_timer(*userdata->timerfd, 5, 0);
    if (atomic_compare_exchange_weak(&rec, (_Bool[]) { false }, true)) {
      if (touch(userdata->configs->start_hook)) {
        log_fatal("could not create start recording hook (%s)\n", strerror(errno));
        atomic_store(&rec, false);
      }
    }

    if (do_log) log_msg_level(LOG_LEVEL_INFO, rawtime, "pir sensor", "rising\n");
  } else {
    if (atomic_compare_exchange_weak(&rec, (_Bool[]) { true }, true)) {
      reset_timer(*userdata->timerfd, 5, 0);
    }

    if (do_log) log_msg_level(LOG_LEVEL_INFO, rawtime, "pir sensor", "falling\n");
  }
}

/*
 * Handle file created in picam state directory
 */
int on_file_create(char *filename, char *content) {
  if (strcmp(filename, "record") == 0) { // only interested in record file
    if (!content) {
      log_error("in on_file_create: content for file '%s' is null.\n", filename);
      return 1;
    }
    if (strcmp(content, "false") == 0) {
#ifndef DEBUG
      atomic_store(&rec, false);
#else
      if (atomic_compare_exchange_weak(&rec, (_Bool[]) { true }, false)) {
        clock_gettime(CLOCK_REALTIME, &end);
        double elapsed = (end.tv_sec-start.tv_sec)*1E9 + end.tv_nsec-start.tv_nsec;

        log_debug("recorded video of length %lf seconds\n", elapsed/1E9);
      }
#endif
    } else if (strcmp(content, "true") == 0) {
#ifdef DEBUG
      log_debug("started recording\n");

      clock_gettime(CLOCK_REALTIME, &start);
#endif
    }
#ifdef DEBUG
    else {
      log_debug("recording state changed to: %s\n", content);
    }
#endif
    return 1;
  } else {
    return 0;
  }
}

/*
 * Create file or update the time of file according to the options given.
 * Returns a negative number on fail
 */
int touch(const char *file) {
  int fd = -1;
  int open_errno = 0;
  struct stat info;
  struct timespec new_times[2];

  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  fd = open(file, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY, mode);

  /*
   * Don't save a copy of errno if it's EISDIR, since that would lead
   * touch to give a bogus diagnostic for e.g., `touch /' (assuming
   * we don't own / or have write access to it).  On Solaris 5.6,
   * and probably other systems, it is EINVAL.  On SunOS4, it's EPERM.
   */
  if (fd == -1 && errno != EISDIR && errno != EINVAL && errno != EPERM)
    open_errno = errno;

  if (stat(file, &info) < 0) {
    if (fd < 0) {
      log_warn("stat failed on file \"%s\": %s\n", file, strerror(errno));
      if (open_errno) {
        errno = open_errno;
      }
      close(fd);
      return 1;
    }
  } else {
    /* keep atime unchanged */
    new_times[0] = info.st_atim;

    /* set mtime to current time */
    clock_gettime(CLOCK_REALTIME, &new_times[1]);
  }

  if (fdutimensat(fd, AT_FDCWD, file, new_times, 0)) {
    if (open_errno) {
      errno = open_errno;
    }
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}

/* Set the access and modification time stamps of FD (a.k.a. FILE) to be
   TIMESPEC[0] and TIMESPEC[1], respectively; relative to directory DIR.
   FD must be either negative -- in which case it is ignored --
   or a file descriptor that is open on FILE.
   If FD is nonnegative, then FILE can be NULL, which means
   use just futimes (or equivalent) instead of utimes (or equivalent),
   and fail if on an old system without futimes (or equivalent).
   If TIMESPEC is null, set the time stamps to the current time.
   ATFLAG is passed to utimensat if FD is negative or futimens was
   unsupported, which can allow operation on FILE as a symlink.
   Return 0 on success, -1 (setting errno) on failure. */
int fdutimensat(int fd, int dir, char const *file, struct timespec const ts[2], int atflag) {
  int result = 1;
  if (0 <= fd)
    result = futimens(fd, ts);
  if (file && (fd < 0 || (result == -1 && errno == ENOSYS)))
    result = utimensat(dir, file, ts, atflag);
  if (result == 1) {
    errno = EBADF;
    result = -1;
  }
  return result;
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
