#include <RGL/Types.h>
#include <RGL/Utils.h>
#include <RGL/private.h>
#include <Cg/CgCommon.h>

#include <string.h>

void rglInitNameSpace(void *data)
{
   rglNameSpace *name = (rglNameSpace*)data;
   name->data = NULL;
   name->firstFree = NULL;
   name->capacity = 0;
}

void rglFreeNameSpace(void *data)
{
   rglNameSpace *name = (rglNameSpace*)data;

   if (name->data)
      free(name->data);

   name->data = NULL;
   name->capacity = 0;
   name->firstFree = NULL;
}

static const int NAME_INCREMENT = 4;

unsigned int rglCreateName(void *data, void* object)
{
   rglNameSpace *name = (rglNameSpace*)data;
   // NULL is reserved for the guard of the linked list.
   if (name->firstFree == NULL)
   {
      // need to allocate more pointer space
      int newCapacity = name->capacity + NAME_INCREMENT;

      // realloc the block of pointers
      void** newData = ( void** )malloc( newCapacity * sizeof( void* ) );
      if ( newData == NULL )
      {
         // XXX what should we generally do here ?
         rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
         return 0;
      }
      memcpy( newData, name->data, name->capacity * sizeof( void* ) );

      if (name->data != NULL)
         free (name->data);

      name->data = newData;

      // initialize the pointers to the next free elements.
      // (effectively build a linked list of free elements in place)
      // treat the last item differently, by linking it to NULL
      for ( int index = name->capacity; index < newCapacity - 1; ++index )
         name->data[index] = name->data + index + 1;

      name->data[newCapacity - 1] = NULL;
      // update the first free element to the new data pointer.
      name->firstFree = name->data + name->capacity;
      // update the new capacity.
      name->capacity = newCapacity;
   }
   // firstFree is a pointer, compute the index of it
   unsigned int result = name->firstFree - name->data;

   // update the first free to the next free element.
   name->firstFree = (void**)*name->firstFree;

   // store the object in data.
   name->data[result] = object;

   // offset the index by 1 to avoid the name 0
   return result + 1;
}

unsigned int rglIsName (void *data, unsigned int name )
{
   rglNameSpace *ns = (rglNameSpace*)data;
   // there should always be a namesepace
   // 0 is never valid.
   if (RGL_UNLIKELY(name == 0))
      return 0;

   // names start numbering from 1, so convert from a name to an index
   --name;

   // test whether it is in the namespace range
   if ( RGL_UNLIKELY( name >= ns->capacity ) )
      return 0;

   // test whether the pointer is inside the data block.
   // if so, it means it is free.
   // if it points to NULL, it means it is the last free name in the linked list.
   void** value = ( void** )ns->data[name];

   if ( RGL_UNLIKELY(value == NULL ||
            ( value >= ns->data && value < ns->data + ns->capacity ) ) )
      return 0;

   // The pointer is not free and allocated, so name is a real name.
   return 1;
}

void rglEraseName(void *data, unsigned int name )
{
   rglNameSpace *ns = (rglNameSpace*)data;
   if (rglIsName(ns, name))
   {
      --name;
      ns->data[name] = ns->firstFree;
      ns->firstFree = ns->data + name;
   }
}
