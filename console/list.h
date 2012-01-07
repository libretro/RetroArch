#ifndef SGUI_LIST_H__
#define SGUI_LIST_H__

#include "sgui.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sgui_list sgui_list_t;

sgui_list_t *sgui_list_new(void);
void sgui_list_free(sgui_list_t *list);

void sgui_list_push(sgui_list_t *list, const char *path, sgui_file_type_t type);
void sgui_list_pop(sgui_list_t *list);
void sgui_list_clear(sgui_list_t *list);

bool sgui_list_empty(const sgui_list_t *list);
void sgui_list_back(const sgui_list_t *list,
      const char **path, sgui_file_type_t *type);

size_t sgui_list_size(const sgui_list_t *list);
void sgui_list_at(const sgui_list_t *list, size_t index,
      const char **path, sgui_file_type_t *type);

void sgui_list_sort(sgui_list_t *list);

#ifdef __cplusplus
}
#endif
#endif

