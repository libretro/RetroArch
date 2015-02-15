#include "nbio.h"
#include <stdio.h>
#include <stdlib.h>

struct nbio_t {
	FILE* f;
	void* data;
	size_t progress;
	size_t len;
	char op;
};

static const char * modes[]={ "rb", "wb", "r+b" };
struct nbio_t* nbio_open(const char * filename, enum nbio_mode_t mode)
{
	FILE* f=fopen(filename, modes[mode]);
	if (!f) return NULL;
	
	struct nbio_t* handle=(struct nbio_t*)malloc(sizeof(struct nbio_t));
	handle->f = f;
	if (mode != NBIO_WRITE)
	{
		fseek(handle->f, 0, SEEK_END);
		handle->len = ftell(handle->f);
	}
	else handle->len = 0;
	handle->data = malloc(handle->len);
	handle->progress = handle->len;
	handle->op = -1;
}

void nbio_begin_read(struct nbio_t* handle)
{
	handle->op = NBIO_READ;
	handle->progress = 0;
}

void nbio_begin_write(struct nbio_t* handle)
{
	handle->op = NBIO_WRITE;
	handle->progress = 0;
}

bool nbio_iterate(struct nbio_t* handle, size_t* progress, size_t* len)
{
	size_t amount = 65536;
	if (amount > handle->len - handle->progress)
	{
		amount = handle->len - handle->progress;
	}
	if (handle->op == NBIO_READ)
	{
		fread((char*)handle->ptr + handle->progress, 1,amount, handle->f);
	}
	if (handle->op == NBIO_WRITE)
	{
		fwrite((char*)handle->ptr + handle->progress, 1,amount, handle->f);
	}
	handle->progress += amount;
	if (progress) *progress = handle->progress;
	if (len) *len = handle->len;
	if (handle->progress == handle->len) handle->op = -1;
	return (handle->op == -1);
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
	if (len < handle->len)
	{
		puts("ERROR - attempted file shrink operation, not implemented");
		abort();
	}
	handle->len = len;
}

void* nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
	if (len) *len = handle->len;
	if (handle->op == -1) return handle->data;
	else return NULL;
}

void nbio_free(struct nbio_t* handle)
{
	fclose(handle->f);
	free(handle->data);
}
