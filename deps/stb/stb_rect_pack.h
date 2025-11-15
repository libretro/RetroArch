/*
 * stb_rect_pack.h - v0.06 - public domain - rectangle packing
 * Sean Barrett 2014
 *
 * Useful for e.g. packing rectangular textures into an atlas.
 * Does not do rotation.
 *
 * Not necessarily the awesomest packing method, but better than
 * the totally naive one in stb_truetype (which is primarily what
 * this is meant to replace).
 *
 * Has only had a few tests run, may have issues.
 *
 * More docs to come.
 *
 * No memory allocations; uses qsort() from stdlib.
 *
 * This library currently uses the Skyline Bottom-Left algorithm.
 *
 * Please note: better rectangle packers are welcome! Please
 * implement them to the same API, but with a different init
 * function.
 *
 * Credits
 *
 *  Library
 *    Sean Barrett
 *  Minor features
 *    Martins Mozeiko
 *  Bugfixes / warning fixes
 *    [your name could be here]
*/

#include <stdint.h>
#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

#ifndef STB_INCLUDE_STB_RECT_PACK_H
#define STB_INCLUDE_STB_RECT_PACK_H

#define STB_RECT_PACK_VERSION  1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stbrp_context stbrp_context;
typedef struct stbrp_node    stbrp_node;
typedef struct stbrp_rect    stbrp_rect;

/* Initialize a rectangle packer to:
 *    pack a rectangle that is 'width' by 'height' in dimensions
 *    using temporary storage provided by the array 'nodes', which is 'num_nodes' long
 *
 * You must call this function every time you start packing into a new target.
 *
 * There is no "shutdown" function. The 'nodes' memory must stay valid for
 * the following stbrp_pack_rects() call (or calls), but can be freed after
 * the call (or calls) finish.
 *
 * Note: to guarantee best results, either:
 *       1. make sure 'num_nodes' >= 'width'
 *   or  2. call stbrp_allow_out_of_mem() defined below with 'allow_out_of_mem = 1'
 *
 * If you don't do either of the above things, widths will be quantized to multiples
 * of small integers to guarantee the algorithm doesn't run out of temporary storage.
 *
 * If you do #2, then the non-quantized algorithm will be used, but the algorithm
 * may run out of temporary storage and be unable to pack some rectangles.
 */

enum
{
   STBRP_HEURISTIC_Skyline_default=0,
   STBRP_HEURISTIC_Skyline_BL_sortHeight = STBRP_HEURISTIC_Skyline_default,
   STBRP_HEURISTIC_Skyline_BF_sortHeight
};

/* the details of the following structures don't matter to you, but they must
 * be visible so you can handle the memory allocations for them
 */

struct stbrp_node
{
   uint16_t  x,y;
   stbrp_node  *next;
};

struct stbrp_context
{
   int width;
   int height;
   int align;
   int init_mode;
   int heuristic;
   int num_nodes;
   stbrp_node *active_head;
   stbrp_node *free_head;
   stbrp_node extra[2]; /* we allocate two extra nodes so optimal user-node-count is 'width' not 'width+2' */
};

/* Assign packed locations to rectangles. The rectangles are of type
 * 'stbrp_rect' defined below, stored in the array 'rects', and there
 * are 'num_rects' many of them.
 *
 * Rectangles which are successfully packed have the 'was_packed' flag
 * set to a non-zero value and 'x' and 'y' store the minimum location
 * on each axis (i.e. bottom-left in cartesian coordinates, top-left
 * if you imagine y increasing downwards). Rectangles which do not fit
 * have the 'was_packed' flag set to 0.
 *
 * You should not try to access the 'rects' array from another thread
 * while this function is running, as the function temporarily reorders
 * the array while it executes.
 *
 * To pack into another rectangle, you need to call stbrp_init_target
 * again. To continue packing into the same rectangle, you can call
 * this function again. Calling this multiple times with multiple rect
 * arrays will probably produce worse packing results than calling it
 * a single time with the full rectangle array, but the option is
 * available.
 */

struct stbrp_rect
{
   int            id;          /* reserved for your use: */
   uint16_t    w, h;        /* input: */
   uint16_t    x, y;        /* output: */
   int            was_packed;  /* non-zero if valid packing */
}; /* 16 bytes, nominally */

void stbrp_pack_rects (stbrp_context *context,
      stbrp_rect *rects, int num_rects);

void stbrp_init_target (stbrp_context *context,
      int width, int height, stbrp_node *nodes, int num_nodes);

RETRO_END_DECLS

#endif
