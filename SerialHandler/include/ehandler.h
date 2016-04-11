/*
 *  ehandler.c
 *    Dispatch events caught by Serial Handler to subscriptions on those events
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

#ifndef EHANDLER_H
#define EHANDLER_H

#include <sys/types.h>
#include <sys/socket.h>

typedef struct {
    char *type;
    int *subscribers;
    pthread_mutex_t mxt;
    size_t ssize, cap;
} event_t;

int     ehandler_init(int);
event_t *ehandler_insert(char *);
event_t *ehandler_get(char *);
int     ehandler_handle(event_t *, char *);
int     ehandler_subscribe(char *, int);
void    ehandler_cleanup();

#endif
