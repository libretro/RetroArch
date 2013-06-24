#ifndef CORE_INFO_H_
#define CORE_INFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "conf/config_file.h"

typedef struct {
   char * path;
   config_file_t* data;
   char * configPath;
   char * displayName;
   char * supportedExtensions;
} core_info_t;

typedef struct {
   core_info_t *list;
   int count;
} core_info_list_t;

core_info_list_t *get_core_info_list();
void free_core_info_list(core_info_list_t * core_info_list);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
