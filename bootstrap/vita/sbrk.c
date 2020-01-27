#include <errno.h>
#include <reent.h>
#include "../../defines/psp_defines.h"
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

#define RAM_THRESHOLD 0x1000000 // Memory left to the system for threads and other internal stuffs

int _newlib_heap_memblock;
unsigned _newlib_heap_size;
char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
static char _newlib_sbrk_mutex[32] __attribute__ ((aligned (8)));

static int _newlib_vm_memblock;

extern int _newlib_heap_size_user __attribute__((weak));
extern int _newlib_vm_size_user __attribute__((weak));

void * _sbrk_r(struct _reent *reent, ptrdiff_t incr) {
	if (sceKernelLockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1, 0) < 0)
		goto fail;
	if (!_newlib_heap_base || _newlib_heap_cur + incr >= _newlib_heap_end) {
		sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
fail:
		reent->_errno = ENOMEM;
		return (void*)-1;
	}

	char *prev_heap_end = _newlib_heap_cur;
	_newlib_heap_cur += incr;

	sceKernelUnlockLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, 1);
	return (void*) prev_heap_end;
}

void _init_vita_heap(void) {

	int _newlib_vm_size = 0;
	if (&_newlib_vm_size_user != NULL) {
		_newlib_vm_size = _newlib_vm_size_user;
	  _newlib_vm_memblock = sceKernelAllocMemBlockForVM("code", _newlib_vm_size_user);

	  if (_newlib_vm_memblock < 0){
	    //sceClibPrintf("sceKernelAllocMemBlockForVM failed\n");
			goto failure;
		}
	}else{
		_newlib_vm_memblock = 0;
	}

	// Create a mutex to use inside _sbrk_r
	if (sceKernelCreateLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex, "sbrk mutex", 0, 0, 0) < 0) {
		goto failure;
	}
	
	// Always allocating the max avaliable USER_RW mem on the system
	SceKernelFreeMemorySizeInfo info;
	info.size = sizeof(SceKernelFreeMemorySizeInfo);
	sceKernelGetFreeMemorySize(&info);

	if (&_newlib_heap_size_user != NULL) {
		_newlib_heap_size = _newlib_heap_size_user;
	}else{
		_newlib_heap_size = info.size_user - RAM_THRESHOLD;
	}

	_newlib_heap_size -= _newlib_vm_size;

	_newlib_heap_memblock = sceKernelAllocMemBlock("Newlib heap", 0x0c20d060, _newlib_heap_size, 0);
	if (_newlib_heap_memblock < 0) {
		goto failure;
	}
	if (sceKernelGetMemBlockBase(_newlib_heap_memblock, (void**)&_newlib_heap_base) < 0) {
		goto failure;
	}
	_newlib_heap_end = _newlib_heap_base + _newlib_heap_size;
	_newlib_heap_cur = _newlib_heap_base;

	return;
failure:
	_newlib_vm_memblock = 0;
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}

int getVMBlock(){
  return _newlib_vm_memblock;
}

void _free_vita_heap(void) {

	// Destroy the sbrk mutex
	sceKernelDeleteLwMutex((struct SceKernelLwMutexWork*)_newlib_sbrk_mutex);

	// Free the heap memblock to avoid memory leakage.
	sceKernelFreeMemBlock(_newlib_heap_memblock);

	if(_newlib_vm_memblock)
  	sceKernelFreeMemBlock(_newlib_vm_memblock);

	_newlib_vm_memblock = 0;
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}
