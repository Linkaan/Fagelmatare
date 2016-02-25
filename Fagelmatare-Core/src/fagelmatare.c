/*
 *  fagelmatare.c
 *    Handle P.I.R. sensor interrupts using wiringPi library and events on the
 *    ATMega328-PU via Serial Handler program
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
#include <limits.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <pthread.h>
#include <errno.h>
#include <wiringPi.h>
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
  struct config *configs;
};

static sem_t wakeup_main;
static sem_t cleanup_done;
static atomic_int fd 	= ATOMIC_VAR_INIT(0);
static atomic_bool mpir = ATOMIC_VAR_INIT(false);
static atomic_bool rec = ATOMIC_VAR_INIT(false);

#ifdef DEBUG
static struct timespec start;
static struct timespec end;
#endif

static int is_atexit_enabled;

void *network_func	(void *param);
void interrupt_callback	(void *param);

int touch           (const char *file);
void *timer_func  	(void *param);
void *queue_func	  (void *param);
void reset_timer  	(void);

int fdutimensat     (int fd, int dir, char const *file, struct timespec const ts[2], int atflag);
int on_file_create  (char *filename, char *content);

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
  pthread_t timer_thread;
  pthread_t state_thread;
  pthread_t network_thread;
  lstack_t results; /* lstack_t struct used by lstack */

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

  /* init user_data struct used by threads for synchronization */
  struct user_data userdata = {
    .configs = &configs,
    .pipefd  = &pipefd[0],
    .results = &results,
  };

  /* initialize wiringpi */
  wiringPiSetup();

  atomic_store(&fd, timerfd_create(CLOCK_REALTIME, 0));
  if(atomic_load(&fd) < 0) {
    log_fatal("timerfd_create failed: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if(pipe(pipefd) < 0) {
    log_fatal("pipe error: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  /* init lock-free stack used by threads for synchronization */
  if((err = lstack_init(&results, 10 + 2)) != 0) {
    log_fatal("could not initialize lstack (%d)\n", err);
    log_exit();
    exit(1);
  }

#ifdef DEBUG
  memset(&start, 0, sizeof(struct timespec));
  memset(&end, 0, sizeof(struct timespec));
#endif

  if(pthread_create(&timer_thread, NULL, timer_func, &userdata)) {
    log_fatal("creating timer thread: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  if(pthread_create(&network_thread, NULL, network_func, &userdata)) {
    log_fatal("creating network thread: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  /* register a callback function (interrupt_callback) on wiringpi when a
  iterrupt is received on the pir sensor gpio pin. */
  if(wiringPiISR(configs.pir_input, INT_EDGE_BOTH, &interrupt_callback, &userdata) < 0) {
    log_fatal("error in wiringPiISR: %s\n", strerror(errno));
    log_exit();
    exit(1);
  }

  // initialize a semaphore and register signal handlers
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

  start_watching_state(&state_thread, configs.state_path, on_file_create, 1);

  setvbuf(stdout, NULL, _IONBF, 0);

  // block this thread until process is interrupted
  sem_wait(&wakeup_main);

  /* write to other end of pipe to tell the threads to terminate, then join the threads */
  write(pipefd[1], NULL, 8);
  close(pipefd[1]);
  close(atomic_load(&fd));

  stop_watching_state();
  pthread_join(timer_thread, NULL);
  pthread_join(network_thread, NULL);
  lstack_free(&results);

  if(atomic_load(&rec)) {
    if(touch(configs.stop_hook)) {
      log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
      atomic_store(&rec, true);
    }
  }

  log_exit();
  free_config(&configs);
  alarm(0);

  sem_post(&cleanup_done);
}

void *network_func(void *param) {
  struct user_data *userdata = param;
  int sockfd, rc, len;
  int valopt;
  int flags;
  char *msg, buf[144];
  struct sockaddr_in addr;
  socklen_t addrlen;
  fd_set myset;
  struct timeval tv;
  struct pollfd p[2];
  time_t *rawtime;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(userdata->configs->inet_addr);
  addr.sin_port = htons(userdata->configs->inet_port);
  addrlen = sizeof(struct sockaddr_in);

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
    if(errno == EINPROGRESS) {
      _log_debug("EINPROGRESS in connect() - selecting\n");
      do {
        tv.tv_sec = 15;
        tv.tv_usec = 0;
        FD_ZERO(&myset);
        FD_SET(sockfd, &myset);
        rc = select(sockfd+1, NULL, &myset, NULL, &tv);
        if(rc < 0 && errno != EINTR) {
          log_error("in network_func: select failed: %s\n", strerror(errno));
          close(sockfd);
          log_exit();
          exit(1);
        }else if(rc > 0) {
          addrlen = sizeof(int);
          if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &addrlen) < 0) {
            log_error("in network_func: getsockopt failed: %s\n", strerror(errno));
            close(sockfd);
            log_exit();
            exit(1);
          }
          if(valopt) {
            log_error("in network_func: delayed connect() failed: %s\n", strerror(valopt));
            close(sockfd);
            log_exit();
            exit(1);
          }
          break;
        }else {
           log_warn("in network_func: select timed out, reconnecting.\n");
           close(sockfd);
           sleep(5);
           goto ConnectToPeer;
         }
      } while (1);
    }else {
      log_error("in network_func: connect error: %s\n", strerror(errno));
      close(sockfd);
      log_exit();
      exit(1);
    }
  }

  len = asprintf(&msg, "/S/templog");
  if(len < 0) {
    log_error("in network_func: asprintf error: %s\n", strerror(errno));
    close(sockfd);
    free(msg);
    log_exit();
    exit(1);
  }
  if((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
    if(rc > 0) log_error("in network_func: partial write (%d of %d)\n", rc, len);
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
      if(p[1].revents & (POLLIN|POLLPRI)) {
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
          if(!strncasecmp("templog", buf, memchr(buf, ':', len))) {
            strtok(buf, ":");
            char *data = strtok(NULL, ":");
            char *text;
            if(data) {
              char *end;

              for(char *d = strtok(data, ":");d != NULL;d = strtok(NULL, ":")) {
                char *type = strtok(d, " ");
                char *data2 = strtok(NULL, " ");
                if(!strncasecmp("NaN", data2, 3)) continue;
                int i = (int) strtol(data2, &end, 10);
                if (*end || errno == ERANGE) {
                  log_warn("in network_func: error parsing integer: %s\n", d);
                  continue;
                }else {
                  i /= 10;
                }

                if(!strcasecmp("cpu", type) {
                  if(asprintf(&text, "%sCPU: \t%.1f'C\n", text, (float)(i)/10.0f) < 0) {
                    log_warn("in network_func: asprintf failed: %s", strerror(errno));
                    free(text);
                    continue;
                  }
                }else if(!strcasecmp("out", type) {
                  if(asprintf(&text, "%sOUTSIDE: \t%.1f'C\n", text, (float)(i)/10.0f) < 0) {
                    log_warn("in network_func: asprintf failed: %s", strerror(errno));
                    free(text);
                    continue;
                  }
                }
              }
              _log_debug("caught event templog (%s)", data);
            }else {
              continue;
            }

            if(text == NULL || text[0] == '\0') {
              free(text);
              continue;
            }

            FILE *subtitles = fopen(userdata->configs->subtitle_hook, "w");
            if(subtitles == NULL) {
              log_error("in network_func: failed to open subtitles hook for writing");
            }else {
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
          }else if(!strncasecmp("subscribed", buf, len)) {
            _log_debug("received message \"/E/subscribed\", sending \"/R/subscribed\" back.\n");
            len = asprintf(&msg, "/R/subscribed");
            if(len < 0) {
              log_fatal("in network_func: asprintf error: %s\n", strerror(errno));
              close(sockfd);
              free(msg);
              log_exit();
              exit(1);
            }
            if((rc = send(sockfd, msg, len, MSG_NOSIGNAL)) != len) {
              if(rc > 0) log_error("in network_func: partial write (%d of %d)\n", rc, len);
              else {
                log_fatal("failed to subscribe to event (write error: %s)\n", strerror(errno));
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

void *timer_func(void *param) {
  struct user_data *userdata = param;
  struct pollfd p[2];

  memset(&p, 0, sizeof(p));

  p[0].fd = atomic_load(&fd);
  p[0].revents = 0;
  p[0].events = POLLIN|POLLPRI;
  p[1] = p[0];
  p[1].fd = userdata->pipefd[0];

  while (1) {
    if(poll(p, 2, -1)) {
      if(p[1].revents & (POLLIN|POLLPRI)) {
        break;
      }

      read(atomic_load(&fd), NULL, 8);
      if(atomic_load(&rec)) {
        if(touch(userdata->configs->stop_hook)) {
          log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
          atomic_store(&rec, true);
        }
      }
    }
  }
  close(userdata->pipefd[0]);

  if(atomic_load(&rec)) {
    if(touch(userdata->configs->stop_hook)) {
      log_fatal("could not create stop recording hook (%s)\n", strerror(errno));
      atomic_store(&rec, true);
    }
  }
  return NULL;
}

void reset_timer(void) {
  struct itimerspec timer_value;

  bzero(&timer_value, sizeof(timer_value));
  timer_value.it_value.tv_sec = 5;
  timer_value.it_value.tv_nsec = 0;
  timer_value.it_interval.tv_sec = 0;
  timer_value.it_interval.tv_nsec = 0;

  if(timerfd_settime(atomic_load(&fd), 0, &timer_value, NULL) != 0) {
    log_error("timerfd_settime failed: %s\n", strerror(errno));
    return;
  }
}

/*
 * callback function for any interrupts received on registered pin
 */
void interrupt_callback(void *param) {
  time_t *rawtime;
  struct user_data *userdata;
  int do_log = 1;

  userdata = param;
  rawtime = malloc(sizeof(time_t));
  if(rawtime == NULL) {
    log_fatal("in interrupt_callback: memory allocation failed: (%s)\n", strerror(errno));
    // abort() ???
    do_log = 0;
  }else {
    time(rawtime);
  }

  // add interrupt event to mysql database
  if(digitalRead(userdata->configs->pir_input) == HIGH) {
    atomic_store(&mpir, (_Bool) 1);
    reset_timer();
    if(atomic_compare_exchange_weak(&rec, (_Bool[]) { false }, true)) {
      if(touch(userdata->configs->start_hook)) {
        log_fatal("could not create start recording hook (%s)\n", strerror(errno));
        atomic_store(&rec, false);
      }
    }

    if(do_log) log_msg_level(LOG_LEVEL_INFO, rawtime, "pir sensor", "rising\n");
  }else {
    atomic_store(&mpir, (_Bool) 0);
    if(atomic_compare_exchange_weak(&rec, (_Bool[]) { true }, true)) {
      reset_timer();
    }

    if(do_log) log_msg_level(LOG_LEVEL_INFO, rawtime, "pir sensor", "falling\n");
  }
}

int on_file_create(char *filename, char *content) {
  if(strcmp(filename, "record") == 0) {
    if(!content) {
      log_error("in on_file_create: content for file '%s' is null.\n", filename);
      return 1;
    }
    if(strcmp(content, "false") == 0) {
#ifndef DEBUG
      atomic_store(&rec, false);
#else
      if(atomic_compare_exchange_weak(&rec, (_Bool[]) { true }, false)) {
        clock_gettime(CLOCK_REALTIME, &end);
        double elapsed = (end.tv_sec-start.tv_sec)*1E9 + end.tv_nsec-start.tv_nsec;

        log_debug("recorded video of length %lf seconds\n", elapsed/1E9);
      }
#endif
    }else if(strcmp(content, "true") == 0){
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
  }else {
    return 0;
  }
}

int touch(const char *file) {
  int fd = -1;
  int open_errno = 0;
  struct stat info;
  struct timespec new_times[2];

  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  fd = open(file, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY, mode);

  /* Don't save a copy of errno if it's EISDIR, since that would lead
     touch to give a bogus diagnostic for e.g., `touch /' (assuming
     we don't own / or have write access to it).  On Solaris 5.6,
     and probably other systems, it is EINVAL.  On SunOS4, it's EPERM.  */
  if(fd == -1 && errno != EISDIR && errno != EINVAL && errno != EPERM)
    open_errno = errno;

  if(stat(file, &info) < 0) {
    if(fd < 0) {
      log_warn("stat failed on file \"%s\": %s\n", file, strerror(errno));
      if(open_errno) {
        errno = open_errno;
      }
      close(fd);
      return 1;
    }
  }else {
    /* keep atime unchanged */
    new_times[0] = info.st_atim;

    /* set mtime to current time */
    clock_gettime(CLOCK_REALTIME, &new_times[1]);
  }

  if(fdutimensat(fd, AT_FDCWD, file, new_times, 0)) {
    if(open_errno) {
      errno = open_errno;
    }
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}

int fdutimensat(int fd, int dir, char const *file, struct timespec const ts[2], int atflag) {
  int result = 1;
  if(0 <= fd)
    result = futimens(fd, ts);
  if(file && (fd < 0 || (result == -1 && errno == ENOSYS)))
    result = utimensat(dir, file, ts, atflag);
  if(result == 1) {
    errno = EBADF;
    result = -1;
  }
  return result;
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
