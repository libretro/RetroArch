/* Minimal D3DKMT stub for the mingw audit build: only the three
 * structs win32_common.h names. Layout fidelity is irrelevant here -
 * the audit reads call edges from objdump, never runs the binary. */
#ifndef D3DKMTHK_STUB_H
#define D3DKMTHK_STUB_H
typedef UINT D3DKMT_HANDLE;
typedef struct _D3DKMT_OPENADAPTERFROMHDC {
   HDC hDc; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;
typedef struct _D3DKMT_GETSCANLINE {
   D3DKMT_HANDLE hAdapter; UINT VidPnSourceId; BOOL InVerticalBlank;
   UINT ScanLine; UINT Reserved[8];
} D3DKMT_GETSCANLINE;
typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
   D3DKMT_HANDLE hAdapter; D3DKMT_HANDLE hDevice; UINT VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;
#endif
