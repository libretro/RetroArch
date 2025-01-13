/*
 * Copyright (c) 2010-2020 The RetroArch team
 * Copyright (c) 2017 John Schember <john@nachtimwald.com>
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (tpool.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#ifndef __LIBRETRO_SDK_TPOOL_H__
#define __LIBRETRO_SDK_TPOOL_H__

#include <retro_common_api.h>

#include <boolean.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

struct tpool;
typedef struct tpool tpool_t;

/**
 * (*thread_func_t):
 * @arg           : Argument.
 *
 * Callback function that the pool will call to do work.
 **/
typedef void (*thread_func_t)(void *arg);

/**
 * tpool_create:
 * @num           : Number of threads the pool should have.
 *                  If 0 defaults to 2.
 *
 * Create a thread pool.
 *
 * Returns: pool.
 */
tpool_t *tpool_create(size_t num);

/**
 * tpool_destroy:
 * @tp            : Thread pool.
 *
 * Destroy a thread pool
 * The pool can be destroyed while there is outstanding work to process. All
 * outstanding unprocessed work will be discarded. There may be a delay before
 * this function returns because it will block for work that is processing to
 * complete.
 **/
void tpool_destroy(tpool_t *tp);

/**
 * tpool_add_work:
 * @tp         : Thread pool.
 * @func       : Function the pool should call.
 * @arg        : Argument to pass to func.
 *
 * Add work to a thread pool.
 *
 * Returns: true if work was added, otherwise false.
 **/
bool tpool_add_work(tpool_t *tp, thread_func_t func, void *arg);

/**
 * tpool_wait:
 * @tp Thread pool.
 *
 * Wait for all work in the pool to be completed.
 */
void tpool_wait(tpool_t *tp);

RETRO_END_DECLS

#endif
