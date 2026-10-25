/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt_css_tree.c).
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


#include <stdlib.h>
#include <string.h>
#include "rsubtitle_vtt_internal.h"

#define VTT_CSS_NODE_LIMIT 262144u
#define VTT_CSS_NODE_CHUNK 256u
#define VTT_CSS_POOL_CHUNK 4096u

struct vtt_css_tree_node
{
   rsubtitle_vtt_css_node_t view;
   size_t last_child;
   uint32_t closing;
};

struct vtt_css_pool
{
   struct vtt_css_pool *next;
   size_t used, capacity;
};

struct rsubtitle_vtt_css_tree
{
   struct vtt_css_tree_node **nodes;
   struct vtt_css_pool *pool;
   rsubtitle_vtt_css_t *lexer;
   rsubtitle_vtt_css_token_t pending;
   char *copy;
   size_t copied, count, current, pool_bytes;
   unsigned chunks, depth;
   int begun, active, failed;
};

static struct vtt_css_tree_node *vtt_css_tree_at(
      const rsubtitle_vtt_css_tree_t *tree, size_t index)
{
   return &tree->nodes[index / VTT_CSS_NODE_CHUNK][index % VTT_CSS_NODE_CHUNK];
}

static char *vtt_css_tree_string(rsubtitle_vtt_css_tree_t *tree, size_t size)
{
   struct vtt_css_pool *pool = tree->pool;
   char *text;
   if (size > RSUBTITLE_SRT_MAX_INPUT - tree->pool_bytes) return NULL;
   if (!pool || size > pool->capacity - pool->used)
   {
      size_t capacity = size < VTT_CSS_POOL_CHUNK ? VTT_CSS_POOL_CHUNK : size;
      pool = (struct vtt_css_pool*)malloc(sizeof(*pool) + capacity);
      if (!pool) return NULL;
      pool->capacity = capacity; pool->used = 0;
      pool->next = tree->pool; tree->pool = pool;
   }
   text = (char*)(pool + 1) + pool->used;
   pool->used += size;
   tree->pool_bytes += size;
   return text;
}

static size_t vtt_css_tree_add(rsubtitle_vtt_css_tree_t *tree,
      const rsubtitle_vtt_css_token_t *token, uint32_t closing)
{
   size_t index = tree->count;
   struct vtt_css_tree_node *node;
   char *text = NULL;
   if (index == VTT_CSS_NODE_LIMIT) return RSUBTITLE_VTT_NO_NODE;
   if (!tree->nodes)
   {
      tree->nodes = (struct vtt_css_tree_node**)calloc(
            VTT_CSS_NODE_LIMIT / VTT_CSS_NODE_CHUNK, sizeof(*tree->nodes));
      if (!tree->nodes) return RSUBTITLE_VTT_NO_NODE;
   }
   if (index == (size_t)tree->chunks * VTT_CSS_NODE_CHUNK)
   {
      struct vtt_css_tree_node *chunk = (struct vtt_css_tree_node*)malloc(
            VTT_CSS_NODE_CHUNK * sizeof(*node));
      if (!chunk) return RSUBTITLE_VTT_NO_NODE;
      tree->nodes[tree->chunks++] = chunk;
   }
   if (token->length)
   {
      text = vtt_css_tree_string(tree, token->length + 1);
      if (!text) return RSUBTITLE_VTT_NO_NODE;
      tree->pending = *token; tree->copy = text; tree->copied = 0;
   }
   node = vtt_css_tree_at(tree, index);
   memset(node, 0, sizeof(*node));
   node->view.token = *token;
   node->view.token.text = text ? text : "";
   node->view.parent = tree->current;
   node->view.first_child = node->view.next = node->last_child = RSUBTITLE_VTT_NO_NODE;
   node->closing = closing;
   if (tree->current != RSUBTITLE_VTT_NO_NODE)
   {
      struct vtt_css_tree_node *parent = vtt_css_tree_at(tree, tree->current);
      if (parent->last_child != RSUBTITLE_VTT_NO_NODE)
         vtt_css_tree_at(tree, parent->last_child)->view.next = index;
      else parent->view.first_child = index;
      parent->last_child = index;
   }
   tree->count++;
   return index;
}

