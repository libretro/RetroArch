/*
  libco.genode_secondary_stack (2018-09-15)
  author: Emery Hemingway
  license: public domain
*/

/* Genode include */
#include <base/thread.h>

/* Libco include */
#include <libco.h>

extern "C"
void *genode_alloc_secondary_stack(unsigned long stack_size)
{
	try {
		return Genode::Thread::myself()->alloc_secondary_stack("libco", stack_size); }
	catch (...) {
		Genode::error("libco: failed to allocate ", stack_size, " byte secondary stack");
		return nullptr;
	}

}

extern "C"
void genode_free_secondary_stack(void *stack)
{
	Genode::Thread::myself()->free_secondary_stack(stack);
}
