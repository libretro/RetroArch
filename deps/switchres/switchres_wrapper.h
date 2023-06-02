/**************************************************************

   switchres_wrapper.h - Switchres C wrapper API header file

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdint.h>
#ifndef SR_DEFINES
#include "switchres_defines.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <dlfcn.h>
#define LIBTYPE void*
#define OPENLIB(libname) dlopen((libname), RTLD_LAZY)
#define LIBFUNC(libh, fn) dlsym((libh), (fn))
#define LIBERROR dlerror
#define CLOSELIB(libh) dlclose((libh))

#elif defined _WIN32
#include <windows.h>
#define LIBTYPE HINSTANCE
#define OPENLIB(libname) LoadLibrary(TEXT((libname)))
#define LIBFUNC(lib, fn) GetProcAddress((lib), (fn))

#define CLOSELIB(libp) FreeLibrary((libp))
#endif

#ifdef _WIN32
/*
 * This is a trick to avoid exporting some functions thus having the binary
 * flagged as a virus. If switchres_wrapper.cpp is included in the compilation
 * LIBERROR() is just declared and not compiled. If switchres_wrapper.cpp is
 * not compiled, LIBERROR is defined here
 */
#ifndef SR_WIN32_STATIC
char* LIBERROR()
{
	DWORD errorMessageID = GetLastError();
	if(errorMessageID == 0)
		return NULL;

	LPSTR messageBuffer;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	SetLastError(0);

	static char error_msg[256] = {0};
	strncpy(error_msg, messageBuffer, sizeof(error_msg)-1);
	LocalFree(messageBuffer);
	return error_msg;
}
#endif /* SR_WIN32_STATIC */
	#ifndef SR_WIN32_STATIC
		#define MODULE_API __declspec(dllexport)
	#else
		#define MODULE_API
	#endif
#else
	#define MODULE_API
#endif /* _WIN32 */

#ifdef __linux__
#define LIBSWR "libswitchres.so"
#elif _WIN32
#define LIBSWR "libswitchres.dll"
#endif

/* Mode flags */
#define SR_MODE_INTERLACED    1<<0
#define SR_MODE_ROTATED       1<<1
#define SR_MODE_DONT_FLUSH    1<<16

/* That's all the exposed data from Switchres calculation */
typedef struct MODULE_API
{
	int      width;
	int      height;
	int      refresh;
	//
	double   vfreq;
	double   hfreq;
	//
	uint64_t pclock;
	int      hbegin;
	int      hend;
	int      htotal;
	int      vbegin;
	int      vend;
	int      vtotal;
	int      interlace;
	int      doublescan;
	int      hsync;
	int      vsync;
	//
	int      is_refresh_off;
	int      is_stretched;
	double   x_scale;
	double   y_scale;
	double   v_scale;
	int      id;
} sr_mode;

/* Used to retrieve SR settings and state */
typedef struct MODULE_API
{
	char     monitor[32];
	int      modeline_generation;
	int      desktop_is_rotated;
	int      interlace;
	int      doublescan;
	double   dotclock_min;
	double   refresh_tolerance;
	int      super_width;
	double   monitor_aspect;
	double   h_size;
	double   h_shift;
	double   v_shift;
	int      pixel_precision;
	int      selected_mode;
	int      current_mode;
} sr_state;

/* Declaration of the wrapper functions */
MODULE_API void sr_init();
MODULE_API char* sr_get_version();
MODULE_API void sr_load_ini(char* config);
MODULE_API void sr_deinit();
MODULE_API int sr_init_disp(const char*, void*);
MODULE_API void sr_set_disp(int);
MODULE_API int sr_get_mode(int, sr_mode*);
MODULE_API int sr_add_mode(int, int, double, int, sr_mode*);
MODULE_API int sr_switch_to_mode(int, int, double, int, sr_mode*);
MODULE_API int sr_flush();
MODULE_API int sr_set_mode(int);
MODULE_API void sr_set_monitor(const char*);
MODULE_API void sr_set_user_mode(int, int, int);
MODULE_API void sr_set_option(const char* key, const char* value);
MODULE_API void sr_get_state(sr_state *state);

/* Logging related functions */
MODULE_API void sr_set_log_level(int);
MODULE_API void sr_set_log_callback_error(void *);
MODULE_API void sr_set_log_callback_info(void *);
MODULE_API void sr_set_log_callback_debug(void *);

/* Others */
MODULE_API void sr_set_sdl_window(void *);

/* Inspired by https://stackoverflow.com/a/1067684 */
typedef struct MODULE_API
{
	void (*init)(void);
	void (*load_ini)(char*);
	char* (*sr_get_version)(void);
	void (*deinit)(void);
	int (*init_disp)(const char*, void*);
	void (*set_disp)(int);
	int (*get_mode)(int, sr_mode*);
	int (*add_mode)(int, int, double, int, sr_mode*);
	int (*switch_to_mode)(int, int, double, int, sr_mode*);
	int (*flush)(void);
	int (*set_mode)(int);
	void (*set_monitor)(const char*);
	void (*set_user_mode)(int, int, int);
	void (*set_option)(const char*, const char*);
	void (*get_state)(sr_state*);
	void (*set_log_level) (int);
	void (*set_log_callback_error)(void *);
	void (*set_log_callback_info)(void *);
	void (*set_log_callback_debug)(void *);
} srAPI;


#ifdef __cplusplus
}
#endif