rsubtitle_vtt_css_tree_t *rsubtitle_vtt_css_tree_new(void)
{
   rsubtitle_vtt_css_tree_t *tree = (rsubtitle_vtt_css_tree_t*)calloc(1, sizeof(*tree));
   if (!tree) return NULL;
   tree->lexer = rsubtitle_vtt_css_new();
   if (!tree->lexer) { free(tree); return NULL; }
   tree->current = RSUBTITLE_VTT_NO_NODE;
   return tree;
}

void rsubtitle_vtt_css_tree_free(rsubtitle_vtt_css_tree_t *tree)
{
   unsigned i;
   if (!tree) return;
   while (tree->pool)
   {
      struct vtt_css_pool *next = tree->pool->next;
      free(tree->pool); tree->pool = next;
   }
   for (i = 0; i < tree->chunks; i++) free(tree->nodes[i]);
   free(tree->nodes);
   rsubtitle_vtt_css_free(tree->lexer);
   free(tree);
}

int rsubtitle_vtt_css_tree_begin(rsubtitle_vtt_css_tree_t *tree,
      const char *input, size_t size, size_t *root)
{
   rsubtitle_vtt_css_token_t token;
   if (root) *root = RSUBTITLE_VTT_NO_NODE;
   if (!tree || !root || tree->active || tree->failed
         || !rsubtitle_vtt_css_begin(tree->lexer, input, size)) return 0;
   memset(&token, 0, sizeof(token));
   token.kind = RSUBTITLE_CSS_ROOT;
   tree->current = RSUBTITLE_VTT_NO_NODE;
   *root = vtt_css_tree_add(tree, &token, 0);
   if (*root == RSUBTITLE_VTT_NO_NODE) { tree->failed = 1; return 0; }
   tree->current = *root; tree->depth = 0;
   tree->begun = tree->active = 1;
   return 1;
}

int rsubtitle_vtt_css_tree_step(rsubtitle_vtt_css_tree_t *tree)
{
   rsubtitle_vtt_css_token_t token;
   uint32_t closing = 0;
   size_t index;
   int status;
   if (!tree || !tree->begun || tree->failed) return -1;
   if (!tree->active) return 1;
   if (tree->copy)
   {
      if (tree->copied < tree->pending.length)
      {
         tree->copy[tree->copied] = tree->pending.text[tree->copied];
         tree->copied++;
      }
      else { tree->copy[tree->copied] = 0; tree->copy = NULL; }
      return 0;
   }
   status = rsubtitle_vtt_css_step(tree->lexer, &token);
   if (status < 0) { tree->failed = 1; return -1; }
   if (!status) return 0;
   if (status == 2)
   {
      tree->active = 0;
      return 1;
   }
   if (token.kind == RSUBTITLE_CSS_FUNCTION) closing = ')';
   else if (token.kind == RSUBTITLE_CSS_DELIM)
   {
      uint32_t c = token.delimiter;
      if (c == '(' || c == '[' || c == '{')
      {
         closing = c == '(' ? ')' : c == '[' ? ']' : '}';
         token.kind = RSUBTITLE_CSS_BLOCK;
      }
      else if (vtt_css_tree_at(tree, tree->current)->closing == c)
      {
         tree->current = vtt_css_tree_at(tree, tree->current)->view.parent;
         tree->depth--;
         return 0;
      }
   }
   if (closing && tree->depth == 128) { tree->failed = 1; return -1; }
   index = vtt_css_tree_add(tree, &token, closing);
   if (index == RSUBTITLE_VTT_NO_NODE) { tree->failed = 1; return -1; }
   if (closing) { tree->current = index; tree->depth++; }
   return 0;
}

size_t rsubtitle_vtt_css_tree_count(const rsubtitle_vtt_css_tree_t *tree)
{
   return tree && !tree->active && !tree->failed ? tree->count : 0;
}

const rsubtitle_vtt_css_node_t *rsubtitle_vtt_css_tree_node(
      const rsubtitle_vtt_css_tree_t *tree, size_t index)
{
   return index < rsubtitle_vtt_css_tree_count(tree) ? &vtt_css_tree_at(tree, index)->view : NULL;
}

