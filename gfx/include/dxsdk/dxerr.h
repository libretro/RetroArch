/*==========================================================================;
 *
 *
 *  File:   dxerr.h
 *  Content:    DirectX Error Library Include File
 *
 ****************************************************************************/

#ifndef _DXERR_H_
#define _DXERR_H_

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

//
//  DXGetErrorString
//
//  Desc:  Converts a DirectX HRESULT to a string
//
//  Args:  HRESULT hr   Can be any error code from
//                      XACT XAUDIO2 XAPO XINPUT DXGI D3D10 D3DX10 D3D9 D3DX9 DDRAW DSOUND DINPUT DSHOW
//
//  Return: Converted string
//
const char*  WINAPI DXGetErrorStringA(__in HRESULT hr);
const WCHAR* WINAPI DXGetErrorStringW(__in HRESULT hr);

#ifdef UNICODE
#define DXGetErrorString DXGetErrorStringW
#else
#define DXGetErrorString DXGetErrorStringA
#endif

//
//  DXGetErrorDescription
//
//  Desc:  Returns a string description of a DirectX HRESULT
//
//  Args:  HRESULT hr   Can be any error code from
//                      XACT XAUDIO2 XAPO XINPUT DXGI D3D10 D3DX10 D3D9 D3DX9 DDRAW DSOUND DINPUT DSHOW
//
//  Return: String description
//
const char*  WINAPI DXGetErrorDescriptionA(__in HRESULT hr);
const WCHAR* WINAPI DXGetErrorDescriptionW(__in HRESULT hr);

#ifdef UNICODE
    #define DXGetErrorDescription DXGetErrorDescriptionW
#else
    #define DXGetErrorDescription DXGetErrorDescriptionA
#endif

//
//  DXTrace
//
//  Desc:  Outputs a formatted error message to the debug stream
//
//  Args:  CHAR* strFile   The current file, typically passed in using the
//                         __FILE__ macro.
//         DWORD dwLine    The current line number, typically passed in using the
//                         __LINE__ macro.
//         HRESULT hr      An HRESULT that will be traced to the debug stream.
//         CHAR* strMsg    A string that will be traced to the debug stream (may be NULL)
//         BOOL bPopMsgBox If TRUE, then a message box will popup also containing the passed info.
//
//  Return: The hr that was passed in.
//
HRESULT WINAPI DXTraceA( __in_z const char* strFile, __in DWORD dwLine, __in HRESULT hr, __in_z_opt const char* strMsg, __in BOOL bPopMsgBox );
HRESULT WINAPI DXTraceW( __in_z const char* strFile, __in DWORD dwLine, __in HRESULT hr, __in_z_opt const WCHAR* strMsg, __in BOOL bPopMsgBox );

#ifdef UNICODE
#define DXTrace DXTraceW
#else
#define DXTrace DXTraceA
#endif

//
// Helper macros
//
#if defined(DEBUG) | defined(_DEBUG)
#define DXTRACE_MSG(str)              DXTrace( __FILE__, (DWORD)__LINE__, 0, str, FALSE )
#define DXTRACE_ERR(str,hr)           DXTrace( __FILE__, (DWORD)__LINE__, hr, str, FALSE )
#define DXTRACE_ERR_MSGBOX(str,hr)    DXTrace( __FILE__, (DWORD)__LINE__, hr, str, TRUE )
#else
#define DXTRACE_MSG(str)              (0L)
#define DXTRACE_ERR(str,hr)           (hr)
#define DXTRACE_ERR_MSGBOX(str,hr)    (hr)
#endif

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // _DXERR_H_
