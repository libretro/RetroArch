#include "stb_rect_pack.h"

#include <stdlib.h>

enum
{
   STBRP__INIT_skyline = 1
};

void stbrp_init_target(stbrp_context *context, int width, int height, stbrp_node *nodes, int num_nodes)
{
   int i;
   for (i=0; i < num_nodes-1; ++i)
      nodes[i].next = &nodes[i+1];
   nodes[i].next = NULL;
   context->init_mode = STBRP__INIT_skyline;
   context->heuristic = STBRP_HEURISTIC_Skyline_default;
   context->free_head = &nodes[0];
   context->active_head = &context->extra[0];
   context->width = width;
   context->height = height;
   context->num_nodes = num_nodes;
   context->align = (context->width + context->num_nodes-1) / context->num_nodes;

   /* node 0 is the full width,
    * node 1 is the sentinel (lets us not store width explicitly) */
   context->extra[0].x = 0;
   context->extra[0].y = 0;
   context->extra[0].next = &context->extra[1];
   context->extra[1].x = (uint16_t) width;
   context->extra[1].y = 65535;
   context->extra[1].next = NULL;
}

/* Find minimum y position if it starts at x1 */
static int stbrp__skyline_find_min_y(stbrp_context *c,
      stbrp_node *first, int x0, int width, int *pwaste)
{
   int min_y, visited_width, waste_area;
   stbrp_node *node = first;
   int x1 = x0 + width;
   min_y = 0;
   waste_area = 0;
   visited_width = 0;
   while (node->x < x1)
   {
      if (node->y > min_y)
      {
         /* raise min_y higher.
          * we've accounted for all waste up to min_y,
          * but we'll now add more waste for everything we've visted
          */
         waste_area += visited_width * (node->y - min_y);
         min_y = node->y;

         /* the first time through, visited_width might be reduced */
         if (node->x < x0)
            visited_width += node->next->x - x0;
         else
            visited_width += node->next->x - node->x;
      }
      else
      {
         /* add waste area */
         int under_width = node->next->x - node->x;
         if (under_width + visited_width > width)
            under_width = width - visited_width;
         waste_area += under_width * (min_y - node->y);
         visited_width += under_width;
      }
      node = node->next;
   }

   *pwaste = waste_area;
   return min_y;
}

typedef struct
{
   int x,y;
   stbrp_node **prev_link;
} stbrp__findresult;

static stbrp__findresult stbrp__skyline_find_best_pos(stbrp_context *c, int width, int height)
{
   int best_waste = (1<<30), best_x, best_y = (1 << 30);
   stbrp__findresult fr;
   stbrp_node **prev, *node, *tail, **best = NULL;

   /* align to multiple of c->align */
   width = (width + c->align - 1);
   width -= width % c->align;

   node = c->active_head;
   prev = &c->active_head;
   while (node->x + width <= c->width)
   {
      int waste;
      int y = stbrp__skyline_find_min_y(c, node, node->x, width, &waste);

      if (c->heuristic == STBRP_HEURISTIC_Skyline_BL_sortHeight)
      {
         /* actually just want to test BL bottom left */
         if (y < best_y)
         {
            best_y = y;
            best = prev;
         }
      }
      else
      {
         /* best-fit */
         if (y + height <= c->height)
         {
            /* can only use it if it first vertically */
            if (y < best_y || (y == best_y && waste < best_waste))
            {
               best_y = y;
               best_waste = waste;
               best = prev;
            }
         }
      }
      prev = &node->next;
      node = node->next;
   }

   best_x = (best == NULL) ? 0 : (*best)->x;

   /* if doing best-fit (BF), we also have to try aligning right edge to each node position
    *
    * e.g, if fitting
    *
    *     ____________________
    *    |____________________|
    *
    *            into
    *
    *   |                         |
    *   |             ____________|
    *   |____________|
    *
    * then right-aligned reduces waste, but bottom-left BL is always chooses left-aligned
    *
    * This makes BF take about 2x the time
    */

   if (c->heuristic == STBRP_HEURISTIC_Skyline_BF_sortHeight)
   {
      tail = c->active_head;
      node = c->active_head;
      prev = &c->active_head;
      /* find first node that's admissible */
      while (tail->x < width)
         tail = tail->next;
      while (tail)
      {
         int xpos = tail->x - width;
         int y,waste;

         /* find the left position that matches this */
         while (node->next->x <= xpos)
         {
            prev = &node->next;
            node = node->next;
         }

         y = stbrp__skyline_find_min_y(c, node, xpos, width, &waste);

         if (y + height < c->height)
         {
            if (y <= best_y)
            {
               if (y < best_y || waste < best_waste || (waste==best_waste && xpos < best_x))
               {
                  best_x = xpos;
                  best_y = y;
                  best_waste = waste;
                  best = prev;
               }
            }
         }
         tail = tail->next;
      }
   }

   fr.prev_link = best;
   fr.x = best_x;
   fr.y = best_y;
   return fr;
}

