/* Audit-build fixup for mingw-w64's older d3d12.h: the unversioned
 * serialize PFN is absent there. Declared with unspecified args (C),
 * so the GetProcAddress cast and the call both compile; the audit
 * only reads call edges, never runs this. */
#ifndef D3D12_AUDIT_FIXUP_H
#define D3D12_AUDIT_FIXUP_H
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <windows.h>
typedef HRESULT (WINAPI *PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)();
#endif
#endif
