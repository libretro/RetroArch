#if !defined(CGC_CGBIO_CGBIIMPL_HPP)
#define CGC_CGBIO_CGBIIMPL_HPP 1

#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

#include "cgbio.hpp"

using std::istream;

namespace cgc {
namespace bio {

class elf_reader_impl : public elf_reader
{
    public:
	elf_reader_impl();
	virtual ~elf_reader_impl();
	virtual CGBIO_ERROR load( const char* filename );
	virtual CGBIO_ERROR load( istream* stream, int start );
	virtual bool	  is_initialized() const;
	virtual ptrdiff_t reference()      const;
	virtual ptrdiff_t release()	   const;

//	virtual int	  get_revision()   const;

    private:
	mutable ptrdiff_t ref_count_;
	istream*	stream_;
	bool		initialized_;
	Elf32_Ehdr	header_;
   std::vector<const isection*> sections_;
}; // CGBIImpl
    
} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_CGBIIMPL_HPP
