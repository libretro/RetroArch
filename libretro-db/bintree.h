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

typedef int (*bintree_cmp_func)(const void *a, const void *b, void *ctx);
typedef int (*bintree_iter_cb) (void *value, void *ctx);

typedef struct bintree_node
{
   void *value;
   struct bintree_node *parent;
   struct bintree_node *left;
   struct bintree_node *right;
} bintree_node_t;

typedef struct bintree
{
   struct bintree_node *root;
   void *ctx;
   bintree_cmp_func cmp;
} bintree_t;

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx);

int bintree_insert(bintree_t *t, struct bintree_node *root, void *value);

int bintree_iterate(struct bintree_node *n, bintree_iter_cb cb, void *ctx);

void bintree_free(struct bintree_node *n);

RETRO_END_DECLS

#endif
