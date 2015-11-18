#ifndef WGL_SHADER_DLG_H
#define WGL_SHADER_DLG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool win32_shader_dlg_init(void);
void shader_dlg_show(HWND parent_hwnd);
void shader_dlg_params_reload(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // WGL_SHADER_DLG_H
