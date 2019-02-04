#ifndef __MYLIST_H__
#define __MYLIST_H__

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <dynamic/dylib.h>

RETRO_BEGIN_DECLS

typedef void* (*constructor_t)(void);
typedef void(*destructor_t)(void*);

typedef struct MyList_t
{
   void **data;
   int capacity;
   int size;
   constructor_t Constructor;
   destructor_t Destructor;
} MyList;

void *mylist_add_element(MyList *list);

void mylist_resize(MyList *list, int newSize, bool runConstructor);

void mylist_create(MyList **list_p, int initialCapacity,
      constructor_t constructor, destructor_t destructor);

void mylist_destroy(MyList **list_p);

void mylist_assign(MyList *list, int index, void *value);

void mylist_remove_at(MyList *list, int index);

void mylist_pop_front(MyList *list);

RETRO_END_DECLS

#endif
