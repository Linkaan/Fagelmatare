/*
 *  config.c
 *    Parse configuration file used by this program.
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
#include <string.h>
#include <errno.h>
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
        }else if(!strcmp(pch, "socket_path") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->sock_path = strdup(pch);
        }else if(!strcmp(pch, "fagelmatare_log_file") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->fagelmatare_log = strdup(pch);
        }else if(!strcmp(pch, "state_path") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->state_path = strdup(pch);
        }else if(!strcmp(pch, "start_hook") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->start_hook = strdup(pch);
        }else if(!strcmp(pch, "stop_hook") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->stop_hook = strdup(pch);
        }else if(!strcmp(pch, "pir_gpio_input") && (pch = strtok(NULL, DELIM)) != NULL) {
          char *end;

          configuration->pir_input = (int) strtol(pch, &end, 10);
          if (*end || errno == ERANGE) {
            return 1;
          }
        }
      }

      free(line);
    }
    fclose(stream);
  }

  return 0;
}

void free_config(struct config *configuration) {
  free(configuration->serv_addr);
  free(configuration->username);
  free(configuration->passwd);
  free(configuration->sock_path);
  free(configuration->state_path);
  free(configuration->start_hook);
  free(configuration->stop_hook);
  configuration->serv_addr = NULL;
  configuration->username = NULL;
  configuration->passwd = NULL;
  configuration->sock_path = NULL;
  configuration->state_path = NULL;
  configuration->start_hook = NULL;
  configuration->stop_hook = NULL;
}
