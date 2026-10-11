#ifndef RA_WGL_DXGI_H
#define RA_WGL_DXGI_H
#include "win_dxgi_present.h"
typedef struct ra_wgl_dxgi ra_wgl_dxgi_t;
ra_wgl_dxgi_t *ra_wgl_dxgi_create(HWND window, unsigned width,
      unsigned height, unsigned format, unsigned internal_format);
int ra_wgl_dxgi_resize(ra_wgl_dxgi_t *presenter, unsigned width, unsigned height,
      unsigned format, unsigned internal_format);
int ra_wgl_dxgi_present(ra_wgl_dxgi_t *presenter, unsigned interval);
int ra_wgl_dxgi_presentable(ra_wgl_dxgi_t *presenter);
void ra_wgl_dxgi_destroy(ra_wgl_dxgi_t *presenter);
#endif
