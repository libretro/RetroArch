#ifndef _MAIN_H
#define _MAIN_H

#include "wiiu/types.h"

struct main_hooks {
  void (*fs_mount)(void);
  void (*fs_unmount)(void);
};

typedef struct main_hooks hooks_t;

extern hooks_t hooks;
extern bool iosuhaxMount;

#endif /* _MAIN_H */
