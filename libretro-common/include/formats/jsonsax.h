/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (jsonsax.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_JSONSAX_H__
#define __LIBRETRO_SDK_FORMAT_JSONSAX_H__

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
  JSONSAX_OK = 0,
  JSONSAX_INTERRUPTED,
  JSONSAX_MISSING_KEY,
  JSONSAX_UNTERMINATED_KEY,
  JSONSAX_MISSING_VALUE,
  JSONSAX_UNTERMINATED_OBJECT,
  JSONSAX_UNTERMINATED_ARRAY,
  JSONSAX_UNTERMINATED_STRING,
  JSONSAX_INVALID_VALUE
};

#ifdef JSONSAX_ERRORS
extern const char* jsonsax_errors[];
#endif

typedef struct
{
  int ( *start_document )( void* userdata );
  int ( *end_document )( void* userdata );
  int ( *start_object )( void* userdata );
  int ( *end_object )( void* userdata );
  int ( *start_array )( void* userdata );
  int ( *end_array )( void* userdata );
  int ( *key )( void* userdata, const char* name, size_t length );
  int ( *array_index )( void* userdata, unsigned int index );
  int ( *string )( void* userdata, const char* string, size_t length );
  int ( *number )( void* userdata, const char* number, size_t length );
  int ( *boolean )( void* userdata, int istrue );
  int ( *null )( void* userdata );
}
jsonsax_handlers_t;

int jsonsax_parse( const char* json, const jsonsax_handlers_t* handlers, void* userdata );

RETRO_END_DECLS

#endif /* __LIBRETRO_SDK_FORMAT_JSONSAX_H__ */
