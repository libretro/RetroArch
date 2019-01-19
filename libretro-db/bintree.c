/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bintree.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <retro_inline.h>

#include "bintree.h"

struct bintree_node
{
   void *value;
   struct bintree_node *parent;
   struct bintree_node *left;
   struct bintree_node *right;
};

struct bintree
{
   struct bintree_node *root;
   bintree_cmp_func cmp;
   void *ctx;
};

static void *NIL_NODE = &NIL_NODE;

static struct bintree_node *bintree_new_nil_node(struct bintree_node *parent)
{
   struct bintree_node *node = (struct bintree_node *)
      calloc(1, sizeof(struct bintree_node));

   if (!node)
      return NULL;

   node->value  = NIL_NODE;
   node->parent = parent;

   return node;
}

static INLINE int bintree_is_nil(const struct bintree_node *node)
{
   return (node == NULL) || (node->value == NIL_NODE);
}

static int bintree_insert_internal(bintree_t *t, struct bintree_node *root, void *value)
{
   int cmp_res = 0;

   if (bintree_is_nil(root))
   {
      root->left  = bintree_new_nil_node(root);
      root->right = bintree_new_nil_node(root);
      root->value = value;

      return 0;
   }

   cmp_res = t->cmp(root->value, value, t->ctx);

   if (cmp_res > 0)
      return bintree_insert_internal(t, root->left, value);
   else if (cmp_res < 0)
      return bintree_insert_internal(t, root->right, value);
   return -EINVAL;
}

static int bintree_iterate_internal(struct bintree_node *n,
      bintree_iter_cb cb, void *ctx)
{
   int rv;

   if (bintree_is_nil(n))
      return 0;

   if ((rv = bintree_iterate_internal(n->left, cb, ctx)) != 0)
      return rv;
   if ((rv = cb(n->value, ctx)) != 0)
      return rv;
   if ((rv = bintree_iterate_internal(n->right, cb, ctx)) != 0)
      return rv;

   return 0;
}

static void bintree_free_node(struct bintree_node *n)
{
   if (!n)
      return;

   if (n->value == NIL_NODE)
   {
      free(n);
      return;
   }

   n->value = NULL;
   bintree_free_node(n->left);
   bintree_free_node(n->right);
   free(n);
}

int bintree_insert(bintree_t *t, void *value)
{
   return bintree_insert_internal(t, t->root, value);
}

int bintree_iterate(const bintree_t *t, bintree_iter_cb cb,
      void *ctx)
{
   return bintree_iterate_internal(t->root, cb, ctx);
}

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx)
{
   bintree_t *t = (bintree_t*)calloc(1, sizeof(*t));

   if (!t)
      return NULL;

   t->root = bintree_new_nil_node(NULL);
   t->cmp  = cmp;
   t->ctx  = ctx;

   return t;
}

void bintree_free(bintree_t *t)
{
   bintree_free_node(t->root);
}
