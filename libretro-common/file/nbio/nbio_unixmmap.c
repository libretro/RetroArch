#include <stdio.h>
#include <stdlib.h>

#include <file/nbio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

struct nbio_t
{
   int fd;
   int map_flags;
   size_t len;
   void* ptr;
};

struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   static const int o_flags[] =   { O_RDONLY,  O_RDWR|O_CREAT|O_TRUNC, O_RDWR,               O_RDONLY,  O_RDWR|O_CREAT|O_TRUNC };
   static const int map_flags[] = { PROT_READ, PROT_WRITE|PROT_READ,   PROT_WRITE|PROT_READ, PROT_READ, PROT_WRITE|PROT_READ   };
   
   int fd;
   size_t len;
   void* ptr;
   struct nbio_t* handle;
   
   fd = open(filename, o_flags[mode]|O_CLOEXEC, 0644);
   if (fd < 0) return NULL;
   
   len = lseek(fd, 0, SEEK_END);
   if (len != 0)
   {
      ptr = mmap(NULL, len, map_flags[mode], MAP_SHARED, fd, 0);
   }
   else
   {
      ptr = NULL;
   }
   if (ptr == MAP_FAILED)
   {
      close(fd);
      return NULL;
   }
   
   handle = malloc(sizeof(struct nbio_t));
   handle->fd = fd;
   handle->map_flags = map_flags[mode];
   handle->len = len;
   handle->ptr = ptr;
   return handle;
}

void nbio_begin_read(struct nbio_t* handle)
{
   /* not needed */
}

void nbio_begin_write(struct nbio_t* handle)
{
   /* not needed */
}

bool nbio_iterate(struct nbio_t* handle)
{
   return true; /* not needed */
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
   if (ftruncate(handle->fd, len) != 0)
   {
      puts("ERROR - couldn't resize file (ftruncate)");
      abort(); /* this one returns void and I can't find any other way for it to report failure */
   }
   munmap(handle->ptr, handle->len);
   handle->ptr = mmap(NULL, len, handle->map_flags, MAP_SHARED, handle->fd, 0);
   handle->len = len;
   if (handle->ptr == MAP_FAILED)
   {
      puts("ERROR - couldn't resize file (mmap)");
      abort();
   }
}

void* nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
   if (len) *len = handle->len;
   return handle->ptr;
}

void nbio_cancel(struct nbio_t* handle)
{
   /* not needed */
}

void nbio_free(struct nbio_t* handle)
{
   close(handle->fd);
   munmap(handle->ptr, handle->len);
   free(handle);
}