static stbrp__findresult stbrp__skyline_pack_rectangle(stbrp_context *context, int width, int height)
{
   /* find best position according to heuristic */
   stbrp__findresult res = stbrp__skyline_find_best_pos(context, width, height);

   /* bail if:
    *    1. it failed
    *    2. the best node doesn't fit (we don't always check this)
    *    3. we're out of memory
    */
   if (res.prev_link == NULL || res.y + height > context->height || context->free_head == NULL)
      res.prev_link = NULL;
   else
   {
      stbrp_node *cur;
      /* on success, create new node */
      stbrp_node *node = context->free_head;
      node->x = (uint16_t) res.x;
      node->y = (uint16_t) (res.y + height);

      context->free_head = node->next;

      /* insert the new node into the right starting point, and
       * let 'cur' point to the remaining nodes needing to be
       * stiched back in
       */

      cur = *res.prev_link;
      if (cur->x < res.x)
      {
         /* preserve the existing one, so start testing with the next one */
         stbrp_node *next = cur->next;
         cur->next = node;
         cur = next;
      }
      else
         *res.prev_link = node;

      /* from here, traverse cur and free the nodes, until we get to one
       * that shouldn't be freed */
      while (cur->next && cur->next->x <= res.x + width)
      {
         stbrp_node *next = cur->next;

         /* move the current node to the free list */
         cur->next = context->free_head;
         context->free_head = cur;
         cur = next;
      }

      /* stitch the list back in */
      node->next = cur;

      if (cur->x < res.x + width)
         cur->x = (uint16_t) (res.x + width);
   }
   return res;
}

static int rect_height_compare(const void *a, const void *b)
{
   stbrp_rect *p = (stbrp_rect *) a;
   stbrp_rect *q = (stbrp_rect *) b;
   if (p->h > q->h)
      return -1;
   if (p->h < q->h)
      return  1;
   return (p->w > q->w) ? -1 : (p->w < q->w);
}

static int rect_original_order(const void *a, const void *b)
{
   stbrp_rect *p = (stbrp_rect *) a;
   stbrp_rect *q = (stbrp_rect *) b;
   return (p->was_packed < q->was_packed) ? -1 : (p->was_packed > q->was_packed);
}

#define STBRP__MAXVAL  0xffff

void stbrp_pack_rects(stbrp_context *context, stbrp_rect *rects, int num_rects)
{
   int i;

   /* we use the 'was_packed' field internally to allow sorting/unsorting */
   for (i=0; i < num_rects; ++i)
      rects[i].was_packed = i;

   /* sort according to heuristic */
   qsort(rects, num_rects, sizeof(rects[0]), rect_height_compare);

   for (i=0; i < num_rects; ++i)
   {
      stbrp__findresult fr = stbrp__skyline_pack_rectangle(context, rects[i].w, rects[i].h);
      if (fr.prev_link)
      {
         rects[i].x = (uint16_t) fr.x;
         rects[i].y = (uint16_t) fr.y;
      }
      else
         rects[i].x = rects[i].y = STBRP__MAXVAL;
   }

   /* unsort */
   qsort(rects, num_rects, sizeof(rects[0]), rect_original_order);

   /* set was_packed flags */
   for (i=0; i < num_rects; ++i)
      rects[i].was_packed = !(rects[i].x == STBRP__MAXVAL && rects[i].y == STBRP__MAXVAL);
}
