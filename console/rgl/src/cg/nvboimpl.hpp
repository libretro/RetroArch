#if !defined(CGC_CGBIO_NVBOIMPL_HPP)
#define CGC_CGBIO_NVBOIMPL_HPP 1

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "cgbio.hpp"

namespace cgc {
namespace bio {

class nvb_writer_impl : public nvb_writer
{
    public:

	nvb_writer_impl();
	virtual
	~nvb_writer_impl();

	virtual ptrdiff_t
	reference();

	virtual ptrdiff_t
	release();

	virtual CGBIO_ERROR
	save( ofstream& ofs );

	virtual CGBIO_ERROR
	save( const char* filename );

	virtual CGBIO_ERROR
	set_attr( unsigned char	file_class,
		  unsigned char	endianness,
		  unsigned char	ELFversion,
		  unsigned char abi,
		  unsigned char ABIversion,
		  Elf32_Half	type,
		  Elf32_Half	machine,
		  Elf32_Word	version,
		  Elf32_Word	flags );

	virtual Elf32_Addr
	get_entry() const;

	virtual CGBIO_ERROR
	set_entry( Elf32_Addr entry );

	virtual unsigned char
	endianness() const;

    private:
	ptrdiff_t		ref_count_;
	Elf32_Ehdr		header_;
}; // elf_writer_impl



class oparam_array_impl : public oparam_array
{
    public:
	oparam_array_impl( nvb_writer* cgbo );

	virtual
	~oparam_array_impl();

	virtual ptrdiff_t
	reference();
	virtual ptrdiff_t
	release();

	virtual CGBIO_ERROR
	add_entry();

    private:
	ptrdiff_t	ref_count_;
	nvb_writer*	cgbo_;
};


} } // bio namespace cgc namespace

#endif // CGC_CGBIO_NVBOIMPL_HPP
