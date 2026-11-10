/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ryaml_writer.h).
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

/* YAML writer: the counterpart of ryaml's reader. The caller walks its
 * data and says what comes next - a mapping, a key, a scalar, a sequence
 * - and the writer lays it out as block YAML, two spaces a level:
 *
 *    Core:
 *      PPU Threads: 2
 *      Libraries: []
 *    List:
 *      - one
 *      - key: value
 *        other: value
 *    Patch:
 *      - [be32, 0x10000, 0x60000000]
 *
 * A sequence may instead be written in flow style on one line, as the
 * last entry shows. An empty mapping or sequence comes out as {} or [].
 * A key or scalar is quoted, with escapes, only where the reader would
 * not read it back as it is: empty, with blanks at either end, starting
 * with an indicator, holding ": " or " #" or a line break, or (in a flow
 * sequence) a comma or bracket. Whatever is written reads back with
 * ryaml_parse() to the same keys, values and structure.
 *
 * Output goes through a callback, a buffer at a time, or into a buffer
 * the writer grows and hands over.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RYAML_WRITER_H
#define __LIBRETRO_SDK_FORMAT_RYAML_WRITER_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct ryaml_writer ryaml_writer_t;

/* Takes @len bytes; returns 0 on failure, which the writer keeps and
 * reports from then on. */
typedef int (*ryaml_writer_io_t)(const void *buf, size_t len, void *user_data);

/**
 * ryaml_writer_new:
 * @io                     : where output goes, NULL to keep it in memory
 * @user_data              : passed to @io
 * Returns: the writer, or NULL when out of memory.
 */
ryaml_writer_t *ryaml_writer_new(ryaml_writer_io_t io, void *user_data);

/**
 * ryaml_writer_free:
 * @w                      : writer, may be NULL
 * Writes out what is left and frees the writer.
 * Returns: 1 when everything was written and well formed, 0 otherwise.
 */
int ryaml_writer_free(ryaml_writer_t *w);

/**
 * ryaml_writer_memory:
 * @w                      : writer made with a NULL @io
 * @len                    : filled with the length
 * The document so far, NUL-terminated, owned by the writer: valid until
 * the next call on it. NULL for a writer with a callback, or after a
 * failure.
 */
const char *ryaml_writer_memory(ryaml_writer_t *w, size_t *len);

/**
 * ryaml_writer_error:
 * Returns: 1 once a write failed or the calls did not make a document
 * (a scalar where a key was due, an end without a begin, ...).
 */
int ryaml_writer_error(const ryaml_writer_t *w);

/* A mapping: keys and their values until ryaml_writer_end_map(). As the
 * document itself, a value of a key, or an item of a block sequence. */
void ryaml_writer_begin_map(ryaml_writer_t *w);
void ryaml_writer_end_map(ryaml_writer_t *w);

/* A sequence: items until ryaml_writer_end_seq(). @flow writes it on one
 * line, [a, b]; its items are then scalars or flow sequences. */
void ryaml_writer_begin_seq(ryaml_writer_t *w, int flow);
void ryaml_writer_end_seq(ryaml_writer_t *w);

/* A key in the current mapping; its value comes next. */
void ryaml_writer_key(ryaml_writer_t *w, const char *key, size_t len);

/* A scalar: the value of the last key, an item, or the document. */
void ryaml_writer_scalar(ryaml_writer_t *w, const char *val, size_t len);

/* An empty line, between two entries of a block mapping or sequence. */
void ryaml_writer_blank_line(ryaml_writer_t *w);

RETRO_END_DECLS

#endif
