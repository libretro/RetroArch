#if !defined(CGC_CGBIO_CGBUTILS_HPP)
#define CGC_CGBIO_CGBUTILS_HPP 1

#include "cgbdefs.hpp"

namespace cgc {
namespace bio {

typedef enum {
    CGBIODATANONE = ELFDATANONE,
    CGBIODATALSB = ELFDATA2LSB,
    CGBIODATAMSB = ELFDATA2MSB
} HOST_ENDIANNESS; // endianness

inline HOST_ENDIANNESS
host_endianness()
{
    const int ii = 1;
    const char* cp = (const char*) &ii;
    return ( 1 == cp[0] ) ? CGBIODATALSB : CGBIODATAMSB;
}

template< typename T > inline T
convert_endianness( const T value, unsigned char endianness )
{
    if ( host_endianness() == endianness )
    {
	return value;
    }
    if ( sizeof( T ) == 1 )
    {
	return value;
    }
    if ( sizeof( T ) == 2 )
    {
	return ( ((value & 0x00FF) << 8)
	       | ((value & 0xFF00) >> 8) );
    }
    if ( sizeof( T ) == 4 )
    {
	return ( ((value & 0x000000FF) << 24)
	       | ((value & 0x0000FF00) << 8)
	       | ((value & 0x00FF0000) >> 8)
	       | ((value & 0xFF000000) >> 24) );
    }
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

template< typename T > inline T
ELF32_ST_BIND( const T idx )
{
    return ( idx >> 4 );
}

template< typename T > inline T
ELF32_ST_TYPE( const T idx )
{
    return ( idx & 0xf );
}

template< typename T > inline T
ELF32_ST_INFO( const T b, const T t )
{
    return ( ( b << 4 ) + ( t & 0xf ) );
}

template< typename T > inline T
ELF32_ST_VISIBILITY( const T o )
{
    return ( o & 0x3 );
}

// these are not used in our relocations, will be replaced by the ones
// we design, will be defined as templates.
#ifndef ELF32_R_SYM
#define ELF32_R_SYM(i) ((i)>>8)
#endif

#ifndef ELF32_R_TYPE
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#endif

#ifndef ELF32_R_INFO
#define ELF32_R_INFO(s,t) (((s)<<8 )+(unsigned char)(t))
#endif

} // bio namespace
} // cgc namespace


#endif // CGC_CGBIO_CGBUTILS_HPP
