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

#include <retro_inline.h>

#include "bintree.h"

static void * const NIL_NODE = (void*)&NIL_NODE;

static struct bintree_node *bintree_new_nil_node(
      struct bintree_node *parent)
{
   struct bintree_node *node = (struct bintree_node *)
      malloc(sizeof(struct bintree_node));

   if (!node)
      return NULL;

   node->value  = NIL_NODE;
   node->parent = parent;
   node->left   = NULL;
   node->right  = NULL;

   return node;
}

int bintree_insert(bintree_t *t, struct bintree_node *root, void *value)
{
   int cmp_res = 0;

   if (!root || (root->value == NIL_NODE))
   {
      root->left  = bintree_new_nil_node(root);
      root->right = bintree_new_nil_node(root);
      root->value = value;

      return 0;
   }

   cmp_res = t->cmp(root->value, value, t->ctx);

   if (cmp_res > 0)
      return bintree_insert(t, root->left, value);
   else if (cmp_res < 0)
      return bintree_insert(t, root->right, value);
   return -1;
}

int bintree_iterate(struct bintree_node *n, bintree_iter_cb cb, void *ctx)
{
   int rv;

   if (!n || (n->value == NIL_NODE))
      return 0;

   if ((rv = bintree_iterate(n->left, cb, ctx)) != 0)
      return rv;
   if ((rv = cb(n->value, ctx)) != 0)
      return rv;
   if ((rv = bintree_iterate(n->right, cb, ctx)) != 0)
      return rv;

   return 0;
}

void bintree_free(struct bintree_node *n)
{
   if (!n)
      return;
   if (n->value != NIL_NODE)
   {
	   n->value = NULL;
	   if (n->left)
		   bintree_free(n->left);
	   if (n->right)
		   bintree_free(n->right);
   }
   free(n);
}

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx)
{
   bintree_t *t = (bintree_t*)malloc(sizeof(*t));

   if (!t)
      return NULL;

   t->root      = bintree_new_nil_node(NULL);
   t->cmp       = cmp;
   t->ctx       = ctx;

   return t;
}
