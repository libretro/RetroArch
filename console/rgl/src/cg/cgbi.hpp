#ifndef _CGC_CGBIO_CGBI_HPP
#define _CGC_CGBIO_CGBI_HPP

#include <iostream>
#include <string>
#include <cstddef>

#include "cgbdefs.hpp"


using std::istream;

namespace cgc {
namespace bio {

class elf_reader {
    public:
	virtual ptrdiff_t	reference()	 const                  = 0;
	virtual ptrdiff_t	release()	 const			= 0;
	virtual CGBIO_ERROR	load( const char * filename )	= 0;
	virtual CGBIO_ERROR	load( istream* stream, int start )	= 0;
	virtual bool		is_initialized() const			= 0;
	virtual ~elf_reader() {}

}; // elf_reader


class isection
{
  public:
    virtual ~isection() = 0;

    virtual int		reference() const = 0;
    virtual int		release()   const = 0;

    // Section info functions
    virtual Elf32_Half	index()     const = 0;
    virtual char *name() const = 0;
    virtual Elf32_Word	type()      const = 0;
    virtual Elf32_Word	flags()     const = 0;
    virtual Elf32_Addr	addr()      const = 0;
    virtual Elf32_Word	size()      const = 0;
    virtual Elf32_Word	link()      const = 0;
    virtual Elf32_Word	info()      const = 0;
    virtual Elf32_Word	addralign() const = 0;
    virtual Elf32_Word	entsize()   const = 0;
    virtual const char*	data()      const = 0;
};

} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_CGBI_HPP
