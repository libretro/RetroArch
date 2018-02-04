#include <stdlib.h>
#include "lwp_stack.h"
#include "lwp_wkspace.h"

u32 __lwp_stack_allocate(lwp_cntrl *thethread,u32 size)
{
	void *stack_addr = NULL;

	if(!__lwp_stack_isenough(size))
		size = CPU_MINIMUM_STACK_SIZE;

	size = __lwp_stack_adjust(size);
	stack_addr = __lwp_wkspace_allocate(size);

	if(!stack_addr) size = 0;

	thethread->stack = stack_addr;
	return size;
}

void __lwp_stack_free(lwp_cntrl *thethread)
{
	if(!thethread->stack_allocated)
		return;

	__lwp_wkspace_free(thethread->stack);
}
