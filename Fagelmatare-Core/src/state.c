#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <state.h>

#define NUM_EVENT_BUF 10
#define EVENT_NAME_BUF_LEN 32

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN  ( NUM_EVENT_BUF * ( EVENT_SIZE + EVENT_NAME_BUF_LEN ) )

typedef struct watch_target {
  char *dir;
  int (*callback)(char *, char *);
  int read_content;
} watch_target;

static int keep_watching = 1;
static pthread_t *watcher_thread;

void *watch_for_file_creation(watch_target *target) {
  int length, i;
  int fd;
  int wd;
  int status;
  int dir_strlen;
  char buffer[EVENT_BUF_LEN];
  char *dir = target->dir;
  int (*callback)(char *, char *) = target->callback;
  int read_content = target->read_content;
  free(target);
  dir_strlen = strlen(dir);

  //struct sigaction term_handler = {.sa_handler = sig_handler};
  //sigaction(SIGTERM, &term_handler, NULL);

  fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init error");
    exit(EXIT_FAILURE);
  }

  struct stat st;
  int err = stat(dir, &st);
  if (err == -1) {
    if (errno == ENOENT) {
      fprintf(stderr, "Error: %s directory does not exist\n", dir);
    } else {
      perror("stat error");
    }
    exit(EXIT_FAILURE);
  } else {
    if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "Error: %s is not a directory\n", dir);
      exit(EXIT_FAILURE);
    }
  }

  if (access(dir, R_OK) != 0) {
    perror("Can't access hook target directory");
    exit(EXIT_FAILURE);
  }

  uint32_t inotify_mask;
  if (read_content) {
    inotify_mask = IN_CLOSE_WRITE;
  } else {
    inotify_mask = IN_CREATE;
  }

  wd = inotify_add_watch(fd, dir, inotify_mask);

  while (keep_watching) {
    length = read(fd, buffer, EVENT_BUF_LEN);
    if (length < 0) {
      break;
    }

    i = 0;
    status = 0;
    while (i < length) {
      struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
      if (event->len) {
        if (event->mask & inotify_mask) {
          if (!(event->mask & IN_ISDIR)) { // file
            int path_len = dir_strlen + strlen(event->name) + 2;
            char *path = malloc(path_len);
            if (path == NULL) {
              perror("malloc for file path failed");
            } else {
              snprintf(path, path_len, "%s/%s", dir, event->name);

              if (read_content) {
                // Read file contents
                FILE *fp = fopen(path, "rb");
                char *content = NULL;
                if (fp) {
                  fseek(fp, 0, SEEK_END);
                  long content_len = ftell(fp);
                  fseek(fp, 0, SEEK_SET);
                  content = malloc(content_len + 1);
                  if (content) {
                    fread(content, 1, content_len, fp);
                    content[content_len] = '\0';
                  } else {
                    perror("malloc for file content failed");
                  }
                  fclose(fp);
                } else {
                  perror("fopen failed");
                }
                status = callback(event->name, content);
                free(content);
              } else {
                status = callback(event->name, NULL);
              }

              // Delete that file
              if (status && unlink(path) != 0) {
                perror("unlink failed");
              }
              free(path);
            }
          }
        }
      }
      i += EVENT_SIZE + event->len;
    }
  }

  inotify_rm_watch(fd, wd);
  close(fd);
  pthread_exit(0);
}

void start_watching_state(pthread_t *thread, char *dir, int (*callback)(char *, char *), int read_content) {
  watch_target *target = malloc(sizeof(watch_target));
  target->dir = dir;
  target->callback = callback;
  target->read_content = read_content;
  pthread_create(thread, NULL, (void * (*)(void *))watch_for_file_creation, target);
  watcher_thread = thread;
}

void stop_watching_state() {
  keep_watching = 0;
  pthread_kill(*watcher_thread, SIGTERM);
}
