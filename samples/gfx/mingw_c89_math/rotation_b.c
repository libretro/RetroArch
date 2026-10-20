#include <windows.h>
#include <d3dkmthk.h>
unsigned rotation_b(unsigned v) { return (unsigned)D3DKMDT_VPPR_GET_OFFSET_ROTATION((D3DKMDT_VIDPN_PRESENT_PATH_ROTATION)v); }
