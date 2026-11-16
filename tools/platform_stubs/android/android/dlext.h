/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_android_dlext_h
#define STUB_android_dlext_h
#include <stddef.h>
#include <stdint.h>
enum { ANDROID_DLEXT_USE_LIBRARY_FD = 0x10 };
struct android_namespace_t;
typedef struct
{
   uint64_t flags;
   void *reserved_addr;
   size_t reserved_size;
   int relro_fd;
   int library_fd;
   int64_t library_fd_offset;
   struct android_namespace_t *library_namespace;
} android_dlextinfo;
#endif
