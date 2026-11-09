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

rxml_document_t *rxml_load_document_string(const char *str);

/* As rxml_load_document_string, but takes ownership of @buf - a heap
 * allocation of at least @len + 1 bytes with buf[len] == '\0' -
 * instead of copying it.  The document keeps the buffer alive for its
 * whole lifetime (the tree points into it) and releases it in
 * rxml_free_document; on failure the buffer is freed here before NULL
 * is returned, so the caller must not touch it again either way.
 * For large inputs this halves peak memory against the copying string
 * entry point, which is what callers that already hold the document
 * bytes (e.g. a task that read the file itself) should care about. */
rxml_document_t *rxml_load_document_owned(char *buf, size_t len);

/* As rxml_load_document_owned, with parse options and the same
 * failure-position reporting as rxml_load_document_string_opts.  This
 * is the entry point to reach for when a caller wants
 * RXML_OPT_STRICT_EOF without giving up the buffer handover: the
 * option pair is otherwise only reachable through the incremental
 * rxml_parse_* calls. */
rxml_document_t *rxml_load_document_owned_opts(char *buf, size_t len,
      unsigned opts, rxml_parse_error_t *err);

/* Incremental variant of rxml_load_document_owned(): the same
 * ownership contract (@buf is heap, NUL-terminated at @len, and is
 * either given to the resulting document or freed on failure /
 * abort), with the parse spread over as many rxml_parse_step()
 * calls as the caller's budget requires.
 *
 * rxml_parse_begin_owned() returns NULL only on allocation failure
 * (having freed @buf).  Each rxml_parse_step() consumes roughly
 * @max_bytes of input - the bound is @max_bytes plus one construct,
 * since parsing pauses only between constructs - and returns 0 to
 * continue, 1 when the document reached its verdict successfully,
 * or -1 when it failed (@max_bytes of 0 runs to the verdict in one
 * call).  rxml_parse_end() releases the parse state and returns the
 * document after a successful verdict, NULL otherwise (discarding
 * any partial parse).  rxml_parse_abort() releases everything
 * unconditionally. */
typedef struct rxml_parse rxml_parse_t;

rxml_parse_t *rxml_parse_begin_owned(char *buf, size_t len,
      unsigned opts);
int rxml_parse_step(rxml_parse_t *parse, size_t max_bytes);
rxml_document_t *rxml_parse_end(rxml_parse_t *parse);
void rxml_parse_abort(rxml_parse_t *parse);

/* As rxml_load_document_string, with parse options; on failure, *err
 * (when non-NULL) receives the position the parser stopped at. */
rxml_document_t *rxml_load_document_string_opts(const char *str,
      unsigned opts, rxml_parse_error_t *err);
void rxml_free_document(rxml_document_t *doc);

struct rxml_node *rxml_root_node(rxml_document_t *doc);

const char *rxml_node_attrib(struct rxml_node *node, const char *attrib);

/* Writer.  Builds a document in memory: elements are opened, given
 * attributes and text, and closed in document order, and the result is
 * taken with rxml_writer_data.  Text and attribute values are escaped
 * so that rxml parses them back unchanged.  A call that cannot give
 * well-formed output (a second root element, an attribute after the
 * element's content, text outside an element, a close with nothing
 * open, a missing or tag-breaking name) or an allocation failure stops
 * the writer: later calls do nothing and rxml_writer_data returns NULL.
 *
 * @indent: written once per level before each element on a line of
 * its own (e.g. "\t"); NULL writes the document on one line. */
typedef struct rxml_writer rxml_writer_t;

rxml_writer_t *rxml_writer_new(const char *indent);
void rxml_writer_free(rxml_writer_t *w);

/* <?xml version="1.0" encoding="UTF-8"?> - only as the first output */
void rxml_write_declaration(rxml_writer_t *w);
void rxml_write_open(rxml_writer_t *w, const char *name);
/* Only before the open element's text and children */
void rxml_write_attrib(rxml_writer_t *w, const char *name, const char *value);
void rxml_write_text(rxml_writer_t *w, const char *text);
void rxml_write_close(rxml_writer_t *w);
/* <name>text</name>, or <name /> for NULL or empty text */
void rxml_write_element(rxml_writer_t *w, const char *name, const char *text);
/* @node and everything under it, e.g. a parsed document written back
 * with changes made around it */
void rxml_write_node(rxml_writer_t *w, const struct rxml_node *node);

/* The document, NUL-terminated, owned by the writer; NULL when the
 * writer stopped or the root element is not yet closed */
const char *rxml_writer_data(rxml_writer_t *w, size_t *len);
int rxml_writer_failed(const rxml_writer_t *w);

RETRO_END_DECLS

#endif
