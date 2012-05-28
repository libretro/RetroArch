#ifndef CGC_CGBIO_CGBDEFS_HPP
#define CGC_CGBIO_CGBDEFS_HPP

#include <vector>

#ifndef STL_NAMESPACE
#define STL_NAMESPACE ::std::
#endif

#define CGF_OUTPUTFROMH0 0x01
#define CGF_DEPTHREPLACE 0x02
#define CGF_PIXELKILL 0x04

typedef size_t ptrdiff_t;
typedef size_t ptrdiff_t;

typedef struct _Elf32_cgParameter {
	unsigned int cgp_name;
	unsigned int cgp_semantic;
	unsigned short cgp_default;
	unsigned short cgp_reloc;
	unsigned short cgp_resource;
	unsigned short cgp_resource_index;
	unsigned char cgp_type;
	unsigned short cgp_info;
	unsigned char unused;
} Elf32_cgParameter;

#define ET_NONE        0
#define ET_REL         1
#define ET_EXEC        2
#define ET_DYN         3
#define ET_CORE        4
#define ET_LOOS   0xFE00
#define ET_HIOS   0xFEFF
#define ET_LOPROC 0xFF00
#define ET_HIPROC 0xFFFF

#define EM_NONE         0
#define EM_RSX		0x528e 

#define EM_RSX_NONE	0

#define EI_ABIVERSION_RSX	1

#define EV_NONE    0
#define EV_CURRENT 1

#define EI_MAG0        0
#define EI_MAG1        1
#define EI_MAG2        2
#define EI_MAG3        3
#define EI_CLASS       4
#define EI_DATA        5
#define EI_VERSION     6
#define EI_OSABI       7
#define EI_ABIVERSION  8
#define EI_PAD         9
#define EI_NIDENT     16

#define ELFMAG0 0x7F
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'

#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ELFOSABI_NONE	    0
#define ELFOSABI_HPUX       1
#define ELFOSABI_NETBSD     2
#define ELFOSABI_LINUX      3
#define ELFOSABI_SOLARIS    6
#define ELFOSABI_AIX        7
#define ELFOSABI_IRIX       8
#define ELFOSABI_FREEBSD    9
#define ELFOSABI_TRU64      10
#define ELFOSABI_MODESTO    11
#define ELFOSABI_OPENBSD    12
#define ELFOSABI_CGRUNTIME  19

#define SHN_UNDEF          0
#define SHN_LORESERVE 0xFF00
#define SHN_LOPROC    0xFF00
#define SHN_HIPROC    0xFF1F
#define SHN_LOOS      0xFF20
#define SHN_HIOS      0xFF3F
#define SHN_ABS       0xFFF1
#define SHN_COMMON    0xFFF2
#define SHN_XINDEX    0xFFFF
#define SHN_HIRESERVE 0xFFFF

#define SHT_NULL                   0
#define SHT_PROGBITS               1
#define SHT_SYMTAB                 2
#define SHT_STRTAB                 3
#define SHT_RELA                   4
#define SHT_HASH                   5
#define SHT_DYNAMIC                6
#define SHT_NOTE                   7
#define SHT_NOBITS                 8
#define SHT_REL                    9
#define SHT_SHLIB                 10
#define SHT_DYNSYM                11
#define SHT_INIT_ARRAY            14
#define SHT_FINI_ARRAY            15
#define SHT_PREINIT_ARRAY         16
#define SHT_GROUP                 17
#define SHT_SYMTAB_SHNDX          18
#define SHT_LOOS          0x60000000
#define SHT_HIOS          0x6fffffff
#define SHT_LOPROC        0x70000000
#define SHT_RSX_PARAM	  0x70000000
#define SHT_RSX_SHADERTAB 0x70000001
#define SHT_RSX_FXTAB     0x70000002
#define SHT_RSX_ANNOTATE  0x70000003
#define SHT_HIPROC        0x7FFFFFFF
#define SHT_LOUSER        0x80000000
#define SHT_HIUSER        0xFFFFFFFF

#define SHF_WRITE                   0x1
#define SHF_ALLOC                   0x2
#define SHF_EXECINSTR               0x4
#define SHF_MERGE                  0x10
#define SHF_STRINGS                0x20
#define SHF_INFO_LINK              0x40
#define SHF_LINK_ORDER             0x80
#define SHF_OS_NONCONFORMING      0x100
#define SHF_GROUP                 0x200
#define SHF_TLS                   0x400
#define SHF_MASKOS           0x0ff00000
#define SHF_MASKPROC         0xF0000000

#define GRP_COMDAT          0x1
#define GRP_MASKOS   0x0ff00000
#define GRP_MASKPROC 0xf0000000

#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2
#define STB_LOOS   10
#define STB_HIOS   12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE   0
#define STT_OBJECT   1
#define STT_FUNC     2
#define STT_SECTION  3
#define STT_FILE     4
#define STT_COMMON   5
#define STT_TLS      6
#define STT_LOOS    10
#define STT_HIOS    12
#define STT_LOPROC  13
#define STT_HIPROC  15

#define STV_DEFAULT   0
#define STV_INTERNAL  1
#define STV_HIDDEN    2
#define STV_PROTECTED 3

#define STO_RSX_SHADER 0
#define STO_RSX_EFFECT 1

