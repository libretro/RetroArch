#include "nbio.h"
#include <string.h>
#include <stdio.h>

int main()
{
	struct nbio_t* write = nbio_open("test.bin", NBIO_WRITE);
	nbio_resize(write, 1024*1024);
	
	size_t size;
	void* ptr = nbio_get_ptr(write, &size);
	if (size != 1024*1024) puts("ERROR: wrong size (1)");
	
	memset(ptr, 0x42, 1024*1024);
	nbio_begin_write(write);
	
	size_t prog;
	bool looped=false;
	while (!nbio_iterate(write, &prog, &size))
	{
		printf("%u/%u\n", (unsigned)prog, (unsigned)size);
		looped=true;
	}
	if (!looped) puts("Write finished immediately?");
	nbio_free(write);
	
	
	struct nbio_t* read = nbio_open("test.bin", NBIO_READ);
	
	ptr = nbio_get_ptr(read, &size);
	if (size != 1024*1024) puts("ERROR: wrong size (2)");
	if (ptr) puts("Read pointer is available before iterating?");
	
	nbio_begin_read(read);
	
	looped=false;
	while (!nbio_iterate(read, &prog, &size))
	{
		printf("%u/%u\n", (unsigned)prog, (unsigned)size);
		looped=true;
	}
	if (!looped) puts("Read finished immediately?");
	
	ptr = nbio_get_ptr(read, &size);
	if (size != 1024*1024) puts("ERROR: wrong size (3)");
	if (*(char*)ptr != 0x42 || memcmp(ptr, (char*)ptr+1, 1024*1024-1)) puts("ERROR: wrong data");
	
	nbio_free(read);
}
