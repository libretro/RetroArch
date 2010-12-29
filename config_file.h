#ifndef __CONFIG_FILE_H
#define __CONFIG_FILE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct config_file config_file_t;

config_file_t *config_file_new(const char *path);
void config_file_free(config_file_t *conf);

bool config_get_double(config_file_t *conf, const char *entry, double *in);
bool config_get_int(config_file_t *conf, const char *entry, int *in);
bool config_get_char(config_file_t *conf, const char *entry, char *in);
bool config_get_string(config_file_t *conf, const char *entry, char **in);
bool config_get_bool(config_file_t *conf, const char *entry, bool *in);



#endif
