#ifndef _FRONTEND_CONSOLE_H
#define _FRONTEND_CONSOLE_H

#ifdef GEKKO
#define SALAMANDER_FILE "boot.dol"
#define DEFAULT_EXE_EXT ".dol"
#elif defined(__CELLOS_LV2__)
#define SALAMANDER_FILE "EBOOT.BIN"
#define DEFAULT_EXE_EXT ".SELF"
#elif defined(_XBOX1)
#define SALAMANDER_FILE "default.xbe"
#define DEFAULT_EXE_EXT ".xbe"
#elif defined(_XBOX360)
#define SALAMANDER_FILE "default.xex"
#define DEFAULT_EXE_EXT ".xex"
#endif

#endif
