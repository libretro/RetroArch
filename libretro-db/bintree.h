/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bintree.h).
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

#ifndef __LIBRETRODB_BINTREE_H__
#define __LIBRETRODB_BINTREE_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct bintree bintree_t;

typedef int (*bintree_cmp_func)(const void *a, const void *b, void *ctx);
typedef int (*bintree_iter_cb) (void *value, void *ctx);

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx);

int bintree_insert(bintree_t *t, void *value);

int bintree_iterate(const bintree_t *t, bintree_iter_cb cb, void *ctx);

void bintree_free(bintree_t *t);

RETRO_END_DECLS

#endif
