#if !defined(CGC_CGBIO_NVBIIMPL_HPP)
#define CGC_CGBIO_NVBIIMPL_HPP 1

#include <iostream>
#include <string>
#include <vector>
#include <cstddef>

#include <Cg/cg.h>
#include <Cg/cgBinary.h>

#include "cgbio.hpp"

namespace cgc {
namespace bio {

class nvb_reader_impl : public nvb_reader
{
    public:
	nvb_reader_impl();

	virtual
	~nvb_reader_impl();

	virtual ptrdiff_t
	reference() const;

	virtual ptrdiff_t
	release() const;

#ifndef __CELLOS_LV2__
	virtual CGBIO_ERROR
	load( std::istream* stream, int start ,bool owner = false);

	virtual CGBIO_ERROR
	load( const char* filename );
#endif
	
	virtual CGBIO_ERROR
	loadFromString( const char* source, size_t length);
	
	virtual bool
	is_loaded() const;

	virtual unsigned char
	endianness() const;

	virtual CGprofile
	profile() const;

	virtual unsigned int
	revision() const;

	virtual unsigned int
	size() const;
	
	virtual unsigned int
	number_of_params() const;

	virtual unsigned int
	ucode_size() const;

	virtual const char*
	ucode() const;

	virtual const CgBinaryFragmentProgram*
	fragment_program() const;

	virtual const CgBinaryVertexProgram*
	vertex_program() const;

	virtual CGBIO_ERROR
	get_param( unsigned int index,
		        CGtype&	type,
		    CGresource&	resource,
		        CGenum&	variability,
		           int&	resource_index,
		   const char ** name,
	    std::vector<float>&	default_value,
     std::vector<unsigned int>&	embedded_constants,
		   const char ** semantic,
			CGenum&	direction,
			   int&	paramno,
			  bool&	is_referenced,
			  bool&	is_shared ) const;

	virtual CGBIO_ERROR get_param_name( unsigned int index, const char ** name ,  bool& is_referenced) const;

    private:
	mutable ptrdiff_t	ref_count_;
	int			offset_;
	bool			loaded_;
	bool			owner_;
	bool			strStream_;
	CgBinaryProgram		header_;
	unsigned char		endianness_;
	char*			image_;
}; // nvb_reader_impl

} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_NVBIIMPL_HPP
