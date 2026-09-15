/* Force-included into filestream_rbuf_fault_test's private copy of
 * file_stream.o, which is built with
 * -DFILESTREAM_RBUF_MALLOC=filestream_rbuf_fault_malloc.  Nothing else
 * includes this, and no other object in the tree is built that way. */

#ifndef _RBUF_FAULT_HOOKS_H
#define _RBUF_FAULT_HOOKS_H

#include <stddef.h>

void *filestream_rbuf_fault_malloc(size_t len);

#endif
