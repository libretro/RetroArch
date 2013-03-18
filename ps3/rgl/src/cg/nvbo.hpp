#if !defined(CGC_CGBIO_NVBO_HPP)
#define CGC_CGBIO_NVBO_HPP 1

#include <cstddef>
#include <fstream>
#include <string>

#include "cgbdefs.hpp"

using std::ofstream;
using std::streampos;

namespace cgc {
namespace bio {

class nvb_writer {
    public:
	enum PRODUCER
	{
	    CGBO_HASH,
	    CGBO_STR,
	    CGBO_SYM,
	    CGBO_REL,
	    CGBO_PARAM,
	    CGBO_VP,
	    CGBO_FP
	};

	virtual ~nvb_writer () {}

	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual CGBIO_ERROR
	save( ofstream& ofs ) = 0;

	virtual CGBIO_ERROR
	save( const char* filename ) = 0;

	virtual CGBIO_ERROR
	set_attr( unsigned char file_class,
		  unsigned char endianness,
		  unsigned char ELFversion,
		  unsigned char abi,
		  unsigned char ABIversion,
		  Elf32_Half    type,
		  Elf32_Half    machine,
		  Elf32_Word    version,
		  Elf32_Word    flags ) = 0;

	virtual Elf32_Addr
	get_entry() const = 0;

	virtual CGBIO_ERROR
	set_entry( Elf32_Addr entry ) = 0;

	virtual unsigned char
	endianness() const = 0;
}; // elf_writer interface class

class oparam_array
{
    public:
	virtual ~oparam_array() {}	
		
	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual CGBIO_ERROR
	add_entry() = 0;
}; // oparam_table


} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_NVBO_HPP
