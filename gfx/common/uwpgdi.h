#ifndef UWPGDI_H_
#define UWPGDI_H_


#include <windows.h>

#include <GL/gl.h>

#if !defined(_GDI32_)
#define WINGDIAPI_UWP __declspec(dllimport)
#else
#define WINGDIAPI_UWP __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Undeclared APIs exported by libgallium on UWP
 */

WINGDIAPI_UWP HGLRC WINAPI wglCreateContext(HDC);
WINGDIAPI_UWP BOOL  WINAPI wglDeleteContext(HGLRC);
WINGDIAPI_UWP BOOL  WINAPI wglMakeCurrent(HDC, HGLRC);
WINGDIAPI_UWP BOOL APIENTRY wglSwapBuffers(HDC hdc);
WINGDIAPI_UWP PROC APIENTRY wglGetProcAddress(LPCSTR lpszProc);
WINGDIAPI_UWP BOOL APIENTRY wglShareLists(
   HGLRC unnamedParam1,
   HGLRC unnamedParam2
);

#ifdef __cplusplus
}
#endif

#endif UWPGDI_H_
