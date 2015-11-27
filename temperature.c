#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <my_global.h>
#include <mysql.h>

#define SUBTITLE_HOOK "/usr/src/picam/hooks/subtitle"
#define CONFIG_PATH "/etc/fagelmatare.conf"
#define SOCKET_PATH "/tmp/shandler.sock"

struct config {
 char *serv_addr;
 char *username;
 char *passwd;
};

int get_config(char *filename, struct config *configuration);
void free_config(struct config *configuration);
char* make_message(const char *fmt, ...);

int main(void) {
  MYSQL *mysql;
  FILE *subtitles;
  struct sockaddr_un addr;
  struct config configs;
  struct timeval timeout;
  char *msg, buf[8];
  char *query;
  int cpu_temperature, out_temperature;
  int len,fd,rc, i;
  fd_set set;

  /* parse configuration file */
  if(get_config(CONFIG_PATH, &configs)) {
    perror("could not parse configuration file");
    exit(EXIT_FAILURE);
  }

  memset(&buf, 0, sizeof(buf));

  mysql = mysql_init(NULL);
  if(NULL == mysql) {
    fprintf(stderr, "%s\n", mysql_error(mysql));
    free_config(&configs);
    exit(EXIT_FAILURE);
  }

  if(NULL == mysql_real_connect(mysql, configs.serv_addr, configs.username, configs.passwd, "fagelmatare", 0, NULL, 0)) {
    mysql_close(mysql);
    fprintf(stderr, "%s\n", mysql_error(mysql));
    free_config(&configs);
    exit(EXIT_FAILURE);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

  for(i = 0; i < 2; ++i) {
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      mysql_close(mysql);
      break;
    }

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      perror("connect error");
      mysql_close(mysql);
      close(fd);
      break;
    }

    msg = make_message("1;temperature");
    len = strlen(msg);
    if((rc = send(fd, msg, len, MSG_NOSIGNAL)) != len) {
      if(rc > 0) fprintf(stderr, "partial write");
      else {
        if(rc < 0) {
          perror("write error");
        }else {
          fprintf(stderr, "connection was closed unexpectedly.\n");
        }
        mysql_close(mysql);
        close(fd);
        free(msg);
        break;
      }
    }
    free(msg);

    FD_ZERO(&set);
    FD_SET(fd, &set);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    rc = select(fd+1, &set, NULL, NULL, &timeout);
    if(rc == -1) {
      perror("select failed");
      out_temperature = -1;
    }else if(rc > 0) {
      if((rc = recv(fd, buf, sizeof(buf), 0)) > 0) {
        char *end;

        if(!strncmp("-1", buf, 2)) { // TODO change protocol to add prefix temp=
          out_temperature = -1;
          fprintf(stderr, "SerialHandler returned negative one, resending request...\n");
          close(fd);
          continue;
        }
        out_temperature = (int) strtol(buf, &end, 10);
        if (*end || errno == ERANGE) {
          out_temperature = -1;
          fprintf(stderr, "error parsing SerialHandler return: %s\n", buf);
          close(fd);
          continue;
        }
        asprintf(&query,
          "INSERT INTO `temperatur` ("
          "`source`,`temperature`, `datetime`"
          ") VALUES ("
          "'Outside', '%s', NOW()"
          ")", buf);

        if(mysql_query(mysql, query)) {
          fprintf(stderr, "%s\n", mysql_error(mysql));
        }
        free(query);
      }else if(rc < 0) {
        out_temperature = -1;
        perror("recv failed");
        close(fd);
        break;
      }
    }else {
      fprintf(stderr, "select timeout expired\n");
      close(fd);
      continue;
    }
    close(fd);
    break;
  }
  fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
  if(fd < 0) {
    perror("failed to read temperature file");
    cpu_temperature = -1;
  }else {
    if((rc = read(fd, buf, sizeof(buf))) > 0) {
      char *end;

      strtok(buf, " \n");
      cpu_temperature = (int) strtol(buf, &end, 10);
      if (*end || errno == ERANGE) {
        fprintf(stderr, "error parsing temperature file: %s\n", buf);
        cpu_temperature = -1;
      }else {
        cpu_temperature = (cpu_temperature + 50) / 100;
        asprintf(&query,
          "INSERT INTO `temperatur` ("
          "`source`,`temperature`, `datetime`"
          ") VALUES ("
          "'CPU', '%d', NOW()"
          ")", cpu_temperature);

        if(mysql_query(mysql, query)) {
          fprintf(stderr, "%s\n", mysql_error(mysql));
        }
        free(query);
      }
    }else if(rc < 0) {
      perror("read failed");
    }
  }
  close(fd);
  mysql_close(mysql);
  free_config(&configs);

  subtitles = fopen(SUBTITLE_HOOK, "w");
  if(subtitles == NULL) {
    perror("failed to open subtitles hook for writing");
  }else {
    fprintf(subtitles,
      "text=OUTSIDE %.1f'C\\nCPU %.1f'C\n"
      "font_name=FreeMono:style=Bold\n"
      "pt=12\n"
      "layout_align=top,left\n"
      "text_align=left\n"
      "horizontal_margin=30\n"
      "vertical_margin=30\n"
      "duration=0" , out_temperature / 10.0f, cpu_temperature / 10.0f);
    fclose(subtitles);
  }
  return 0;
}

int get_config(char *filename, struct config *configuration) {
  FILE *stream;

  if((stream = fopen(filename, "r")) == NULL) {
    return 1;
  }else {
    char *line;
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, stream)) != -1) {
      char *pch;

      pch = strtok(line, " \n");
      if(pch != NULL) {
        if(!strcmp(pch, "address") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->serv_addr = strdup(pch);
        }else if(!strcmp(pch, "username") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->username = strdup(pch);
        }else if(!strcmp(pch, "passwd") && (pch = strtok(NULL, " \n")) != NULL) {
          configuration->passwd = strdup(pch);
        }
      }
    }
    fclose(stream);
  }

  return 0;
}

void free_config(struct config *configuration) {
  free(configuration->serv_addr);
  free(configuration->username);
  free(configuration->passwd);
  configuration->serv_addr = NULL;
  configuration->username = NULL;
  configuration->passwd = NULL;
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
