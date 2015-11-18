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

static struct bintree_node *new_nil_node(struct bintree_node *parent)
{
   struct bintree_node *node = (struct bintree_node *)
      calloc(1, sizeof(struct bintree_node));

   if (!node)
      return NULL;

   node->value  = NIL_NODE;
   node->parent = parent;

   return node;
}

static INLINE int is_nil(const struct bintree_node *node)
{
   return (node == NULL) || (node->value == NIL_NODE);
}

static int insert(bintree_t *t, struct bintree_node *root, void *value)
{
   int cmp_res = 0;

   if (is_nil(root))
   {
      root->left  = new_nil_node(root);
      root->right = new_nil_node(root);

      if (!root->left || !root->right)
      {
         if (root->left)
         {
            free(root->left);
            root->left = NULL;
         }
         if (root->right)
         {
            free(root->right);
            root->right = NULL;
         }
         return -ENOMEM;
      }
      root->value = value;

      return 0;
   }

   cmp_res = t->cmp(root->value, value, t->ctx);

   if (cmp_res > 0)
      return insert(t, root->left, value);
   else if (cmp_res < 0)
      return insert(t, root->right, value);
   return -EINVAL;
}


static int _bintree_iterate(struct bintree_node *n,
      bintree_iter_cb cb, void *ctx)
{
   int rv;

   if (is_nil(n))
      return 0;

   if ((rv = _bintree_iterate(n->left, cb, ctx)) != 0)
      return rv;
   if ((rv = cb(n->value, ctx)) != 0)
      return rv;
   if ((rv = _bintree_iterate(n->right, cb, ctx)) != 0)
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
   return insert(t, t->root, value);
}

int bintree_iterate(const bintree_t *t, bintree_iter_cb cb,
      void *ctx)
{
   return _bintree_iterate(t->root, cb, ctx);
}

bintree_t *bintree_new(bintree_cmp_func cmp, void *ctx)
{
   bintree_t *t = (bintree_t*)calloc(1, sizeof(*t));

   if (!t)
      return NULL;

   t->root = new_nil_node(NULL);
   t->cmp  = cmp;
   t->ctx  = ctx;

   return t;
}

void bintree_free(bintree_t *t)
{
   bintree_free_node(t->root);
}
