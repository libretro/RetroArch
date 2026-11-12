/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_deque.h).
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

#ifndef __LIBRETRO_SDK_RETRO_DEQUE_H
#define __LIBRETRO_SDK_RETRO_DEQUE_H

/*
 * retro_deque.h - a double-ended queue of one type, in one block
 *
 * What std::deque is used for - push at either end, pop at either end,
 * the n-th element, insert and erase in the middle, in order - without
 * what it costs: std::deque keeps its elements in blocks of a few dozen
 * and takes a block from the heap, or gives one back, each time its
 * contents cross a block's end. A queue that is pushed at one end and
 * popped at the other does that for ever, however few elements it holds.
 *
 * Here the elements are a ring in one block, a power of two long, set
 * aside when the queue is made for as many elements as the caller says it
 * holds. Pushing, popping, inserting and erasing never allocate and never
 * free. A push or insert into a queue that holds its limit is refused,
 * with one compare (the caller decides: drop, wait, or reserve more - the
 * one call besides init that allocates). Where the old code relied on a
 * queue with no limit, push_back_growing reserves twice as much when the
 * limit is reached, and so allocates only then.
 *
 * The functions are made for the element's type with
 * RETRO_DEQUE_DEFINE(name, type), so an element is copied as what it is
 * (a compare-and-mask and a store for a word-sized one) and not through
 * memcpy with a size known only when the program runs:
 *
 *   struct deadline { uint64_t when; void *who; };
 *   RETRO_DEQUE_DEFINE(deadlines, struct deadline)
 *
 *   deadlines_t q;
 *   deadlines_init(&q, 60);                (holds 60, in a block of 64)
 *   deadlines_push_back(&q, &d);           (false when full)
 *   deadlines_insert(&q, 3, &d);           (before the 4th oldest)
 *   d = *deadlines_front(&q);
 *   deadlines_pop_front(&q);
 *   deadlines_erase(&q, i);
 *   deadlines_free(&q);
 *
 * Insert and erase move whichever side of the place is shorter, so the
 * ends cost one element and the middle half the queue.
 *
 * The type has to be one C copies by assignment (anything C89 has; in
 * C++, a trivially copyable type). Not thread safe: one thread, or the
 * caller's own lock. C89; MSVC compiles it.
 *
 * The allocator is malloc and free unless RETRO_DEQUE_MALLOC and
 * RETRO_DEQUE_FREE are defined before this header is included.
 */

#include <stddef.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>

#ifndef RETRO_DEQUE_MALLOC
#define RETRO_DEQUE_MALLOC(bytes) malloc(bytes)
#endif
#ifndef RETRO_DEQUE_FREE
#define RETRO_DEQUE_FREE(ptr) free(ptr)
#endif

/* C's casts in C, C++'s in C++: a C++ unit that warns of old-style casts
 * (or refuses them) includes this as it is. */
#ifdef __cplusplus
#define RETRO_DEQUE_CAST(to, value) static_cast<to>(value)
#else
#define RETRO_DEQUE_CAST(to, value) ((to)(value))
#endif

#define RETRO_DEQUE_DEFINE(name, type) \
/* The element's type by one name, so that "const" in front of it means \
 * the element whatever the type is spelled as: with a pointer type \
 * written out, "const type *" would be a pointer to a pointer to const. */ \
typedef type name##_value_t; \
typedef struct name \
{ \
   name##_value_t *data; /* mask + 1 elements */ \
   size_t mask;   /* the block's length, a power of two, less one */ \
   size_t first;  /* where the oldest is */ \
   size_t count; \
   size_t limit;  /* how many it holds: what init or reserve was asked for; \
                   * 0 while it has no block */ \
} name##_t; \
\
/* A queue that holds @limit elements, no more (its block is the next \
 * power of two). False when there is no memory; the queue then holds \
 * nothing and refuses every push. */ \
