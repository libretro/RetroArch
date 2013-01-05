#include <string.h>
#include "../../include/export/RGL/rgl.h"
#include "../../include/RGL/Types.h"
#include "../../include/RGL/Utils.h"
#include "../../include/RGL/private.h"

static const unsigned int capacityIncr = 16;

// Initialize texture namespace ns with creation and destruction functions
void rglTexNameSpaceInit(void *data, rglTexNameSpaceCreateFunction create, rglTexNameSpaceDestroyFunction destroy)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   ns->capacity = capacityIncr;
   ns->data = (void **)malloc( ns->capacity * sizeof( void* ) );
   memset( ns->data, 0, ns->capacity*sizeof( void* ) );
   ns->create = create;
   ns->destroy = destroy;
}

// Free texture namespace ns
void rglTexNameSpaceFree(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for (GLuint i = 1;i < ns->capacity; ++i)
      if (ns->data[i])
         ns->destroy( ns->data[i] );

   free( ns->data );
   ns->data = NULL;
}

// Reset all names in namespace ns to NULL
void rglTexNameSpaceResetNames(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( GLuint i = 1;i < ns->capacity;++i )
   {
      if ( ns->data[i] )
      {
         ns->destroy( ns->data[i] );
         ns->data[i] = NULL;
      }
   }
}

// Get an index of the first free name in namespace ns
GLuint rglTexNameSpaceGetFree(void *data)
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;
   GLuint i;

   for (i = 1;i < ns->capacity;++i)
      if (!ns->data[i])
         break;
   return i;
}

// Add name to namespace by increasing capacity and calling creation call back function
// Return GL_TRUE for success, GL_FALSE for failure
GLboolean rglTexNameSpaceCreateNameLazy(void *data, GLuint name )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   if (name >= ns->capacity)
   {
      int newCapacity = name >= ns->capacity + capacityIncr ? name + 1 : ns->capacity + capacityIncr;
      void **newData = ( void ** )realloc( ns->data, newCapacity * sizeof( void * ) );
      memset( newData + ns->capacity, 0, ( newCapacity - ns->capacity )*sizeof( void * ) );
      ns->data = newData;
      ns->capacity = newCapacity;
   }
   if ( !ns->data[name] )
   {
      ns->data[name] = ns->create();
      if ( ns->data[name] ) return GL_TRUE;
   }
   return GL_FALSE;
}

// Check if name is a valid name in namespace ns
// Return GL_TRUE if so, GL_FALSE otherwise
GLboolean rglTexNameSpaceIsName(void *data, GLuint name )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;
   
   if ((name > 0) && (name < ns->capacity))
      return( ns->data[name] != 0 );
   else return GL_FALSE;
}

// Generate new n names in namespace ns
void rglTexNameSpaceGenNames(void *data, GLsizei n, GLuint *names )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( int i = 0;i < n;++i )
   {
      GLuint name = rglTexNameSpaceGetFree( ns );
      names[i] = name;
      if (name)
         rglTexNameSpaceCreateNameLazy( ns, name );
   }
}

// Delete n names from namespace ns
void rglTexNameSpaceDeleteNames(void *data, GLsizei n, const GLuint *names )
{
   rglTexNameSpace *ns = (rglTexNameSpace*)data;

   for ( int i = 0;i < n;++i )
   {
      GLuint name = names[i];
      if (!rglTexNameSpaceIsName(ns, name))
         continue;
      ns->destroy( ns->data[name] );
      ns->data[name] = NULL;
   }
}
