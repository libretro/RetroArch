/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (task_content_prefetch.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef TASK_CONTENT_PREFETCH_H
#define TASK_CONTENT_PREFETCH_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Reads the bytes of a set of content paths ahead of the load, a
 * byte-budgeted slice per task tick, so a large ROM - plain or a ZIP
 * entry decompressing as it is read - streams in while the frontend
 * keeps running, instead of freezing it for one long gulp.
 *
 * Paths may be plain files or "archive#entry".  Each completed path
 * is handed to the deposit callback (main thread, ownership of the
 * exact-size free()able buffer transfers); when every path has been
 * deposited or failed, done_cb fires (also main thread).  A path
 * that cannot be prefetched (a 7z solid block, a vanished file) is
 * simply skipped - the load's ordinary read path remains the
 * authority, and a missing deposit costs a blocking read, not
 * correctness. */
typedef void (*content_prefetch_deposit_t)(void *ud, const char *path,
      uint8_t *data, size_t size);
typedef void (*content_prefetch_done_t)(void *ud, bool all_ok);

bool task_push_content_prefetch(const char **paths, size_t count,
      content_prefetch_deposit_t deposit, content_prefetch_done_t done,
      void *ud);

RETRO_END_DECLS

#endif