static INLINE bool name##_init(name##_t *q, size_t limit) \
{ \
   size_t capacity = 1; \
   while (capacity < limit && capacity < RETRO_DEQUE_CAST(size_t, -1) / 2 / sizeof(name##_value_t)) \
      capacity <<= 1; \
   q->first = 0; \
   q->count = 0; \
   q->mask  = 0; \
   q->limit = 0; \
   q->data  = capacity >= limit ? RETRO_DEQUE_CAST(name##_value_t*, RETRO_DEQUE_MALLOC(capacity * sizeof(name##_value_t))) : NULL; \
   if (!q->data) \
      return false; \
   q->mask  = capacity - 1; \
   q->limit = limit; \
   return true; \
} \
\
/* Safe on a queue whose init failed, and on one already freed */ \
static INLINE void name##_free(name##_t *q) \
{ \
   if (q->data) \
      RETRO_DEQUE_FREE(q->data); \
   q->data  = NULL; \
   q->mask  = 0; \
   q->first = 0; \
   q->count = 0; \
   q->limit = 0; \
} \
\
static INLINE size_t name##_count(const name##_t *q) { return q->count; } \
static INLINE bool   name##_empty(const name##_t *q) { return q->count == 0; } \
static INLINE size_t name##_limit(const name##_t *q) { return q->limit; } \
static INLINE bool   name##_full(const name##_t *q)  { return q->count >= q->limit; } \
\
/* The @i-th oldest; @i below the count */ \
static INLINE name##_value_t *name##_at(const name##_t *q, size_t i) \
{ \
   return &q->data[(q->first + i) & q->mask]; \
} \
\
static INLINE name##_value_t *name##_front(const name##_t *q) { return &q->data[q->first]; } \
static INLINE name##_value_t *name##_back(const name##_t *q) \
{ \
   return &q->data[(q->first + q->count - 1) & q->mask]; \
} \
\
/* Holds @limit from now on (never fewer than it holds already): the \
 * elements move to a larger block when that takes one, in order. False \
 * when there is no memory, the queue then as it was. Pointers into the \
 * queue are invalid afterwards. */ \
static INLINE bool name##_reserve(name##_t *q, size_t limit) \
{ \
   name##_t larger; \
   size_t i; \
   if (limit <= q->limit) \
      return true; \
   if (q->data && limit <= q->mask + 1) \
   { \
      q->limit = limit; \
      return true; \
   } \
   if (!name##_init(&larger, limit)) \
      return false; \
   for (i = 0; i < q->count; i++) \
      larger.data[i] = *name##_at(q, i); \
   larger.count = q->count; \
   if (q->data) \
      RETRO_DEQUE_FREE(q->data); \
   *q = larger; \
   return true; \
} \
\
/* False when the queue is full: nothing changed */ \
static INLINE bool name##_push_back(name##_t *q, const name##_value_t *value) \
{ \
   if (name##_full(q)) \
      return false; \
   q->data[(q->first + q->count) & q->mask] = *value; \
   q->count++; \
   return true; \
} \
\
/* For a queue with no limit of its own (what a std::deque was used as): \
 * when it holds its limit, it is first made to hold twice as many - \
 * that push allocates, the others do not. Give it at init the most it \
 * is expected to hold, and it never will. False only when there is no \
 * memory: nothing changed. */ \
static INLINE bool name##_push_back_growing(name##_t *q, const name##_value_t *value) \
{ \
   if (name##_full(q) && !name##_reserve(q, q->limit ? q->limit * 2 : 8)) \
      return false; \
   q->data[(q->first + q->count) & q->mask] = *value; \
   q->count++; \
   return true; \
} \
\
static INLINE bool name##_push_front(name##_t *q, const name##_value_t *value) \
{ \
   if (name##_full(q)) \
      return false; \
   q->first          = (q->first - 1) & q->mask; \
   q->data[q->first] = *value; \
   q->count++; \
   return true; \
} \
\
/* The queue is not empty */ \
static INLINE void name##_pop_front(name##_t *q) \
{ \
   q->first = (q->first + 1) & q->mask; \
   q->count--; \
} \
\
static INLINE void name##_pop_back(name##_t *q) \
{ \
   q->count--; \
} \
\
/* @value before the @i-th oldest (@i up to the count: at the end). \
 * False when the queue is full: nothing changed */ \
static INLINE bool name##_insert(name##_t *q, size_t i, const name##_value_t *value) \
{ \
   size_t k; \
   if (name##_full(q) || i > q->count) \
      return false; \
   if (i < q->count - i) \
   { \
      /* the older side is the shorter: it moves one place down */ \
      q->first = (q->first - 1) & q->mask; \
      for (k = 0; k < i; k++) \
         q->data[(q->first + k) & q->mask] = q->data[(q->first + k + 1) & q->mask]; \
   } \
   else \
   { \
      for (k = q->count; k > i; k--) \
         q->data[(q->first + k) & q->mask] = q->data[(q->first + k - 1) & q->mask]; \
   } \
   q->data[(q->first + i) & q->mask] = *value; \
   q->count++; \
   return true; \
} \
\
/* The @i-th oldest goes; @i below the count */ \
static INLINE void name##_erase(name##_t *q, size_t i) \
{ \
   size_t k; \
   if (i < q->count - 1 - i) \
   { \
      for (k = i; k > 0; k--) \
         q->data[(q->first + k) & q->mask] = q->data[(q->first + k - 1) & q->mask]; \
      q->first = (q->first + 1) & q->mask; \
   } \
   else \
   { \
      for (k = i; k + 1 < q->count; k++) \
         q->data[(q->first + k) & q->mask] = q->data[(q->first + k + 1) & q->mask]; \
   } \
   q->count--; \
} \
\
static INLINE void name##_clear(name##_t *q) \
{ \
   q->first = 0; \
   q->count = 0; \
} \
\
/* So that a use of the macro ends in a semicolon, or not, as the caller \
 * likes */ \
typedef name##_t name##_deque_defined_t

#endif
