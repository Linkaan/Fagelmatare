#ifndef STATE_H
#define STATE_H

void start_watching_state(pthread_t *thread, char *dir, int (*callback)(char *, char *), int read_content);
void stop_watching_state();

#endif
