#include <RGL/Types.h>
#include <RGL/Utils.h>
#include <RGL/private.h>
#include <Cg/CgCommon.h>

#include <string.h>

void rglInitNameSpace( rglNameSpace * name )
{
   name->data = NULL;
   name->firstFree = NULL;
   name->capacity = 0;
}

void rglFreeNameSpace( rglNameSpace * ns )
{
   // XXX should we verify all names were freed ?

   if ( ns->data ) { free( ns->data ); };
   ns->data = NULL;
   ns->capacity = 0;
   ns->firstFree = NULL;
}

static const int NAME_INCREMENT = 4;
unsigned int rglCreateName( rglNameSpace * ns, void* object )
{
   // NULL is reserved for the guard of the linked list.
   if (ns->firstFree == NULL)
   {
      // need to allocate more pointer space
      int newCapacity = ns->capacity + NAME_INCREMENT;

      // realloc the block of pointers
      void** newData = ( void** )malloc( newCapacity * sizeof( void* ) );
      if ( newData == NULL )
      {
         // XXX what should we generally do here ?
         rglCgRaiseError( CG_MEMORY_ALLOC_ERROR );
         return 0;
      }
      memcpy( newData, ns->data, ns->capacity * sizeof( void* ) );
      if ( ns->data != NULL ) free( ns->data );
      ns->data = newData;

      // initialize the pointers to the next free elements.
      // (effectively build a linked list of free elements in place)
      // treat the last item differently, by linking it to NULL
      for ( int index = ns->capacity; index < newCapacity - 1; ++index )
         ns->data[index] = ns->data + index + 1;

      ns->data[newCapacity - 1] = NULL;
      // update the first free element to the new data pointer.
      ns->firstFree = ns->data + ns->capacity;
      // update the new capacity.
      ns->capacity = newCapacity;
   }
   // firstFree is a pointer, compute the index of it
   unsigned int result = ns->firstFree - ns->data;

   // update the first free to the next free element.
   ns->firstFree = ( void** ) * ns->firstFree;

   // store the object in data.
   ns->data[result] = object;

   // offset the index by 1 to avoid the name 0
   return result + 1;
}

unsigned int rglIsName( rglNameSpace* ns, unsigned int name )
{
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

void rglEraseName( rglNameSpace* ns, unsigned int name )
{
   if (rglIsName(ns, name))
   {
      --name;
      ns->data[name] = ns->firstFree;
      ns->firstFree = ns->data + name;
   }
}
