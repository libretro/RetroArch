/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (c) 2016, 2017, 2018 vitasdk
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <reent.h>
#include <vitasdk.h>

extern unsigned int _newlib_heap_size_user __attribute__((weak));

int _newlib_heap_memblock;
unsigned _newlib_heap_size;
static char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
static char _newlib_sbrk_mutex[32] __attribute__((aligned(8)));

void *_sbrk_r(struct _reent *reent, ptrdiff_t incr) {
	if (sceKernelLockLwMutex((SceKernelLwMutexWork *)_newlib_sbrk_mutex, 1, 0) < 0)
		goto fail;
	if (!_newlib_heap_base || _newlib_heap_cur + incr >= _newlib_heap_end) {
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)_newlib_sbrk_mutex, 1);
	fail:
		reent->_errno = ENOMEM;
		return (void *)-1;
	}

	char *prev_heap_end = _newlib_heap_cur;
	_newlib_heap_cur += incr;

	sceKernelUnlockLwMutex((SceKernelLwMutexWork *)_newlib_sbrk_mutex, 1);
	return (void *)prev_heap_end;
}

void _init_vita_heap(void) {
	// Create a mutex to use inside _sbrk_r
	if (sceKernelCreateLwMutex((SceKernelLwMutexWork *)_newlib_sbrk_mutex, "sbrk mutex", 0, 0, 0) < 0) {
		goto failure;
	}
	if (&_newlib_heap_size_user != NULL) {
		_newlib_heap_size = _newlib_heap_size_user;
	} else {
		// Create a memblock for the heap memory, 32MB
		_newlib_heap_size = 32 * 1024 * 1024;
	}
	_newlib_heap_memblock = sceKernelAllocMemBlock("Newlib heap", 0x0c20d060, _newlib_heap_size, 0);
	if (_newlib_heap_memblock < 0) {
		goto failure;
	}
	if (sceKernelGetMemBlockBase(_newlib_heap_memblock, (void *)&_newlib_heap_base) < 0) {
		goto failure;
	}
	_newlib_heap_end = _newlib_heap_base + _newlib_heap_size;
	_newlib_heap_cur = _newlib_heap_base;

	return;
failure:
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}

void _free_vita_heap(void) {
	// Destroy the sbrk mutex
	sceKernelDeleteLwMutex((SceKernelLwMutexWork *)_newlib_sbrk_mutex);

	// Free the heap memblock to avoid memory leakage.
	sceKernelFreeMemBlock(_newlib_heap_memblock);

	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}
