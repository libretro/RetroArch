/**************************************************************

   switchres_wrapper.h - Switchres C wrapper API header file

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

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

/* That's all the exposed data from Switchres calculation */
typedef struct MODULE_API {
	int width;
	int height;
	double refresh;
	unsigned char is_refresh_off;
	unsigned char is_stretched;
	int x_scale;
	int y_scale;
	unsigned char interlace;
} sr_mode;


/* Declaration of the wrapper functions */
MODULE_API void sr_init();
MODULE_API void sr_load_ini(char* config);
MODULE_API void sr_deinit();
MODULE_API unsigned char sr_init_disp(const char* src);
MODULE_API unsigned char sr_add_mode(int, int, double, unsigned char, sr_mode*);
MODULE_API unsigned char sr_switch_to_mode(int, int, double, unsigned char, sr_mode*);
MODULE_API void sr_set_monitor(const char*);
MODULE_API void sr_set_rotation(unsigned char);
MODULE_API void sr_set_user_mode(int, int, int);

/* Logging related functions */
MODULE_API void sr_set_log_level (int);
MODULE_API void sr_set_log_callback_error(void *);
MODULE_API void sr_set_log_callback_info(void *);
MODULE_API void sr_set_log_callback_debug(void *);


/* Inspired by https://stackoverflow.com/a/1067684 */
typedef struct MODULE_API {
    void (*init)(void);
    void (*sr_sr_load_ini)(char*);
    void (*deinit)(void);
    unsigned char (*sr_init_disp)(const char*);
    unsigned char (*sr_add_mode)(int, int, double, unsigned char, sr_mode*);
    unsigned char (*sr_switch_to_mode)(int, int, double, unsigned char, sr_mode*);
    void (*sr_set_monitor)(const char*);
    void (*sr_set_rotation)(unsigned char);
    void (*sr_set_user_mode)(int, int, int);
    void (*sr_set_log_level) (int);
    void (*sr_set_log_callback_error)(void *);
    void (*sr_set_log_callback_info)(void *);
    void (*sr_set_log_callback_debug)(void *);
} srAPI;


#ifdef __cplusplus
}
#endif
