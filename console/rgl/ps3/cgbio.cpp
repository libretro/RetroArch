#include <string>
#include <cstddef>

#include <Cg/cgc.h>
#include <Cg/cgGL.h>
#include <Cg/cgBinary.h>

#include "cgbio.hpp"

using std::fill_n;

namespace cgc {
namespace bio {

bin_io* bin_io::instance_ = 0;

bin_io::bin_io()
{
}

bin_io::bin_io( const bin_io& )
{
}

const bin_io* bin_io::instance()
{
    if ( 0 == instance_ )
    {
	instance_ = new bin_io;
    }
    return instance_;
}

void bin_io::delete_instance()
{
	if ( 0 != instance_ )
	{
		delete instance_;
		instance_ = 0;
	}
}

CGBIO_ERROR bin_io::new_elf_reader( elf_reader** obj ) const
{
    CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
    *obj = new elf_reader_impl;

    if ( 0 == *obj )
	ret = CGBIO_ERROR_MEMORY;

    return ret;
}

const char*
bin_io::error_string( CGBIO_ERROR error ) const
{
    switch ( error )
    {
	case CGBIO_ERROR_NO_ERROR:
	    return "No error";
	case CGBIO_ERROR_LOADED:
	    return "Binary file has been loaded earlier";
	case CGBIO_ERROR_FILEIO:
	    return "File input output error";
	case CGBIO_ERROR_FORMAT:
	    return "File format error";
	case CGBIO_ERROR_INDEX:
	    return "Index is out of range";
	case CGBIO_ERROR_MEMORY:
	    return "Can't allocate memory";
	case CGBIO_ERROR_RELOC:
	    return "Relocation error";
	case CGBIO_ERROR_SYMBOL:
	    return "Symbol error";
	case CGBIO_ERROR_UNKNOWN_TYPE:
	    return "Uknown type";
	default:
	    return "Unknown error";
    }
    return "Unknown error";
}

CGBIO_ERROR
bin_io::new_nvb_reader( nvb_reader** obj ) const
{
    CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
    *obj = new nvb_reader_impl;

    if ( *obj == 0 )
	ret = CGBIO_ERROR_MEMORY;

    return ret;
}

nvb_reader_impl::nvb_reader_impl()
{
    ref_count_ = 1;
    offset_ = 0;
    image_ = 0;
    owner_ = false;
    loaded_ = false;
    endianness_ = host_endianness();
    std::fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
}

nvb_reader_impl::~nvb_reader_impl()
{
    if ( image_ != 0 )
	delete [] image_;
}

ptrdiff_t
nvb_reader_impl::reference() const
{
    return ++ref_count_;
}

ptrdiff_t
nvb_reader_impl::release() const
{
    ptrdiff_t ret = --ref_count_;

    if ( ref_count_ == 0 )
        delete this;

    return ret;
}

CGBIO_ERROR
nvb_reader_impl::loadFromString( const char* source, size_t length)
{
	if ( loaded_ )
	{
		return CGBIO_ERROR_LOADED;
	}
	CGBIO_ERROR ret = CGBIO_ERROR_NO_ERROR;
	while (1)
	{
		if (length < sizeof( header_ ))
		{
			ret = CGBIO_ERROR_FORMAT;
			break;
		}
		memcpy(&header_ ,source,sizeof( header_ ));
		if ( CG_BINARY_FORMAT_REVISION != header_.binaryFormatRevision )
		{
			endianness_ = ( CGBIODATALSB == endianness_ ) ? CGBIODATAMSB : CGBIODATALSB;
			int binaryRevision = convert_endianness( header_.binaryFormatRevision, endianness_ );
			if ( CG_BINARY_FORMAT_REVISION != binaryRevision )
			{
				ret = CGBIO_ERROR_FORMAT;
				break;
			}
		}
		size_t sz = length;
		image_ = new char[sz];
		memcpy(image_,source,sz);
		loaded_ = true;
		ret = CGBIO_ERROR_NO_ERROR;
		break;
	}
	return ret;
}

bool
nvb_reader_impl::is_loaded() const
{
    return loaded_;
}

unsigned char
nvb_reader_impl::endianness() const
{
    return endianness_;
}

CGprofile
nvb_reader_impl::profile() const
{
    return (CGprofile) convert_endianness( (unsigned int) header_.profile, endianness() );
}

unsigned int
nvb_reader_impl::revision() const
{
    return convert_endianness( header_.binaryFormatRevision, endianness() );
}

unsigned int nvb_reader_impl::size() const
{
    return convert_endianness( header_.totalSize, endianness() );
}

unsigned int nvb_reader_impl::number_of_params() const
{
    return convert_endianness( header_.parameterCount, endianness() );
}

unsigned int nvb_reader_impl::ucode_size() const
{
    return convert_endianness( header_.ucodeSize, endianness() );
}

const char* nvb_reader_impl::ucode() const
{
    if ( 0 == image_ || 0 == ucode_size() )
	return 0;

    return ( image_ + convert_endianness( header_.ucode, endianness() ) );
}

const CgBinaryFragmentProgram*
nvb_reader_impl::fragment_program() const
{
    if ( 0 == image_ )
	return 0;

    return reinterpret_cast<CgBinaryFragmentProgram*>( &image_[convert_endianness( header_.program, endianness_ )] );
}

const CgBinaryVertexProgram* nvb_reader_impl::vertex_program() const
{
    if ( 0 == image_ )
	return 0;

    return reinterpret_cast<CgBinaryVertexProgram*>( &image_[convert_endianness( header_.program, endianness_ )] );
}

CGBIO_ERROR nvb_reader_impl::get_param_name( unsigned int index, const char **name, bool& is_referenced ) const
{
	if ( index >= number_of_params() )
		return CGBIO_ERROR_INDEX;

	if ( 0 == image_ )
		return CGBIO_ERROR_NO_ERROR;

	const CgBinaryParameter* params = reinterpret_cast<CgBinaryParameter*>( &image_[convert_endianness( header_.parameterArray, endianness_ )] );
	const CgBinaryParameter& pp = params[index];
    is_referenced = convert_endianness( pp.isReferenced, endianness() ) != 0;
	CgBinaryStringOffset nm_offset = convert_endianness( pp.name,endianness() );
	if ( nm_offset != 0 )
		*name = &image_[nm_offset];
	else
		*name = "";
	return CGBIO_ERROR_NO_ERROR;
}

CGBIO_ERROR
nvb_reader_impl::get_param( unsigned int index,
			    	 CGtype& type,
			     CGresource& resource,
				 CGenum& variability,
				    int& resource_index,
				 const char ** name,
		     std::vector<float>& default_value,
	      std::vector<unsigned int>& embedded_constants,
				 const char ** semantic,
				    int& paramno,
				   bool& is_referenced,
				   bool& is_shared ) const
{
    if ( index >= number_of_params() )
	return CGBIO_ERROR_INDEX;

    if ( 0 == image_ )
	return CGBIO_ERROR_NO_ERROR;

    const CgBinaryParameter* params = reinterpret_cast<CgBinaryParameter*>( &image_[convert_endianness( header_.parameterArray, endianness_ )] );
    const CgBinaryParameter& pp = params[index];
    type		= static_cast<CGtype>(convert_endianness( static_cast<unsigned int>( pp.type ),	endianness() ) );
    resource		= static_cast<CGresource>(convert_endianness( static_cast<unsigned int>( pp.res ), endianness() ) );
    variability		= static_cast<CGenum>(convert_endianness( static_cast<unsigned int>( pp.var ),endianness() ) );
    resource_index	= convert_endianness( pp.resIndex,endianness() );
    paramno		= convert_endianness( pp.paramno, endianness() );
    is_referenced	= convert_endianness( pp.isReferenced,endianness() ) != 0;
    is_shared		= convert_endianness( pp.isShared,endianness() ) != 0;
    CgBinaryStringOffset		nm_offset =	convert_endianness( pp.name,endianness() );
    CgBinaryFloatOffset			dv_offset =	convert_endianness( pp.defaultValue,endianness() );
    CgBinaryEmbeddedConstantOffset	ec_offset =	convert_endianness( pp.embeddedConst,endianness() );
    CgBinaryStringOffset		sm_offset =	convert_endianness( pp.semantic,endianness() );
    if ( 0 != nm_offset )
    {
		*name = &image_[nm_offset];
    }
	else
		*name = "";

    if ( 0 != sm_offset )
    {
		*semantic = &image_[sm_offset];
    }
	else
		*semantic = "";
    if ( 0 != dv_offset )
    {
	char *vp = &image_[dv_offset];
	for (int ii = 0; ii < 4; ++ii)
	{
		int tmp;
		memcpy(&tmp,vp+4*ii,4);
		tmp = convert_endianness(tmp,endianness());
		float tmp2;
		memcpy(&tmp2,&tmp,4);
		default_value.push_back( tmp2 );
	}
    }
    if ( 0 != ec_offset )
    {
	void *vp = &image_[ec_offset];
	CgBinaryEmbeddedConstant& ec = *(static_cast<CgBinaryEmbeddedConstant*>( vp ));
	for (unsigned int ii = 0; ii < convert_endianness( ec.ucodeCount, endianness() ); ++ii)
	{
	    unsigned int offset = convert_endianness( ec.ucodeOffset[ii], endianness() );
	    embedded_constants.push_back( offset );
	}
    }
    return CGBIO_ERROR_NO_ERROR;
}

elf_reader_impl::elf_reader_impl()
{
    ref_count_ = 1;
    initialized_ = false;
    fill_n( reinterpret_cast<char*>( &header_ ), sizeof( header_ ), '\0' );
}

elf_reader_impl::~elf_reader_impl()
{
}

CGBIO_ERROR
elf_reader_impl::load( const char* filename )
{
    return CGBIO_ERROR_NO_ERROR;
}

CGBIO_ERROR
elf_reader_impl::load( std::istream* stream, int start )
{
    return CGBIO_ERROR_NO_ERROR;
}

bool
elf_reader_impl::is_initialized() const
{
    return initialized_;
}

ptrdiff_t
elf_reader_impl::reference() const
{
    return ++ref_count_;
}

ptrdiff_t
elf_reader_impl::release() const
{
	ptrdiff_t ret = --ref_count_;
    if ( 0 == ref_count_ )
	{
        delete this;
    }

    return ret;
}

}
}
