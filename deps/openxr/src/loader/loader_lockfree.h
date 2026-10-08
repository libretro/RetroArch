/* Copyright (c) 2026 libretro contributors
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Lock-free publication for the loader's shared tables.
 *
 * A table is an immutable snapshot behind one pointer. Readers
 * acquire-load it and walk it, with no lock and no read-modify-write.
 * Writers build a new snapshot from the current one and publish it with
 * a compare-exchange, retrying against whatever moved it. A superseded
 * snapshot goes onto its owner's retire stack and is freed when the
 * owner is, so a reader still walking it never touches freed memory and
 * a pointer the compare-exchange tests is never reused under it.
 */

#ifndef LOADER_LOCKFREE_H
#define LOADER_LOCKFREE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <queues/mpsc_stack.h>

#if !defined(RETRO_ATOMIC_HAS_PTR)
#error "The OpenXR loader needs pointer atomics (RETRO_ATOMIC_HAS_PTR)."
#endif

/* The node is first, so a snapshot is its own retire-stack node. The
 * union keeps the items that follow aligned for a uint64_t on 32-bit
 * targets. */
typedef struct loader_snapshot {
    union {
        mpsc_stack_node_t node;
        unsigned long long align_;
    } retire;
    size_t count;
    size_t align_pad_;
} loader_snapshot_t;

#define LOADER_SNAPSHOT_ITEMS(s, type) ((type*)((loader_snapshot_t*)(s) + 1))

/* A snapshot with room for count items of item_size bytes and extra
 * trailing bytes; NULL when out of memory. */
static INLINE loader_snapshot_t* loader_snapshot_new(size_t count, size_t item_size, size_t extra) {
    loader_snapshot_t* s = (loader_snapshot_t*)malloc(sizeof(*s) + count * item_size + extra);
    if (s) {
        s->retire.node.next = NULL;
        s->count = count;
        s->align_pad_ = 0;
    }
    return s;
}

/* Frees every snapshot on a retire stack. Only for the stack's owner,
 * once no reader can still hold one. */
static INLINE void loader_snapshot_free_retired(mpsc_stack_t* retired) {
    mpsc_stack_node_t* node = mpsc_stack_drain(retired);
    while (node) {
        mpsc_stack_node_t* next = node->next;
        free(node);
        node = next;
    }
}

#endif /* LOADER_LOCKFREE_H */
