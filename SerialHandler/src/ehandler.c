/*
 *  ehandler.c
 *    Dispatch events caught by Serial Handler to subscriptions on those events
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
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <ehandler.h>
#include <lstack.h>
#include <log.h>

#define DEBUG
#ifndef DEBUG
#define _log_debug(format, ...)
#else
#define _log_debug(format, ...) log_debug(format, ##__VA_ARGS__)
#endif

static pthread_mutex_t ehandler_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t queue_thread;
static lstack_t event_queue;
static event_t **events;
static size_t esize;
static size_t cap;

void remove_element(int *, int, int);
static int need_quit(pthread_mutex_t *);

void *queue_func(void *param) {
  int i, rc, len;
  char *str;
  event_t *event;

  while(!need_quit(&ehandler_mutex)) {
    if((event = lstack_pop(&event_queue)) != NULL) {
      asprintf(&str, "/E/%s", event->type);
      len = strlen(str);
      for(i = 0;i < event->ssize;i++) {
        if((rc = send((event->subscribers)[i], str, len, MSG_NOSIGNAL)) != len) {
          if(rc > 0) log_error("partial write (%d of %d)\n", rc, len);
          else {
            if(rc < 0) {
              _log_debug("closing connection to socket %d because send error: %s\n", (event->subscribers)[i], strerror(errno));
            }else {
              _log_debug("closing connection to socket %d because reading end is closed.\n", (event->subscribers)[i]);
            }
            close((event->subscribers)[i]);
            remove_element(event->subscribers, i--, (event->ssize)--);
          }
        }
      }
      free(str);
    }
    usleep(50000);
  }
  return NULL;
}

int ehandler_init(int initial_size) {
  int i;

  if(lstack_init(&event_queue, 10 + 2) != 0) {
    errno = ENOMEM;
    return -1;
  }

  pthread_mutex_lock(&ehandler_mutex);
  if(pthread_create(&queue_thread, NULL, queue_func, NULL)) {
    return -1;
  }

  cap = initial_size;
  esize = 0;
  events = malloc(initial_size * sizeof(event_t*));
  if(events == NULL){
    errno = ENOMEM;
    return -1;
  }
  for(i = 0;i < initial_size;i++) {
    events[i] = malloc(sizeof(event_t));
    if(events[i] == NULL) {
      lstack_free(&event_queue);
      for(i = 0;i < esize;i++) {
        free(events[i]);
      }
      errno = ENOMEM;
      return -1;
    }
    esize++;
  }
  esize = 0;
  return 0;
}

event_t *ehandler_insert(char *type) {
  event_t **tmp;

  if(++esize > cap) {
    tmp = realloc(events, esize * sizeof(event_t*));
    if(tmp == NULL) {
      --esize;
      errno = ENOMEM;
      return NULL;
    }else {
      events = tmp;
    }
    events[esize-1] = malloc(sizeof(event_t));
    if(events[esize-1] == NULL) {
      --esize;
      errno = ENOMEM;
      return NULL;
    }
    cap = esize;
  }

  --esize;
  if(ehandler_get(type) != NULL) {
    errno = EINVAL;
    return NULL;
  }
  ++esize;
  events[esize-1]->type = type;
  events[esize-1]->ssize = 0;
  events[esize-1]->subscribers = malloc(sizeof(int));
  if(events[esize-1]->subscribers != NULL) {
    events[esize-1]->cap = 1;
  }
  return events[esize-1];
}

event_t *ehandler_get(char *type) {
  int i;

  for(i = 0;i < esize;i++) {
    if(!strncasecmp(type, events[i]->type, strlen(events[i]->type))) {
      return events[i];
    }
  }
  return NULL;
}

int ehandler_handle(event_t *event) {
  if(lstack_push(&event_queue, event) != 0) {
    errno = ENOMEM;
    return -1;
  }
  return 0;
}

int ehandler_subscribe(char *type, int socket) {
  int *tmp, rc, len;
  char buf[32], *str = "/E/subscribed";
  struct timeval timeout;
  event_t *event;
  fd_set set;

  if((event = ehandler_get(type)) == NULL && (event = ehandler_insert(type)) == NULL) {
    _log_debug("closing connection to socket %d because ehandler_insert error: %s\n", socket, strerror(errno));
    close(socket);
    return -1;
  }

  len = strlen(str);
  if((rc = send(socket, str, len, MSG_NOSIGNAL)) != len) {
    if(rc > 0) _log_debug("closing connection to socket %d because partial write (%d of %d).\n", socket, rc, len);
    else {
      _log_debug("closing connection to socket %d because send error: %s\n", socket, strerror(errno));
    }
    close(socket);
    return -1;
  }

  FD_ZERO(&set);
  FD_SET(socket, &set);

  memset(&timeout, 0, sizeof(timeout));
  timeout.tv_sec = 1;
  timeout.tv_usec = 500000;

  rc = select(socket+1, &set, NULL, NULL, &timeout);
  if(rc < 0) {
    _log_debug("closing connection to socket %d because select error: %s\n", socket, strerror(errno));
    close(socket);
    return -1;
  }else if(rc > 0) {
    if((rc = recv(socket, buf, len, 0)) > 0) {
      if(strncasecmp(buf, "/R/subscribed", rc)) {
        _log_debug("closing connection to socket %d because read string \"%s\" isn't equal to \"/R/subscribed\"\n", socket, buf);
        close(socket);
        return -1;
      }
    }else {
      _log_debug("closing connection to socket %d because recv error: %s\n", socket, strerror(errno));
      close(socket);
      return -1;
    }
  }else {
    _log_debug("closing connection to socket %d because select timed out.\n", socket);
    close(socket);
    return -1;
  }

  if(++(event->ssize) > event->cap) {
    tmp = realloc(event->subscribers, event->ssize * sizeof(int));
    if(tmp == NULL) {
      --(event->ssize);
      _log_debug("closing connection to socket %d because out of memory\n", socket);
      close(socket);
      return ENOMEM;
    }else {
      event->subscribers = tmp;
    }
    event->cap = event->ssize;
  }

  (event->subscribers)[event->ssize-1] = socket;
  return 0;
}

void ehandler_cleanup() {
  int i, j;

  pthread_mutex_unlock(&ehandler_mutex);
  pthread_join(queue_thread,NULL);
  pthread_mutex_destroy(&ehandler_mutex);
  lstack_free(&event_queue);

  for(i = 0;i < esize;i++) {
    for(j = 0;j < events[i]->ssize;j++) {
      if((events[i]->subscribers)[j]) {
        _log_debug("closing connection to socket %d because cleanup.\n", (events[i]->subscribers)[j]);
        close((events[i]->subscribers)[j]);
      }
    }
    free(events[i]->subscribers);
    free(events[i]);
  }
  free(events);
}

void remove_element(int *array, int index, int array_length) {
   int i;

   for(i = index;i < array_length - 1;i++) array[i] = array[i + 1];
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
