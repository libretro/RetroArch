#ifndef _BASE_H_
#define _BASE_H_

#include <stdlib.h>
#include <new>

namespace RGL
{

   template<class T> class Vector
   {
      T* array;
      unsigned int count;
      unsigned int capacity;
      unsigned int increment;
      public:
      void * operator new( size_t size ) { return malloc( size ); }
      void * operator new( size_t /*size*/, void *p ) { return p; }
      void operator delete( void * /*ptr*/, void * /*p*/ ) { }
      Vector(): array( 0 ), count( 0 ), capacity( 0 ), increment( 4 ) {}
      ~Vector() { clear(); reallocArray( 0 ); }

      inline void setIncrement( unsigned int i ) { increment = i; }
      inline unsigned int getCount() { return count; }

      inline void reallocArray( unsigned int newCapacity )
      {
         if ( newCapacity == capacity ) return;
         if ( newCapacity > capacity ) newCapacity = ( newCapacity > capacity + increment ) ? newCapacity : ( capacity + increment );
         if ( newCapacity == 0 )
         {
            free( array );
            array = 0;
         }
         else array = static_cast<T*>( realloc( static_cast<void *>( array ), sizeof( T ) * newCapacity ) );
         capacity = newCapacity;
      }

      inline void clear()
      {
         if ( !array ) return;
         for ( unsigned int i = 0;i < count;++i )( array + i )->~T();
         count = 0;
      }

      inline unsigned int pushBack( const T &element )
      {
         if ( count + 1 > capacity ) reallocArray( count + 1 );
         new(( void * )( array + count ) ) T( element );
         return ++count;
      }

      inline void removeElement( const T &element )
      {
         for ( unsigned int i = count; i > 0; --i )
         {
            if ( array[i-1] == element )
            {
               remove( i - 1 );
               return;
            }
         }
      }

      inline void remove( unsigned int index )
      {
         ( array + index )->~T();
         --count;
         if ( count > index ) memmove( array + index, array + index + 1, ( count - index )*sizeof( T ) );
      }

      inline T *getArray() const { return array; }
      inline T& operator []( int i ) const { return array[i]; }
   };

}

#endif
