#ifndef RA_DXGI_BRIDGE_H
#define RA_DXGI_BRIDGE_H
#include <windows.h>
#include <stdint.h>
typedef struct ra_dxgi_bridge ra_dxgi_bridge_t;
ra_dxgi_bridge_t *ra_dxgi_bridge_create(HWND window, unsigned width,
      unsigned height, unsigned format, const unsigned char luid[8]);
int ra_dxgi_bridge_resize(ra_dxgi_bridge_t *bridge,
      unsigned width, unsigned height, unsigned format);
void ra_dxgi_bridge_destroy(ra_dxgi_bridge_t *bridge);
HANDLE ra_dxgi_bridge_resource(ra_dxgi_bridge_t *bridge, unsigned index);
HANDLE ra_dxgi_bridge_ready_fence(ra_dxgi_bridge_t *bridge);
HANDLE ra_dxgi_bridge_done_fence(ra_dxgi_bridge_t *bridge);
int ra_dxgi_bridge_presentable(ra_dxgi_bridge_t *bridge);
int ra_dxgi_bridge_failed(ra_dxgi_bridge_t *bridge);
void ra_dxgi_bridge_hdr_metadata(ra_dxgi_bridge_t *bridge, float peak);
unsigned ra_dxgi_bridge_index(ra_dxgi_bridge_t *bridge, unsigned interval);
int64_t ra_dxgi_bridge_present_time(ra_dxgi_bridge_t *bridge);
/* The ID3D12Device the bridge presents with, for NVAPI */
void *ra_dxgi_bridge_device(ra_dxgi_bridge_t *bridge);
uint64_t ra_dxgi_bridge_last_use(ra_dxgi_bridge_t *bridge, unsigned index);
int ra_dxgi_bridge_present(ra_dxgi_bridge_t *bridge, uint64_t ready,
      unsigned interval);
#endif
