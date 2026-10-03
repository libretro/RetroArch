/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ryaml.h).
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

/* YAML reader for block-structured documents.
 *
 * The caller owns the bytes and hands them over; the reader never opens
 * anything, in keeping with the rest of formats/. One call turns a
 * buffer into a tree of nodes addressed by integer handles, and the
 * handles stay valid until the tree is freed.
 *
 * Scalars are not copied. A node's key and value point into the caller's
 * buffer, so that buffer has to outlive the tree. The two constructs
 * that cannot be expressed as a subrange of the input -- block scalars,
 * whose lines carry indentation that is not part of the value, and
 * quoted scalars containing escapes -- are materialised into the tree's
 * own arena instead. Everything else costs an offset and a length.
 *
 * Usage:
 *
 *    ryaml_t *y = ryaml_parse(buf, len);
 *    if (y)
 *    {
 *       int root = ryaml_root(y);
 *       int n;
 *       for (n = ryaml_first_child(y, root); n >= 0;
 *            n = ryaml_next_sibling(y, n))
 *       {
 *          size_t klen;
 *          const char *k = ryaml_key(y, n, &klen);
 *          ...
 *       }
 *       ryaml_free(y);
 *    }
 *
 * The grammar is the block subset plus single-line flow collections:
 * nested block mappings, block sequences, compact mappings written on a
 * sequence dash, plain and quoted scalars, literal and folded block
 * scalars with the three chomping modes, {} and [] flow collections,
 * and comments. Anchors, aliases, tags, directives, multi-document
 * streams and flow collections spanning several lines are not read; a
 * document using them is rejected rather than half-parsed. Tabs are
 * never indentation, which is what the specification says and what a
 * hand-written file usually means anyway.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RYAML_H
#define __LIBRETRO_SDK_FORMAT_RYAML_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Returned by the navigation calls when there is nothing to return.
 * Every call below accepts RYAML_NONE and answers as if asked about an
 * empty node, so a chain of lookups can be written without a test at
 * each step. */
#define RYAML_NONE (-1)

enum ryaml_type
{
   RYAML_TYPE_NONE = 0,
   RYAML_TYPE_SCALAR,   /* has a value                       */
   RYAML_TYPE_MAP,      /* has children, each with a key     */
   RYAML_TYPE_SEQ       /* has children, none with a key     */
};

typedef struct ryaml ryaml_t;

/**
 * ryaml_parse:
 * @buf                    : document bytes, borrowed, not copied
 * @len                    : length of @buf in bytes
 *
 * Reads a whole document. @buf must stay valid and unmodified for as
 * long as the returned tree is used, because scalars point into it.
 *
 * A byte order mark, if present, is skipped. CRLF and LF both count as
 * line endings.
 *
 * Returns: the tree, or NULL if the document could not be read or the
 * allocation failed. Use ryaml_parse_ex() to find out which.
 */
ryaml_t *ryaml_parse(const char *buf, size_t len);

/**
 * ryaml_parse_ex:
 * @buf                    : document bytes, borrowed, not copied
 * @len                    : length of @buf in bytes
 * @err_line               : filled with the 1-based line of the fault
 * @err_col                : filled with the 1-based column of the fault
 *
 * As ryaml_parse(), and on failure reports where the reader stopped.
 * Either pointer may be NULL. On success both are left alone.
 *
 * Returns: the tree, or NULL.
 */
ryaml_t *ryaml_parse_ex(const char *buf, size_t len,
      size_t *err_line, size_t *err_col);

/**
 * ryaml_free:
 * @y                      : tree, may be NULL
 *
 * Releases the tree. The document buffer is untouched.
 */
void ryaml_free(ryaml_t *y);

/**
 * ryaml_root:
 * @y                      : tree
 *
 * Returns: the root node. An empty document still has a root; it simply
 * has no children.
 */
int ryaml_root(const ryaml_t *y);

/**
 * ryaml_count:
 * @y                      : tree
 *
 * Returns: how many nodes the tree holds, root included. Useful for
 * sizing a side table indexed by node handle.
 */
size_t ryaml_count(const ryaml_t *y);

