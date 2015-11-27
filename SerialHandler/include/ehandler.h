#ifndef EHANDLER_H
#define EHANDLER_H

#include <sys/types.h>
#include <sys/socket.h>

typedef struct {
    char *type;
    int *subscribers;
    size_t ssize, cap;
} event_t;

int     ehandler_init(int);
event_t *ehandler_insert(char *);
event_t *ehandler_get(char *);
int     ehandler_handle(event_t *);
int     ehandler_subscribe(char *, int);
void    ehandler_cleanup();

#endif
