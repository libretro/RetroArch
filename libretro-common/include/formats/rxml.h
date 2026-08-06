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

#include <stddef.h>

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

   /* Source position of the element's start tag: the byte offset of
    * its '<' into the document by default, converted in place to a
    * 1-based source line number when RXML_OPT_LINES is passed to
    * rxml_load_document_string_opts. */
   unsigned line;
} rxml_node_t;

/* Reject documents that end inside an open construct instead of
 * returning the partial tree built so far (the historical behavior,
 * kept as the default). */
#define RXML_OPT_STRICT_EOF 1
/* Record the source line of every element in rxml_node::line.  Costs
 * one pass over the document after a successful parse; the parse
 * itself is unaffected. */
#define RXML_OPT_LINES      2

typedef struct rxml_parse_error
{
   size_t   offset; /* byte offset of the failure in the input */
   unsigned line;   /* 1-based */
   unsigned col;    /* 1-based, in bytes */
} rxml_parse_error_t;

rxml_document_t *rxml_load_document(const char *path);
rxml_document_t *rxml_load_document_string(const char *str);

/* As rxml_load_document_string, with parse options; on failure, *err
 * (when non-NULL) receives the position the parser stopped at. */
rxml_document_t *rxml_load_document_string_opts(const char *str,
      unsigned opts, rxml_parse_error_t *err);
void rxml_free_document(rxml_document_t *doc);

struct rxml_node *rxml_root_node(rxml_document_t *doc);

const char *rxml_node_attrib(struct rxml_node *node, const char *attrib);

RETRO_END_DECLS

#endif