int rsubtitle_vtt_css_rules_begin(rsubtitle_vtt_css_rules_t *rules,
      const rsubtitle_vtt_css_tree_t *tree, size_t root)
{
   const rsubtitle_vtt_css_node_t *node = rsubtitle_vtt_css_tree_node(tree, root);
   if (!rules || !node || (node->token.kind != RSUBTITLE_CSS_ROOT
         && (node->token.kind != RSUBTITLE_CSS_BLOCK || node->token.delimiter != '{')))
      return 0;
   rules->tree = tree;
   rules->cursor = node->first_child;
   rules->first = rules->last = RSUBTITLE_VTT_NO_NODE;
   rules->at_rule = rules->prefix = 0;
   rules->top_level = node->token.kind == RSUBTITLE_CSS_ROOT;
   return 1;
}

int rsubtitle_vtt_css_rules_step(rsubtitle_vtt_css_rules_t *rules,
      rsubtitle_vtt_css_rule_t *rule)
{
   const rsubtitle_vtt_css_node_t *node;
   size_t index;
   int body, semicolon;
   if (!rules || !rule || !rules->tree) return -1;
   index = rules->cursor;
   if (index == RSUBTITLE_VTT_NO_NODE)
   {
      if (rules->first == RSUBTITLE_VTT_NO_NODE || !rules->at_rule) return 2;
      rule->first = rules->first; rule->last = rules->last;
      rule->body = RSUBTITLE_VTT_NO_NODE; rule->at_rule = 1;
      rules->first = rules->last = RSUBTITLE_VTT_NO_NODE;
      return 1;
   }
   node = rsubtitle_vtt_css_tree_node(rules->tree, index);
   if (!node) return -1;
   rules->cursor = node->next;
   body = node->token.kind == RSUBTITLE_CSS_BLOCK && node->token.delimiter == '{';
   semicolon = node->token.kind == RSUBTITLE_CSS_DELIM && node->token.delimiter == ';';
   if (rules->first == RSUBTITLE_VTT_NO_NODE)
   {
      if (node->token.kind == RSUBTITLE_CSS_SPACE
            || (rules->top_level && (node->token.kind == RSUBTITLE_CSS_CDO
                  || node->token.kind == RSUBTITLE_CSS_CDC)))
         return 0;
      if (body)
      {
         rule->first = rule->last = RSUBTITLE_VTT_NO_NODE;
         rule->body = index; rule->at_rule = 0;
         return 1;
      }
      rules->first = index;
      rules->at_rule = node->token.kind == RSUBTITLE_CSS_AT;
      rules->prefix = node->token.kind == RSUBTITLE_CSS_IDENT && node->token.length >= 2
         && node->token.text[0] == '-' && node->token.text[1] == '-';
      rules->last = index;
      return 0;
   }
   if (body && !rules->at_rule && rules->prefix == 2)
   {
      rules->first = rules->last = RSUBTITLE_VTT_NO_NODE;
      return 0;
   }
   if (node->token.kind != RSUBTITLE_CSS_SPACE && rules->prefix == 1)
      rules->prefix = node->token.kind == RSUBTITLE_CSS_DELIM && node->token.delimiter == ':' ? 2 : 0;
   if (body || (semicolon && rules->at_rule))
   {
      rule->first = rules->first; rule->last = rules->last;
      rule->body = body ? index : RSUBTITLE_VTT_NO_NODE;
      rule->at_rule = rules->at_rule;
      rules->first = rules->last = RSUBTITLE_VTT_NO_NODE;
      return 1;
   }
   rules->last = index;
   return 0;
}

enum vtt_css_declaration_phase
{
   VTT_CSS_DECL_START = 0,
   VTT_CSS_DECL_COLON,
   VTT_CSS_DECL_VALUE,
   VTT_CSS_DECL_BAD,
   VTT_CSS_DECL_AT
};

int rsubtitle_vtt_css_declarations_begin(rsubtitle_vtt_css_declarations_t *decls,
      const rsubtitle_vtt_css_tree_t *tree, size_t body)
{
   const rsubtitle_vtt_css_node_t *node = rsubtitle_vtt_css_tree_node(tree, body);
   if (!decls || !node || (node->token.kind != RSUBTITLE_CSS_ROOT
         && (node->token.kind != RSUBTITLE_CSS_BLOCK || node->token.delimiter != '{')))
      return 0;
   memset(decls, 0, sizeof(*decls));
   decls->tree = tree;
   decls->cursor = node->first_child;
   return 1;
}