int ryaml_first_child(const ryaml_t *y, int node);
int ryaml_last_child(const ryaml_t *y, int node);
int ryaml_next_sibling(const ryaml_t *y, int node);
int ryaml_parent(const ryaml_t *y, int node);

/**
 * ryaml_num_children:
 * @y                      : tree
 * @node                   : node to count under
 *
 * Returns: the number of direct children, 0 for a scalar.
 */
size_t ryaml_num_children(const ryaml_t *y, int node);

/**
 * ryaml_find_child:
 * @y                      : tree
 * @node                   : mapping to search
 * @key                    : NUL-terminated key, matched exactly
 *
 * Returns: the child, or RYAML_NONE. Key comparison is case-sensitive,
 * as YAML requires; a caller wanting otherwise should fold its own keys.
 */
int ryaml_find_child(const ryaml_t *y, int node, const char *key);

/**
 * ryaml_find_child_len:
 * @y                      : tree
 * @node                   : mapping to search
 * @key                    : key bytes, need not be NUL-terminated
 * @key_len                : length of @key
 *
 * Returns: the child, or RYAML_NONE.
 */
int ryaml_find_child_len(const ryaml_t *y, int node,
      const char *key, size_t key_len);

/**
 * ryaml_has_child:
 * @y                      : tree
 * @node                   : mapping to search
 * @key                    : NUL-terminated key
 *
 * Returns: 1 if present, 0 otherwise.
 */
int ryaml_has_child(const ryaml_t *y, int node, const char *key);

int ryaml_type(const ryaml_t *y, int node);
int ryaml_is_map(const ryaml_t *y, int node);
int ryaml_is_seq(const ryaml_t *y, int node);
int ryaml_has_children(const ryaml_t *y, int node);
int ryaml_has_val(const ryaml_t *y, int node);
int ryaml_has_key(const ryaml_t *y, int node);

/**
 * ryaml_key:
 * @y                      : tree
 * @node                   : node to read
 * @len                    : filled with the key length
 *
 * The bytes are not NUL-terminated: they are a subrange of the document
 * buffer, or of the tree's arena for a key that needed unescaping.
 *
 * Returns: the key bytes, or NULL when the node has no key.
 */
const char *ryaml_key(const ryaml_t *y, int node, size_t *len);

/**
 * ryaml_val:
 * @y                      : tree
 * @node                   : node to read
 * @len                    : filled with the value length
 *
 * As ryaml_key(). A mapping or sequence has no value of its own.
 *
 * Returns: the value bytes, or NULL when the node has no value.
 */
const char *ryaml_val(const ryaml_t *y, int node, size_t *len);

/**
 * ryaml_val_int:
 * @y                      : tree
 * @node                   : node to read
 * @out                    : filled with the parsed value on success
 *
 * Reads a decimal or 0x-prefixed integer, with an optional sign. The
 * whole value has to be consumed, so a trailing unit or stray character
 * is a failure rather than a truncated number.
 *
 * Returns: 1 on success, 0 if the node has no value or it does not read
 * as an integer, in which case @out is untouched.
 */
int ryaml_val_int(const ryaml_t *y, int node, int *out);

/**
 * ryaml_val_uint:
 * @y                      : tree
 * @node                   : node to read
 * @out                    : filled with the parsed value on success
 *
 * As ryaml_val_int() without a sign.
 *
 * Returns: 1 on success, 0 otherwise.
 */
int ryaml_val_uint(const ryaml_t *y, int node, uint32_t *out);

/**
 * ryaml_val_bool:
 * @y                      : tree
 * @node                   : node to read
 * @out                    : filled with 0 or 1 on success
 *
 * Accepts the core schema spellings: true/false, yes/no, on/off, in any
 * case.
 *
 * Returns: 1 on success, 0 otherwise.
 */
int ryaml_val_bool(const ryaml_t *y, int node, int *out);

/**
 * ryaml_val_equals:
 * @y                      : tree
 * @node                   : node to read
 * @str                    : NUL-terminated string to compare against
 *
 * Returns: 1 when the node's value is exactly @str, 0 otherwise.
 */
int ryaml_val_equals(const ryaml_t *y, int node, const char *str);

RETRO_END_DECLS

#endif
