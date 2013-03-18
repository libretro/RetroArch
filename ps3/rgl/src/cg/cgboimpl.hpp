#if !defined(CGC_CGBIO_CGBOIMPL_HPP)
#define CGC_CGBIO_CGBOIMPL_HPP 1

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "cgbio.hpp"

namespace cgc {
namespace bio {

class osection_impl;

class elf_writer_impl : public elf_writer
{
    public:

	elf_writer_impl();
	virtual
	~elf_writer_impl();

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

	virtual Elf32_Half
	num_of_sections() const;

	virtual osection*
	get_section( Elf32_Half index ) const;

	virtual osection*
	get_section( const char * name ) const;

	virtual osection*
	add_section( const char * name,
		     Elf32_Word type,
		     Elf32_Word flags,
		     Elf32_Word info,
		     Elf32_Word align,
		     Elf32_Word esize );

	virtual streampos
	section_offset( Elf32_Half index ) const;

	virtual CGBIO_ERROR
	new_section_out( PRODUCER, osection*, void** ) const;

    private:
	ptrdiff_t		ref_count_;
	Elf32_Ehdr		header_;
	std::vector<osection_impl*>	sections_;
}; // elf_writer_impl


class osection_impl : public osection
{
    public:
	osection_impl( Elf32_Half index,
		       elf_writer* cgbo,
		       const char * name,
		       Elf32_Word type,
		       Elf32_Word flags,
		       Elf32_Word info,
		       Elf32_Word addrAlign,
		       Elf32_Word entrySize );

	virtual
	~osection_impl();

	virtual ptrdiff_t
	reference();

	virtual ptrdiff_t
	release();

	virtual CGBIO_ERROR
	save( ofstream& of,
	      streampos header,
	      streampos data);

	virtual Elf32_Half
	index() const;

	virtual const char *
	name() const;

	virtual Elf32_Word
	type() const;

	virtual Elf32_Word
	flags() const;

	virtual Elf32_Word
	info() const;

	virtual Elf32_Word
	addralign() const;

	virtual Elf32_Word
	entsize() const;

	virtual Elf32_Word
	size() const;

	virtual Elf32_Word
	get_name_index() const;

	virtual void
	set_name_index( Elf32_Word index );

	virtual Elf32_Addr
	get_address() const;

	virtual void
	set_address( Elf32_Addr addr );

	virtual Elf32_Word
	get_link() const;

	virtual void
	set_link( Elf32_Word link );

	virtual char*
	get_data() const;

	virtual CGBIO_ERROR
	set_data( const char* data, Elf32_Word size );

	virtual CGBIO_ERROR
	add_data( const char* data, Elf32_Word size );

    private:
	Elf32_Half	index_;
	elf_writer*	cgbo_;
	Elf32_Shdr	sh_;
	char	name_[512];
	char*	data_;
};

class ostring_table_impl : public ostring_table
{
    public:
	ostring_table_impl( elf_writer* cgbo, osection* section );

	virtual
	~ostring_table_impl();

	virtual ptrdiff_t
	reference();

	virtual ptrdiff_t
	release();

	virtual const char*
	get( Elf32_Word index ) const;

	//search the string table for a given string
	virtual Elf32_Word
	find( const char *) const;

	//add the string to the table if it's not already there, return the index of the string
	virtual Elf32_Word
	addUnique( const char* str );
	
	virtual Elf32_Word
	add( const char* str );

    private:
	ptrdiff_t	ref_count_;
	elf_writer*	cgbo_;
	osection*	section_;
	std::vector<char> data_;
};

class oconst_table_impl : public oconst_table
{
public:
	oconst_table_impl( elf_writer* cgbo, osection* section );

	virtual
		~oconst_table_impl();

	virtual ptrdiff_t
		reference();

	virtual ptrdiff_t
		release();

	virtual const Elf32_Word*
		get( Elf32_Word index ) const;

	virtual Elf32_Word
		add( const Elf32_Word *data,  Elf32_Word count );

private:
	ptrdiff_t	ref_count_;
	elf_writer*	cgbo_;
	osection*	section_;
	std::vector <Elf32_Word> data_;
};

class osymbol_table_impl : public osymbol_table
{
    public:
	osymbol_table_impl( elf_writer* cgbo, osection* sec );

	virtual
	~osymbol_table_impl();

	virtual ptrdiff_t
	reference();

	virtual ptrdiff_t
	release();

	virtual Elf32_Word
	add_entry( Elf32_Word		name,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	info,
		   unsigned char	other,
		   Elf32_Half		shndx );

	virtual Elf32_Word
	add_entry( Elf32_Word		name,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	bind,
		   unsigned char	type,
		   unsigned char	other,
		   Elf32_Half		shndx );

	virtual Elf32_Word
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	info,
		   unsigned char	other,
		   Elf32_Half		shndx );

	virtual Elf32_Word
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	bind,
		   unsigned char	type,
		   unsigned char	other,
		   Elf32_Half		shndx );
    private:
	ptrdiff_t	ref_count_;
	elf_writer*	cgbo_;
	osection*	section_;
};

class orelocation_table_impl : public orelocation_table
{
    public:
	orelocation_table_impl( elf_writer* cgbo, osection* sec );

	virtual
	~orelocation_table_impl();

	virtual ptrdiff_t
	reference();

	virtual ptrdiff_t
	release();

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr offset,
		   Elf32_Word info );

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr offset,
		   Elf32_Word symbol,
		   unsigned char type );

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr offset,
		   Elf32_Word info,
		   Elf32_Sword addend );

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr offset,
		   Elf32_Word symbol,
		   unsigned char type,
		   Elf32_Sword addend );

	virtual CGBIO_ERROR
	add_entry( ostring_table* pStrWriter,
		   const char*    str,
		   osymbol_table* pSymWriter,
		   Elf32_Addr     value,
		   Elf32_Word     size,
		   unsigned char  symInfo,
		   unsigned char  other,
		   Elf32_Half     shndx,
		   Elf32_Addr     offset,
		   unsigned char  type );

	virtual CGBIO_ERROR
	add_entry( ostring_table* pStrWriter,
		   const char* str,
		   osymbol_table* pSymWriter,
		   Elf32_Addr value,
		   Elf32_Word size,
		   unsigned char symInfo,
		   unsigned char other,
		   Elf32_Half shndx,
		   Elf32_Addr offset,
		   unsigned char type,
		   Elf32_Sword addend );

    private:
	ptrdiff_t	ref_count_;
	elf_writer*	cgbo_;
	osection*	section_;
}; // CGBORelocationTableImpl

class oparam_table_impl : public oparam_table
{
    public:
	oparam_table_impl( elf_writer* cgbo, osection* sec );

	virtual
	~oparam_table_impl();

	virtual ptrdiff_t
	reference();
	virtual ptrdiff_t
	release();

	virtual CGBIO_ERROR
	add_entry( Elf32_cgParameter& cgparam );

    private:
	ptrdiff_t	ref_count_;
	elf_writer*	cgbo_;
	osection*	section_;
};


} } // bio namespace cgc namespace

#endif // CGC_CGBIO_CGBOIMPL_HPP