static int vtt_css_declaration_finish(rsubtitle_vtt_css_declarations_t *decls,
      rsubtitle_vtt_css_declaration_t *declaration)
{
   const rsubtitle_vtt_css_node_t *last = rsubtitle_vtt_css_tree_node(decls->tree, decls->last[0]);
   const rsubtitle_vtt_css_node_t *bang = rsubtitle_vtt_css_tree_node(decls->tree, decls->last[1]);
   const rsubtitle_vtt_css_node_t *property = rsubtitle_vtt_css_tree_node(decls->tree, decls->property);
   size_t i, values = decls->values;
   unsigned important = 0;
   decls->phase = VTT_CSS_DECL_START;
   if (last && bang && last->token.kind == RSUBTITLE_CSS_IDENT
         && last->token.length == 9 && bang->token.kind == RSUBTITLE_CSS_DELIM
         && bang->token.delimiter == '!')
   {
      important = 1;
      for (i = 0; i < 9; i++)
      {
         unsigned c = (unsigned char)last->token.text[i];
         if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
         if (c != (unsigned char)"important"[i]) important = 0;
      }
   }
   if (important) values -= 2;
   if (decls->curly && values != 1 && !(property->token.length >= 2
         && property->token.text[0] == '-' && property->token.text[1] == '-'))
      return 0;
   declaration->property = decls->property;
   declaration->first = values ? decls->first : RSUBTITLE_VTT_NO_NODE;
   declaration->last = values ? decls->last[important ? 2 : 0] : RSUBTITLE_VTT_NO_NODE;
   declaration->important = important;
   return 1;
}

int rsubtitle_vtt_css_declarations_step(rsubtitle_vtt_css_declarations_t *decls,
      rsubtitle_vtt_css_declaration_t *declaration)
{
   const rsubtitle_vtt_css_node_t *node;
   size_t index;
   int semicolon;
   if (!decls || !declaration || !decls->tree) return -1;
   index = decls->cursor;
   if (index == RSUBTITLE_VTT_NO_NODE)
      return decls->phase == VTT_CSS_DECL_VALUE
         ? vtt_css_declaration_finish(decls, declaration) : 2;
   node = rsubtitle_vtt_css_tree_node(decls->tree, index);
   if (!node) return -1;
   decls->cursor = node->next;
   semicolon = node->token.kind == RSUBTITLE_CSS_DELIM && node->token.delimiter == ';';
   if (semicolon)
   {
      if (decls->phase == VTT_CSS_DECL_VALUE)
         return vtt_css_declaration_finish(decls, declaration);
      decls->phase = VTT_CSS_DECL_START;
      return 0;
   }
   if (decls->phase == VTT_CSS_DECL_AT)
   {
      if (node->token.kind == RSUBTITLE_CSS_BLOCK && node->token.delimiter == '{')
         decls->phase = VTT_CSS_DECL_START;
      return 0;
   }
   if (decls->phase == VTT_CSS_DECL_BAD) return 0;
   if (decls->phase == VTT_CSS_DECL_START)
   {
      if (node->token.kind == RSUBTITLE_CSS_SPACE) return 0;
      if (node->token.kind != RSUBTITLE_CSS_IDENT)
      {
         decls->phase = node->token.kind == RSUBTITLE_CSS_AT ? VTT_CSS_DECL_AT : VTT_CSS_DECL_BAD;
         return 0;
      }
      decls->property = index;
      decls->first = decls->last[0] = decls->last[1] = decls->last[2] = RSUBTITLE_VTT_NO_NODE;
      decls->values = decls->curly = 0;
      decls->phase = VTT_CSS_DECL_COLON;
      return 0;
   }
   if (decls->phase == VTT_CSS_DECL_COLON)
   {
      if (node->token.kind == RSUBTITLE_CSS_SPACE) return 0;
      decls->phase = node->token.kind == RSUBTITLE_CSS_DELIM && node->token.delimiter == ':'
         ? VTT_CSS_DECL_VALUE : VTT_CSS_DECL_BAD;
      return 0;
   }
   if (node->token.kind == RSUBTITLE_CSS_SPACE) return 0;
   if (decls->first == RSUBTITLE_VTT_NO_NODE) decls->first = index;
   decls->last[2] = decls->last[1]; decls->last[1] = decls->last[0]; decls->last[0] = index;
   decls->values++;
   if (node->token.kind == RSUBTITLE_CSS_BLOCK && node->token.delimiter == '{') decls->curly++;
   return 0;
}
