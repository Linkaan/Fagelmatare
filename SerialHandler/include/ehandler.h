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
