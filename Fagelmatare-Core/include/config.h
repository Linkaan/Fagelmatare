#ifndef CONFIG_H
#define CONFIG_H

#define DELIM " \n"

struct config {
 char *serv_addr;
 char *username;
 char *passwd;
};

int get_config(char *filename, struct config *configuration);
void free_config(struct config *configuration);
#endif
