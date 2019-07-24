#include <stdlib.h>
#include <string.h>

#include "mylist.h"
#include "mem_util.h"

void mylist_resize(MyList *list, int new_size, bool run_constructor)
{
   int new_capacity;
   int old_size;
   int i;
   void *element    = NULL;
   if (new_size < 0)
      new_size  = 0;
   if (!list)
      return;
   new_capacity = new_size;
   old_size     = list->size;

   if (new_size == old_size)
      return;

   if (new_size > list->capacity)
   {
      if (new_capacity < list->capacity * 2)
         new_capacity = list->capacity * 2;

      /* try to realloc */
      list->data = (void**)realloc(
            (void*)list->data, new_capacity * sizeof(void*));

      for (i = list->capacity; i < new_capacity; i++)
         list->data[i] = NULL;

      list->capacity = new_capacity;
   }

   if (new_size <= list->size)
   {
      for (i = new_size; i < list->size; i++)
      {
         element = list->data[i];

         if (element)
         {
            list->destructor(element);
            list->data[i] = NULL;
         }
      }
   }
   else
   {
      for (i = list->size; i < new_size; i++)
      {
         if (run_constructor)
            list->data[i] = list->constructor();
         else
            list->data[i] = NULL;
      }
   }

   list->size = new_size;
}

void *mylist_add_element(MyList *list)
{
   int old_size;

   if (!list)
      return NULL;

   old_size = list->size;
   mylist_resize(list, old_size + 1, true);
   return list->data[old_size];
}

void mylist_create(MyList **list_p, int initial_capacity,
      constructor_t constructor, destructor_t destructor)
{
   MyList *list        = NULL;

   if (!list_p)
      return;

   if (initial_capacity < 0)
      initial_capacity = 0;

   list                = *list_p;
   if (list)
      mylist_destroy(list_p);

   list               = (MyList*)malloc(sizeof(MyList));
   *list_p            = list;
   list->size         = 0;
   list->constructor  = constructor;
   list->destructor   = destructor;

   if (initial_capacity > 0)
   {
      list->data      = (void**)calloc(initial_capacity, sizeof(void*));
      list->capacity  = initial_capacity;
   }
   else
   {
      list->data      = NULL;
      list->capacity  = 0;
   }
}

void mylist_destroy(MyList **list_p)
{
   MyList *list = NULL;
   if (!list_p)
      return;

   list = *list_p;

   if (list)
   {
      mylist_resize(list, 0, false);
      free(list->data);
      free(list);
      *list_p = NULL;
   }
}

void mylist_assign(MyList *list, int index, void *value)
{
   void *old_element = NULL;

   if (index < 0 || index >= list->size)
      return;

   old_element       = list->data[index];
   list->destructor(old_element);
   list->data[index] = value;
}

void mylist_remove_at(MyList *list, int index)
{
   int i;

   if (index < 0 || index >= list->size)
      return;

   mylist_assign(list, index, NULL);

   for (i = index + 1; i < list->size; i++)
      list->data[i - 1] = list->data[i];

   list->size--;
   list->data[list->size] = NULL;
}

void mylist_pop_front(MyList *list)
{
   mylist_remove_at(list, 0);
}

void mylist_push_back(MyList *list, void *value)
{
   int old_size;
   if (!list)
      return;
   old_size = list->size;
   mylist_resize(list, old_size + 1, false);
   list->data[old_size] = value;
}
