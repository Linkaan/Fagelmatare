#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>

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

      pch = strtok(line, DELIM);
      if(pch != NULL) {
        if(!strcmp(pch, "address") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->serv_addr = strdup(pch);
        }else if(!strcmp(pch, "username") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->username = strdup(pch);
        }else if(!strcmp(pch, "passwd") && (pch = strtok(NULL, DELIM)) != NULL) {
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
