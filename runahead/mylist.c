#include <stdlib.h>
#include <string.h>

#include "mylist.h"
#include "mem_util.h"

void mylist_resize(MyList *list, int newSize, bool runConstructor)
{
   int newCapacity;
   int oldSize;
   int i;
   void *element    = NULL;
   if (newSize < 0)
      newSize = 0;
   if (!list)
      return;
   newCapacity = newSize;
   oldSize     = list->size;

   if (newSize == oldSize)
      return;

   if (newSize > list->capacity)
   {
      if (newCapacity < list->capacity * 2)
         newCapacity = list->capacity * 2;

      /* try to realloc */
      list->data = (void**)realloc((void*)list->data, newCapacity * sizeof(void*));

      for (i = list->capacity; i < newCapacity; i++)
         list->data[i] = NULL;

      list->capacity = newCapacity;
   }
   if (newSize <= list->size)
   {
      for (i = newSize; i < list->size; i++)
      {
         element = list->data[i];

         if (element)
         {
            list->Destructor(element);
            list->data[i] = NULL;
         }
      }
   }
   else
   {
      for (i = list->size; i < newSize; i++)
      {
         if (runConstructor)
            list->data[i] = list->Constructor();
         else
            list->data[i] = NULL;
      }
   }
   list->size = newSize;
}

void *mylist_add_element(MyList *list)
{
   int oldSize;

   if (!list)
      return NULL;

   oldSize = list->size;
   mylist_resize(list, oldSize + 1, true);
   return list->data[oldSize];
}

void mylist_create(MyList **list_p, int initialCapacity,
      constructor_t constructor, destructor_t destructor)
{
   MyList *list = NULL;
   if (!list_p)
      return;
   if (initialCapacity < 0)
      initialCapacity = 0;
   list               = *list_p;
   if (list)
      mylist_destroy(list_p);

   list               = (MyList*)malloc(sizeof(MyList));
   *list_p            = list;
   list->size         = 0;
   list->Constructor  = constructor;
   list->Destructor   = destructor;

   if (initialCapacity > 0)
   {
      list->data      = (void**)calloc(initialCapacity, sizeof(void*));
      list->capacity  = initialCapacity;
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
   void *oldElement = NULL;

   if (index < 0 || index >= list->size)
      return;

   oldElement = list->data[index];
   list->Destructor(oldElement);
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
   int oldSize;
   if (!list)
      return;
   oldSize = list->size;
   mylist_resize(list, oldSize + 1, false);
   list->data[oldSize] = value;
}
