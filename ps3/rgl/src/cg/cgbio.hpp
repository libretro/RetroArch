/* cgbio.hpp -- interface to the whole cg binary input/output library.
 *
 * This is the only header file, that an application need to include
 * in order to manipulate both elf and nvidia original cg binary
 * format.
 */

#if !defined(CGC_CGBIO_CGBIO_HPP)
#define CGC_CGBIO_CGBIO_HPP 1

#include "cgbdefs.hpp"

#include <string>


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
    
} // bio namespace
} // cgc namespace

#include "nvbi.hpp"

namespace cgc {
namespace bio {

    class bin_io
    {
	public:
	    static const bin_io* instance();
		static void delete_instance();

	    CGBIO_ERROR new_nvb_reader( nvb_reader** obj ) const;

	    const char *error_string( CGBIO_ERROR error ) const;

	private:
	    bin_io();
	    bin_io( const bin_io& );

	    static bin_io* instance_;
}; // bin_io

} // bio namespace
} // cgc namespace

#endif // CGC_CGBIO_CGBIO_HPP
