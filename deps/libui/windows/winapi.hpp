// 31 may 2015
#define UNICODE
#define _UNICODE
#define STRICT
#define STRICT_TYPED_ITEMIDS

// see https://github.com/golang/go/issues/9916#issuecomment-74812211
// TODO get rid of this
#define INITGUID

// for the manifest
#ifndef _UI_STATIC
#define ISOLATION_AWARE_ENABLED 1
#endif

// get Windows version right; right now Windows Vista
// unless otherwise stated, all values from Microsoft's sdkddkver.h
// TODO is all of this necessary? how is NTDDI_VERSION used?
// TODO plaform update sp2
#define WINVER			0x0600	/* from Microsoft's winnls.h */
#define _WIN32_WINNT		0x0600
#define _WIN32_WINDOWS	0x0600	/* from Microsoft's pdh.h */
#define _WIN32_IE			0x0700
#define NTDDI_VERSION		0x06000000

#include <windows.h>

// Microsoft's resource compiler will segfault if we feed it headers it was not designed to handle
#ifndef RC_INVOKED
#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <usp10.h>

#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <inttypes.h>

#include <vector>
#include <map>
#include <sstream>
#endif
