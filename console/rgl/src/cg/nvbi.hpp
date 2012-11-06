#if !defined(CGC_CGBIO_NVBI_HPP)
#define CGC_CGBIO_NVBI_HPP 1

#include "cgbdefs.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

#include <Cg/cg.h>
#include <Cg/cgBinary.h>


namespace cgc {
namespace bio {

class nvb_reader {
    public:
	virtual ~nvb_reader() {}
	
	virtual ptrdiff_t
	reference() const = 0;

	virtual ptrdiff_t
	release() const = 0;

#ifndef __CELLOS_LV2__
	virtual CGBIO_ERROR
	load( std::istream* stream, int start, bool owner = false ) = 0;
	
	virtual CGBIO_ERROR
	load( const char *filename ) = 0;
#endif

	virtual CGBIO_ERROR
	loadFromString( const char* source, size_t length ) = 0;

	virtual bool
	is_loaded() const = 0;

	virtual unsigned char
	endianness() const = 0;

	virtual CGprofile
	profile() const = 0;

	virtual unsigned int
	revision() const = 0;

	virtual unsigned int
	size() const = 0;
	
	virtual unsigned int
	number_of_params() const = 0;

	virtual unsigned int
	ucode_size() const = 0;

	virtual const char*
	ucode() const = 0;

	virtual const CgBinaryFragmentProgram*
	fragment_program() const = 0;

	virtual const CgBinaryVertexProgram*
	vertex_program() const = 0;

	virtual CGBIO_ERROR
	get_param( unsigned int index,
			CGtype&	type,
		    CGresource&	resource,
			CGenum&	variability,
			   int&	resource_index,
		   const char** name,
	    std::vector<float>&	default_value,
     std::vector<unsigned int>& embedded_constants,
		   const char**	semantic,
			CGenum&	direction,
			   int&	paramno,
			  bool&	is_referenced,
			  bool&	is_shared ) const = 0;

	virtual CGBIO_ERROR get_param_name( unsigned int index, const char** name, bool& is_referenced) const = 0;

}; // nvb_reader


} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_NVBI_HPP
