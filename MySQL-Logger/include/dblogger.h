#ifndef DBLOGGER_H
#define DBLOGGER_H

#include "log_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int connect_to_database(const char *address, const char *user, const char *pwd);

extern int log_to_database(log_entry *ent);

extern int disconnect(void);

#ifdef __cplusplus
}
#endif

#endif
