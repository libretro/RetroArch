#include "nbio.h"
#include <stdio.h>
#include <stdlib.h>

struct nbio_t
{
   FILE* f;
   void* data;
   size_t progress;
   size_t len;
   /*
    * possible values:
    * NBIO_READ, NBIO_WRITE - obvious
    * -1 - currently doing nothing
    * -2 - the pointer was reallocated since the last operation
    */
   signed char op;
};

static const char * modes[]={ "rb", "wb", "r+b" };

struct nbio_t* nbio_open(const char * filename, enum nbio_mode_t mode)
{
	struct nbio_t* handle;
	FILE* f=fopen(filename, modes[mode]);
	if (!f)
      return NULL;
	
	handle=(struct nbio_t*)malloc(sizeof(struct nbio_t));

   if (!handle)
   {
      fclose(f);
      return NULL;
   }

	handle->f   = f;
   handle->len = 0;

	if (mode != NBIO_WRITE)
	{
		fseek(handle->f, 0, SEEK_END);
		handle->len = ftell(handle->f);
	}

	handle->data = malloc(handle->len);

   if (handle->len && !handle->data)
   {
      free(handle);
      fclose(f);
      return NULL;
   }

	handle->progress = handle->len;
	handle->op       = -2;
	
	return handle;
}

void nbio_begin_read(struct nbio_t* handle)
{
	if (handle->op >= 0)
	{
		puts("ERROR - attempted file read operation while busy");
		abort();
	}

	fseek(handle->f, 0, SEEK_SET);

	handle->op       = NBIO_READ;
	handle->progress = 0;
}

void nbio_begin_write(struct nbio_t* handle)
{
	if (handle->op >= 0)
	{
		puts("ERROR - attempted file write operation while busy");
		abort();
	}

	fseek(handle->f, 0, SEEK_SET);
	handle->op = NBIO_WRITE;
	handle->progress = 0;
}

bool nbio_iterate(struct nbio_t* handle, size_t* progress, size_t* len)
{
	size_t amount = 65536;

	if (amount > handle->len - handle->progress)
		amount = handle->len - handle->progress;

	if (handle->op == NBIO_READ)
		fread((char*)handle->data + handle->progress, 1,amount, handle->f);
	if (handle->op == NBIO_WRITE)
		fwrite((char*)handle->data + handle->progress, 1,amount, handle->f);

	handle->progress += amount;

	if (progress)
      *progress = handle->progress;
	if (len)
      *len = handle->len;

	if (handle->progress == handle->len)
      handle->op = -1;
	return (handle->op < 0);
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
	if (handle->op >= 0)
	{
		puts("ERROR - attempted file resize operation while busy");
		abort();
	}
	if (len < handle->len)
	{
		puts("ERROR - attempted file shrink operation, not implemented");
		abort();
	}

	handle->len  = len;
	handle->data = realloc(handle->data, handle->len);
	handle->op   = -1;
}

void* nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
	if (len)
      *len = handle->len;
	if (handle->op == -1)
      return handle->data;
	return NULL;
}

void nbio_free(struct nbio_t* handle)
{
	fclose(handle->f);
	free(handle->data);
}