#define STN_UNDEF 0

#define R_RSX_NONE      0
#define R_RSX_FLOAT4	1

struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    unsigned short    e_type;
    unsigned short    e_machine;
    unsigned int    e_version;
    unsigned int    e_entry;
    unsigned int     e_phoff;
    unsigned int     e_shoff;
    unsigned int    e_flags;
    unsigned short    e_ehsize;
    unsigned short    e_phentsize;
    unsigned short    e_phnum;
    unsigned short    e_shentsize;
    unsigned short    e_shnum;
    unsigned short    e_shstrndx;
};

struct Elf32_Shdr {
    unsigned int sh_name;
    unsigned int sh_type;
    unsigned int sh_flags;
    unsigned int sh_addr;
    unsigned int  sh_offset;
    unsigned int sh_size;
    unsigned int sh_link;
    unsigned int sh_info;
    unsigned int sh_addralign;
    unsigned int sh_entsize;
};

struct Elf32_Phdr {
    unsigned int p_type;
    unsigned int  p_offset;
    unsigned int p_vaddr;
    unsigned int p_paddr;
    unsigned int p_filesz;
    unsigned int p_memsz;
    unsigned int p_flags;
    unsigned int p_align;
};

struct Elf32_Sym {
    unsigned int    st_name;
    unsigned int    st_value;
    unsigned int    st_size;
    unsigned char st_info;
    unsigned char st_other;
    unsigned short    st_shndx;
};

struct Elf32_Note {
	unsigned int	n_namesz;	/* Name size */
	unsigned int	n_descsz;	/* Content size */
	unsigned int	n_type;		/* Content type */
};


struct Elf32_Rel {
    unsigned int r_offset;
    unsigned int r_info;
};

struct Elf32_Rela {
    unsigned int  r_offset;
    unsigned int  r_info;
    signed int r_addend;
};

struct Elf32_Dyn {
    signed int d_tag;
    union {
        unsigned int d_val;
        unsigned int d_ptr;
    } d_un;
};

using std::istream;

namespace cgc {
namespace bio {

enum CGBIO_ERROR {
    CGBIO_ERROR_NO_ERROR,
    CGBIO_ERROR_LOADED,
    CGBIO_ERROR_FILEIO,
    CGBIO_ERROR_FORMAT,
    CGBIO_ERROR_INDEX,
    CGBIO_ERROR_MEMORY,
    CGBIO_ERROR_RELOC,
    CGBIO_ERROR_SYMBOL,
    CGBIO_ERROR_UNKNOWN_TYPE
};

typedef enum {
    CGBIODATANONE = ELFDATANONE,
    CGBIODATALSB = ELFDATA2LSB,
    CGBIODATAMSB = ELFDATA2MSB
} HOST_ENDIANNESS;

class elf_reader
{
    public:
	virtual ptrdiff_t	reference()	 const                  = 0;
	virtual ptrdiff_t	release()	 const			= 0;
	virtual CGBIO_ERROR	load( const char * filename )	= 0;
	virtual CGBIO_ERROR	load( istream* stream, int start )	= 0;
	virtual bool		is_initialized() const			= 0;
	virtual ~elf_reader() {}

};

class nvb_reader
{
	public:
		virtual ~nvb_reader() {}

		virtual ptrdiff_t
			reference() const = 0;

		virtual ptrdiff_t
			release() const = 0;

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
					STL_NAMESPACE vector<float>&	default_value,
					STL_NAMESPACE vector<unsigned int>& embedded_constants,
					const char**	semantic,
					int&	paramno,
					bool&	is_referenced,
					bool&	is_shared ) const = 0;

		virtual CGBIO_ERROR get_param_name( unsigned int index, const char** name, bool& is_referenced) const = 0;

};

class bin_io
{
	public:
		static const bin_io* instance();
		static void delete_instance();

		CGBIO_ERROR new_elf_reader( elf_reader** obj ) const;

		CGBIO_ERROR new_nvb_reader( nvb_reader** obj ) const;

		const char *error_string( CGBIO_ERROR error ) const;

	private:
		bin_io();
		bin_io( const bin_io& );

		static bin_io* instance_;
};


class isection
{
  public:
    virtual ~isection() = 0;

    virtual int		reference() const = 0;
    virtual int		release()   const = 0;

    virtual unsigned short	index()     const = 0;
    virtual char *name() const = 0;
    virtual unsigned int	type()      const = 0;
    virtual unsigned int	flags()     const = 0;
    virtual unsigned int	addr()      const = 0;
    virtual unsigned int	size()      const = 0;
    virtual unsigned int	link()      const = 0;
    virtual unsigned int	info()      const = 0;
    virtual unsigned int	addralign() const = 0;
    virtual unsigned int	entsize()   const = 0;
    virtual const char*	data()      const = 0;
};

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
    private:
	mutable ptrdiff_t ref_count_;
	istream*	stream_;
	bool		initialized_;
	Elf32_Ehdr	header_;
	STL_NAMESPACE vector<const isection*> sections_;
};

inline HOST_ENDIANNESS host_endianness()
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
	    STL_NAMESPACE vector<float>&	default_value,
     STL_NAMESPACE vector<unsigned int>&	embedded_constants,
		   const char ** semantic,
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
};

}
}

#endif
