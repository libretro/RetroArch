#include <stdio.h>
#include <stdlib.h>

#include <file/nbio.h>

#include <windows.h>

struct nbio_t
{
   HANDLE file;
   bool is_write;
   size_t len;
   void* ptr;
};

#define FILE_SHARE_ALL (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   static const DWORD dispositions[] = { OPEN_EXISTING, CREATE_ALWAYS, OPEN_ALWAYS, OPEN_EXISTING, CREATE_ALWAYS };
   
   bool is_write = (mode == NBIO_WRITE || mode == NBIO_UPDATE || mode == BIO_WRITE);
   DWORD access = (is_write ? GENERIC_READ|GENERIC_WRITE : GENERIC_READ);
   HANDLE file = CreateFile(filename, access, FILE_SHARE_ALL, NULL, dispositions[mode], FILE_ATTRIBUTE_NORMAL, NULL);
   
   HANDLE mem;
   
   void* ptr;
   LARGE_INTEGER len;
   struct nbio_t* handle;
   
   if (file == INVALID_HANDLE_VALUE) return NULL;
   
   GetFileSizeEx(file, &len);
   
   mem = CreateFileMapping(file, NULL, is_write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
   ptr = MapViewOfFile(mem, is_write ? (FILE_MAP_READ|FILE_MAP_WRITE) : FILE_MAP_READ, 0, 0, len.QuadPart);
   CloseHandle(mem);
   
   handle = malloc(sizeof(struct nbio_t));
   handle->file = file;
   handle->is_write = is_write;
   handle->len = len.QuadPart;
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
   LARGE_INTEGER len_li;
   HANDLE mem;
   
   if (len < handle->len)
   {
      /* this works perfectly fine if this check is removed, but it won't work on other nbio implementations */
      /* therefore, it's blocked so nobody accidentally relies on it */
      puts("ERROR - attempted file shrink operation, not implemented");
      abort();
   }
   
   len_li.QuadPart = len;
   SetFilePointerEx(handle->file, len_li, NULL, FILE_BEGIN);
   
   if (!SetEndOfFile(handle->file))
   {
      puts("ERROR - couldn't resize file (SetEndOfFile)");
      abort(); /* this one returns void and I can't find any other way for it to report failure */
   }
   handle->len = len;
   
   UnmapViewOfFile(handle->ptr);
   mem = CreateFileMapping(handle->file, NULL, handle->is_write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, NULL);
   handle->ptr = MapViewOfFile(mem, handle->is_write ? (FILE_MAP_READ|FILE_MAP_WRITE) : FILE_MAP_READ, 0, 0, len);
   CloseHandle(mem);
   
   if (handle->ptr == NULL)
   {
      puts("ERROR - couldn't resize file (MapViewOfFile)");
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
   CloseHandle(handle->file);
   UnmapViewOfFile(handle->ptr);
   free(handle);
}
