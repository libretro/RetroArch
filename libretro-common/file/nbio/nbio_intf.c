#include <stdio.h>
#include <stdlib.h>

#include <file/nbio.h>

extern nbio_intf_t nbio_stdio;

static nbio_intf_t *internal_nbio = &nbio_stdio;

struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   return internal_nbio->open(filename, mode);
}

void nbio_begin_read(struct nbio_t* handle)
{
   internal_nbio->begin_read(handle);
}

void nbio_begin_write(struct nbio_t* handle)
{
   internal_nbio->begin_write(handle);
}

bool nbio_iterate(struct nbio_t* handle)
{
   return internal_nbio->iterate(handle);
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
   internal_nbio->resize(handle, len);
}

void *nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
   return internal_nbio->get_ptr(handle, len);
}

void nbio_cancel(struct nbio_t* handle)
{
   internal_nbio->cancel(handle);
}

void nbio_free(struct nbio_t* handle)
{
   internal_nbio->free(handle);
}
