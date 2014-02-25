#ifndef CGC_CGBIO_CGBUTILS_HPP
#define CGC_CGBIO_CGBUTILS_HPP

#include "cgbdefs.hpp"

#define ELF32_ST_BIND(idx) ( idx >> 4 )
#define ELF32_ST_TYPE(idx) ( idx & 0xf )
#define ELF32_ST_INFO(b, t) (( b << 4 ) + ( t & 0xf ))
#define ELF32_ST_VISIBILITY(o) ( o & 0x3 )

namespace cgc
{
   namespace bio
   {
      typedef enum
      {
         CGBIODATANONE = ELFDATANONE,
         CGBIODATALSB = ELFDATA2LSB,
         CGBIODATAMSB = ELFDATA2MSB
      } HOST_ENDIANNESS; // endianness

      inline HOST_ENDIANNESS host_endianness(void)
      {
         const int ii = 1;
         const char* cp = (const char*) &ii;
         return ( cp[0] == 1 ) ? CGBIODATALSB : CGBIODATAMSB;
      }

      template< typename T > inline T convert_endianness( const T value, unsigned char endianness )
      {
         if ( host_endianness() == endianness )
            return value;

         if ( sizeof( T ) == 1 )
            return value;

         if ( sizeof( T ) == 2 )
            return ( ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8) );

         if ( sizeof( T ) == 4 )
            return ( ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8)
                  | ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24) );

         if ( sizeof( T ) == 8 )
         {
            T result = value;
            for ( int ii = 0; ii < 4; ++ii )
            {
               char ch = *( (( char* ) &result) + ii );
               *( (( char* ) &result) +      ii  ) = *( (( char* ) &result) + (7 - ii) );
               *( (( char* ) &result) + (7 - ii) ) = ch;
            }
            return result;
         }

         // exception
         return value;
      }
   } // bio namespace
} // cgc namespace


#endif // CGC_CGBIO_CGBUTILS_HPP
