/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml.h).
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
#ifndef __LIBRETRO_SDK_FORMAT_RXML_H__
#define __LIBRETRO_SDK_FORMAT_RXML_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Total NIH. Very trivial "XML" implementation for use in RetroArch.
 * Error checking is minimal. Invalid documents may lead to very
 * buggy behavior, but memory corruption should never happen.
 *
 * Only parts of standard that RetroArch cares about is supported.
 * Nothing more, nothing less. "Clever" XML documents will
 * probably break the implementation.
 *
 * Do *NOT* try to use this for anything else. You have been warned.
 */

typedef struct rxml_document rxml_document_t;

struct rxml_attrib_node
{
   char *attrib;
   char *value;
   struct rxml_attrib_node *next;
};

typedef struct rxml_node
{
   char *name;
   char *data;
   struct rxml_attrib_node *attrib;

   struct rxml_node *children;
   struct rxml_node *next;
} rxml_node_t;

rxml_document_t *rxml_load_document(const char *path);
rxml_document_t *rxml_load_document_string(const char *str);
void rxml_free_document(rxml_document_t *doc);

struct rxml_node *rxml_root_node(rxml_document_t *doc);

const char *rxml_node_attrib(struct rxml_node *node, const char *attrib);

RETRO_END_DECLS

#endif
