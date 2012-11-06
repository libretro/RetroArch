#if !defined(CGC_CGBIO_CGBO_HPP)
#define CGC_CGBIO_CGBO_HPP 1

#include <cstddef>
#include <fstream>
#include <string>

#include "cgbdefs.hpp"
#include "cgbtypes.h"

using std::ofstream;
using std::streampos;

namespace cgc {
namespace bio {

class osection;

class elf_writer {
    public:
	enum PRODUCER
	{
	    CGBO_HASH,
	    CGBO_STR,
	    CGBO_SYM,
	    CGBO_REL,
	    CGBO_PARAM,
		CGBO_CONST,
	    CGBO_VP,
	    CGBO_FP
	};

	virtual ~elf_writer() {}

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

	virtual Elf32_Half
	num_of_sections() const	= 0;

	virtual osection*
	get_section( Elf32_Half index ) const = 0;

	virtual osection*
	get_section( const char* name ) const	= 0;

	virtual osection*
	add_section( const char *name,
		     Elf32_Word type,
		     Elf32_Word flags,
		     Elf32_Word info,
		     Elf32_Word align,
		     Elf32_Word esize ) = 0;

	virtual streampos
	section_offset( Elf32_Half index ) const = 0;

	virtual CGBIO_ERROR
	new_section_out( PRODUCER producer,
			 osection* sec,
			 void** obj ) const = 0;
}; // elf_writer interface class

class osection
{
    public:
	virtual ~osection() {}
	
	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual CGBIO_ERROR
	save( ofstream& of,
	      streampos header,
	      streampos data) = 0;

	virtual Elf32_Half
	index() const = 0;

	virtual const char*
	name() const = 0;

	virtual Elf32_Word
	type() const = 0;

	virtual Elf32_Word
	flags() const = 0;

	virtual Elf32_Word
	info() const = 0;

	virtual Elf32_Word
	addralign() const = 0;

	virtual Elf32_Word
	entsize() const = 0;

	virtual Elf32_Word
	size() const = 0;

	virtual Elf32_Word
	get_name_index() const = 0;

	virtual void
	set_name_index( Elf32_Word index ) = 0;

	virtual Elf32_Addr
	get_address() const = 0;

	virtual void
	set_address( Elf32_Addr addr ) = 0;

	virtual Elf32_Word
	get_link() const = 0;

	virtual void
	set_link( Elf32_Word link ) = 0;

	virtual char*
	get_data() const = 0;

	virtual CGBIO_ERROR
	set_data( const char* data, Elf32_Word size ) = 0;

	virtual CGBIO_ERROR
	add_data( const char* data, Elf32_Word size ) = 0;

}; // osection interface class


class ostring_table
{
    public:
	virtual ~ostring_table() {}
	
	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual const char*
	get( Elf32_Word index ) const = 0;

	//search the string table for a given string
	virtual Elf32_Word
	find( const char *) const = 0;

	//add the string to the table if it's not already there, return the index of the string
	virtual Elf32_Word
	addUnique( const char* str ) = 0;

	virtual Elf32_Word
	add( const char* str ) = 0;
}; // ostring_table

class oconst_table
{
public:
	virtual ~oconst_table() {}

	virtual ptrdiff_t
		reference() = 0;

	virtual ptrdiff_t
		release() = 0;

	virtual const Elf32_Word*
		get( Elf32_Word index ) const = 0;

	virtual Elf32_Word
		add( const Elf32_Word *data,  Elf32_Word size ) = 0;
};

class osymbol_table
{
	public:

	virtual ~osymbol_table() {} 

	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual Elf32_Word
	add_entry( Elf32_Word		name,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	info,
		   unsigned char	other,
		   Elf32_Half		shndx ) = 0;

	virtual Elf32_Word
	add_entry( Elf32_Word		name,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	bind,
		   unsigned char	type,
		   unsigned char	other,
		   Elf32_Half		shndx ) = 0;

	virtual Elf32_Word
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	info,
		   unsigned char	other,
		   Elf32_Half		shndx ) = 0;

	virtual Elf32_Word
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	bind,
		   unsigned char	type,
		   unsigned char	other,
		   Elf32_Half		shndx ) = 0;
}; // osymbol_table


class orelocation_table
{
    public:
	virtual ~orelocation_table() {}
	
	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr		offset,
		   Elf32_Word		info ) = 0;

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr		offset,
		   Elf32_Word		symbol,
		   unsigned char	type ) = 0;

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr		offset,
		   Elf32_Word		info,
		   Elf32_Sword		addend ) = 0;

	virtual CGBIO_ERROR
	add_entry( Elf32_Addr		offset,
		   Elf32_Word		symbol,
		   unsigned char	type,
		   Elf32_Sword		addend ) = 0;

	virtual CGBIO_ERROR
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   osymbol_table*	symtab,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	symInfo,
		   unsigned char	other,
		   Elf32_Half		shndx,
		   Elf32_Addr		offset,
		   unsigned char	type ) = 0;

	virtual CGBIO_ERROR
	add_entry( ostring_table*	strtab,
		   const char*		str,
		   osymbol_table*	symtab,
		   Elf32_Addr		value,
		   Elf32_Word		size,
		   unsigned char	symInfo,
		   unsigned char	other,
		   Elf32_Half		shndx,
		   Elf32_Addr		offset,
		   unsigned char	type,
		   Elf32_Sword		addend ) = 0;
}; // orelocation_table


class oparam_table
{
    public:
	virtual ~oparam_table() {}

	virtual ptrdiff_t
	reference() = 0;

	virtual ptrdiff_t
	release() = 0;

	virtual CGBIO_ERROR
	add_entry( Elf32_cgParameter& cgparam ) = 0;
}; // oparam_table


} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_CGBO_HPP
