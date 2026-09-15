/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (mpsc_stack.h).
 * ---------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_MPSC_STACK_H__
#define __LIBRETRO_SDK_MPSC_STACK_H__

/* A many-producer, single-consumer intrusive stack: any thread pushes,
 * one thread takes everything at once.  The shape a worker-to-main
 * handoff wants - deferred messages, retire lists - where the consumer
 * drains on its own cadence and order within the batch is recovered by
 * reversing, since a push stacks newest-first.
 *
 * Producers never wait and never touch a lock; the consumer's drain is
 * one exchange.  There is no ABA hazard in this shape: nodes are only
 * pushed, and the consumer takes the whole chain, so a producer's
 * compare-exchange retries against a moved head and nothing else.
 *
 * The node's link field belongs to the stack from push until the
 * consumer has taken the chain; everything else in the node is
 * published by the push and safe for the consumer to read after
 * mpsc_stack_drain() returned it.
 */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

typedef struct mpsc_stack_node
{
   struct mpsc_stack_node *next;
} mpsc_stack_node_t;

typedef struct mpsc_stack
{
   retro_atomic_ptr_t head;
} mpsc_stack_t;

static INLINE void mpsc_stack_init(mpsc_stack_t *stack)
{
   retro_atomic_ptr_init(&stack->head, NULL);
}

/* Any thread. The node's payload must be written before the push;
 * the compare-exchange publishes it. */
static INLINE void mpsc_stack_push(mpsc_stack_t *stack,
      mpsc_stack_node_t *node)
{
   do
   {
      node->next = (mpsc_stack_node_t *)
            retro_atomic_load_acquire_ptr(&stack->head);
   } while (!retro_atomic_cas_ptr(&stack->head, node->next, node));
}

/* True when a drain would return nothing: the consumer's cheap
 * per-iteration probe. */
static INLINE bool mpsc_stack_empty(mpsc_stack_t *stack)
{
   return !retro_atomic_load_acquire_ptr(&stack->head);
}

/* The consumer. Takes the whole chain, newest-first; the caller walks
 * or reverses it. */
static INLINE mpsc_stack_node_t *mpsc_stack_drain(mpsc_stack_t *stack)
{
   return (mpsc_stack_node_t *)
         retro_atomic_exchange_ptr(&stack->head, NULL);
}

/* The drained chain, oldest-first. */
static INLINE mpsc_stack_node_t *mpsc_stack_reverse(mpsc_stack_node_t *node)
{
   mpsc_stack_node_t *chain = NULL;
   while (node)
   {
      mpsc_stack_node_t *next = node->next;
      node->next = chain;
      chain      = node;
      node       = next;
   }
   return chain;
}

RETRO_END_DECLS

#endif
