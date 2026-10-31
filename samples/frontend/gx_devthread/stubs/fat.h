/* The part of libfat and libogc's disc interface the frontend uses. */
#ifndef STUB_FAT_H
#define STUB_FAT_H
#include <boolean.h>
typedef struct
{
   bool (*startup)(void);
   bool (*isInserted)(void);
} DISC_INTERFACE;
bool fatInitDefault(void);
bool fatMountSimple(const char *name, const DISC_INTERFACE *interface);
void fatUnmount(const char *name);
#endif
