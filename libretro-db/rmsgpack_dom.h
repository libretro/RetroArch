/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmsgpack_dom.h).
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

#ifndef __LIBRETRODB_MSGPACK_DOM_H__
#define __LIBRETRODB_MSGPACK_DOM_H__

#include <stdint.h>

#include <retro_common_api.h>
#include <streams/file_stream.h>

RETRO_BEGIN_DECLS

enum rmsgpack_dom_type
{
	RDT_NULL = 0,
	RDT_BOOL,
	RDT_UINT,
	RDT_INT,
	RDT_STRING,
	RDT_BINARY,
	RDT_MAP,
	RDT_ARRAY
};

struct rmsgpack_dom_value
{
   enum rmsgpack_dom_type type;
   union
   {
      uint64_t uint_;
      int64_t int_;
      struct
      {
         uint32_t len;
         char *buff;
      } string;
      struct
      {
         uint32_t len;
         char *buff;
      } binary;
      int bool_;
      struct
      {
         uint32_t len;
         struct rmsgpack_dom_pair *items;
      } map;
      struct
      {
         uint32_t len;
         struct rmsgpack_dom_value *items;
      } array;
   } val;
};

struct rmsgpack_dom_pair
{
	struct rmsgpack_dom_value key;
	struct rmsgpack_dom_value value;
};

void rmsgpack_dom_value_print(struct rmsgpack_dom_value *obj);
void rmsgpack_dom_value_free(struct rmsgpack_dom_value *v);

int rmsgpack_dom_value_cmp(
      const struct rmsgpack_dom_value *a, const struct rmsgpack_dom_value *b);

struct rmsgpack_dom_value *rmsgpack_dom_value_map_value(
        const struct rmsgpack_dom_value *map,
        const struct rmsgpack_dom_value *key);

int rmsgpack_dom_read(RFILE *fd, struct rmsgpack_dom_value *out);

int rmsgpack_dom_write(RFILE *fd, const struct rmsgpack_dom_value *obj);

int rmsgpack_dom_read_into(RFILE *fd, ...);

RETRO_END_DECLS

#endif
