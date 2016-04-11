/*
 *  config.c
 *    Parse configuration file used by this program.
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
        }else if(!strcmp(pch, "inet_addr") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->inet_addr = strdup(pch);
        }else if(!strcmp(pch, "inet_port") && (pch = strtok(NULL, DELIM)) != NULL) {
          char *end;

          configuration->inet_port = (int) strtol(pch, &end, 10);
          if (*end || errno == ERANGE) {
            return 1;
          }
        }else if(!strcmp(pch, "fagelmatare_log_file") && (pch = strtok(NULL, DELIM)) != NULL) {
          configuration->fagelmatare_log = strdup(pch);
        }
      }
    }
    free(line);
    fclose(stream);
  }

  return 0;
}

void free_config(struct config *configuration) {
  free(configuration->serv_addr);
  free(configuration->username);
  free(configuration->passwd);
  free(configuration->inet_addr);
  free(configuration->fagelmatare_log);
  configuration->serv_addr = NULL;
  configuration->username = NULL;
  configuration->passwd = NULL;
  configuration->inet_addr = NULL;
  configuration->fagelmatare_log = NULL;
}
