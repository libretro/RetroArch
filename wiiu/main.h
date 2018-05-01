#ifndef WIIU_MAIN__H
#define WIIU_MAIN__H

#include "hbl.h"
#include "wiiu_dbg.h"

#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/dynamic.h"
#include "system/memory.h"
#include "system/exception_handler.h"

#include "../retroarch.h"
#include "../verbosity.h"
#include "../frontend/frontend.h"
#include "../gfx/video_driver.h"
#include "../tasks/tasks_internal.h"

void __init(void);
void __fini(void);
int main(int argc, char **argv);

#endif /* WIIU_MAIN_H */
