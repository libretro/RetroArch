#ifndef VIDEO_LAYOUT_SCOPE_H
#define VIDEO_LAYOUT_SCOPE_H

#include "view.h"
#include "element.h"

#define SCOPE_BUFFER_SIZE 256

typedef struct param param_t;

typedef struct scope
{
   int        level;

   param_t   *param;

   element_t *elements;
   int        elements_count;

   view_t    *groups;
   int        groups_count;

   char       eval[SCOPE_BUFFER_SIZE];
} scope_t;

void        scope_init         (scope_t *scope);
void        scope_deinit       (scope_t *scope);
void        scope_push         (scope_t *scope);
void        scope_pop          (scope_t *scope);
void        scope_repeat       (scope_t *scope);

void        scope_param        (scope_t *scope, const char *name, const char *value);
void        scope_generator    (scope_t *scope, const char *name, const char *start, const char *increment, const char *lshift, const char *rshift);
const char *scope_eval         (scope_t *scope, const char *src);

element_t  *scope_add_element  (scope_t *scope);
element_t  *scope_find_element (scope_t *scope, const char *name);

view_t     *scope_add_group    (scope_t *scope);
view_t     *scope_find_group   (scope_t *scope, const char *name);

#endif
