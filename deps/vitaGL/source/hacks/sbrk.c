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
