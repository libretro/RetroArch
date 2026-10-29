/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* NVDA's controller interface over local RPC, as NVDA's controller
 * client DLL speaks it.
 *
 * NVDA listens on ncalrpc:[NvdaCtlr.<session id>.<desktop name>] for
 * interface DFF50B99-F7FD-4CA7-A82C-DAEB3E025295 v1.0, whose four
 * procedures - testIfRunning, speakText, cancelSpeech, brailleMessage -
 * NVDA publishes in nvdaHelper/interfaces/nvdaController.idl. The
 * calls go through NdrClientCall2 with the format strings and stub
 * descriptor NVDA's own controller client carries for each of x86,
 * x64 and ARM64, byte for byte (its 2024.4 build). Every procedure
 * has comm and fault status, so a failed call - NVDA not running -
 * comes back as its return value, never as an exception.
 *
 * rpcrt4 and the session and desktop calls are resolved at run time:
 * where any is missing, as before Windows 2000, there is no NVDA to
 * reach and nvda_controller_init() says so. */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_NVDA

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>

#include "nvda_controller_win32.h"

#if defined(_M_ARM64) || defined(__aarch64__)
#define NVDA_CONTROLLER_ARM64
#elif defined(_M_X64) || defined(__x86_64__)
#define NVDA_CONTROLLER_X64
#elif defined(_M_IX86) || defined(__i386__)
#define NVDA_CONTROLLER_X86
#endif

#if defined(NVDA_CONTROLLER_X64) || defined(NVDA_CONTROLLER_X86) \
      || defined(NVDA_CONTROLLER_ARM64)

typedef CLIENT_CALL_RETURN (RPC_VAR_ENTRY *nvda_ndr_client_call2_t)(
      PMIDL_STUB_DESC, PFORMAT_STRING, ...);
typedef RPC_STATUS (RPC_ENTRY *nvda_rpc_binding_from_string_t)(
      unsigned short *, RPC_BINDING_HANDLE *);
typedef RPC_STATUS (RPC_ENTRY *nvda_rpc_binding_free_t)(RPC_BINDING_HANDLE *);
typedef BOOL (WINAPI *nvda_process_id_to_session_id_t)(DWORD, DWORD *);
typedef HDESK (WINAPI *nvda_get_thread_desktop_t)(DWORD);
typedef BOOL (WINAPI *nvda_get_user_object_information_w_t)(HANDLE, int,
      PVOID, DWORD, LPDWORD);

/* Procedure format strings, one procedure each: the procedure header,
 * then its [in, string] wchar_t * where it has one, then its
 * error_status_t return. Indexed by NVDA_PROC_*. */
#define NVDA_PROC_TEST_IF_RUNNING 0
#define NVDA_PROC_SPEAK_TEXT      1
#define NVDA_PROC_CANCEL_SPEECH   2
#define NVDA_PROC_BRAILLE_MESSAGE 3

#if defined(NVDA_CONTROLLER_ARM64)
static const unsigned char nvda_proc_format[] = {
   /* 0: testIfRunning - implicit handle, comm/fault status, method 0,
    * stack 8, no arguments; the extension's 12 bytes, the last two the
    * register each argument comes in */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0c, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 34: speakText - method 1, stack 16, the string at 0, in x0 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0c, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x80,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   /* 74: cancelSpeech - method 2 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0c, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 108: brailleMessage - method 3 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0c, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x80,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   0x00
};
static const unsigned short nvda_proc_offset[4] = { 0, 34, 74, 108 };
#elif defined(NVDA_CONTROLLER_X64)
static const unsigned char nvda_proc_format[] = {
   /* 0: testIfRunning - implicit handle, comm/fault status, method 0,
    * stack 8, no arguments; the extension's 10 bytes */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0a, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 32: speakText - method 1, stack 16, the string at 0 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0a, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   /* 70: cancelSpeech - method 2 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0a, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 102: brailleMessage - method 3 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0a, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   0x00
};
static const unsigned short nvda_proc_offset[4] = { 0, 32, 70, 102 };
#else
static const unsigned char nvda_proc_format[] = {
   /* 0: testIfRunning - method 0, stack 4; the extension's 8 bytes */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x08, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 30: speakText - method 1, stack 8, the string at 0 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x08, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x04, 0x00, 0x10, 0x00,
   /* 66: cancelSpeech - method 2 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x08, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   /* 96: brailleMessage - method 3 */
   0x32, 0x68, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x08, 0x01, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x04, 0x00, 0x10, 0x00,
   0x00
};
static const unsigned short nvda_proc_offset[4] = { 0, 30, 66, 96 };
#endif

/* The one type: at 2 a reference pointer to, at 4, a conformant
 * wide string */
static const unsigned char nvda_type_format[] = {
   0x00, 0x00, 0x11, 0x08, 0x25, 0x5c, 0x00
};

/* Comm and fault status of each procedure in its return value */
static const COMM_FAULT_OFFSETS nvda_comm_fault_offsets[4] = {
   { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 }
};

static const RPC_CLIENT_INTERFACE nvda_client_interface = {
   sizeof(RPC_CLIENT_INTERFACE),
   /* DFF50B99-F7FD-4CA7-A82C-DAEB3E025295 v1.0 */
   { { 0xdff50b99, 0xf7fd, 0x4ca7,
        { 0xa8, 0x2c, 0xda, 0xeb, 0x3e, 0x02, 0x52, 0x95 } }, { 1, 0 } },
   /* NDR 2.0 */
   { { 0x8a885d04, 0x1ceb, 0x11c9,
        { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } }, { 2, 0 } },
   0, 0, 0, 0, 0, 0
};

static handle_t nvda_binding;

static void * __RPC_USER nvda_rpc_alloc(size_t len)
{
   return malloc(len);
}

static void __RPC_USER nvda_rpc_free(void *ptr)
{
   free(ptr);
}

static MIDL_STUB_DESC nvda_stub_desc;

static HMODULE                         nvda_rpcrt4;
static nvda_ndr_client_call2_t         nvda_ndr_client_call2;
static nvda_rpc_binding_free_t         nvda_rpc_binding_free;

static unsigned long nvda_call_void(unsigned proc)
{
   CLIENT_CALL_RETURN ret;
   if (!nvda_binding)
      return RPC_S_INVALID_BINDING;
   ret = nvda_ndr_client_call2(&nvda_stub_desc,
         (PFORMAT_STRING)&nvda_proc_format[nvda_proc_offset[proc]]);
   return (unsigned long)ret.Simple;
}

static unsigned long nvda_call_string(unsigned proc, const wchar_t *text)
{
   CLIENT_CALL_RETURN ret;
   if (!nvda_binding)
      return RPC_S_INVALID_BINDING;
   if (!text)
      return RPC_X_NULL_REF_POINTER;
#if defined(NVDA_CONTROLLER_X64) || defined(NVDA_CONTROLLER_ARM64)
   ret = nvda_ndr_client_call2(&nvda_stub_desc,
         (PFORMAT_STRING)&nvda_proc_format[nvda_proc_offset[proc]], text);
#else
   {
      /* x86: the interpreter copies the procedure's whole stack - the
       * string, then the return value's slot - from where it is told
       * the arguments start */
      const wchar_t *args[2];
      args[0] = text;
      args[1] = NULL;
      ret     = nvda_ndr_client_call2(&nvda_stub_desc,
            (PFORMAT_STRING)&nvda_proc_format[nvda_proc_offset[proc]],
            args);
   }
#endif
   return (unsigned long)ret.Simple;
}

/* @out of @len wide characters: ncalrpc:[NvdaCtlr.<session>.<desktop>] */
static bool nvda_binding_string(wchar_t *out, size_t len)
{
   static const wchar_t prefix[] = L"ncalrpc:[NvdaCtlr.";
   wchar_t desktop[64];
   wchar_t digits[16];
   DWORD   session = 0;
   size_t  n, i, d  = 0;
   HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
   /* Loaded already where there is a window, but not everywhere */
   HMODULE user32   = LoadLibraryA("user32.dll");
   nvda_process_id_to_session_id_t      session_of;
   nvda_get_thread_desktop_t            thread_desktop;
   nvda_get_user_object_information_w_t object_info;
   HDESK   desk;

   if (!kernel32 || !user32)
      return false;
   session_of     = (nvda_process_id_to_session_id_t)(void (*)(void))
      GetProcAddress(kernel32, "ProcessIdToSessionId");
   thread_desktop = (nvda_get_thread_desktop_t)(void (*)(void))
      GetProcAddress(user32, "GetThreadDesktop");
   object_info    = (nvda_get_user_object_information_w_t)(void (*)(void))
      GetProcAddress(user32, "GetUserObjectInformationW");
   if (!session_of || !thread_desktop || !object_info)
      return false;
   if (!session_of(GetCurrentProcessId(), &session))
      return false;
   if (!(desk = thread_desktop(GetCurrentThreadId())))
      return false;
   memset(desktop, 0, sizeof(desktop));
   /* UOI_NAME */
   if (!object_info(desk, 2, desktop, sizeof(desktop) - sizeof(wchar_t),
            NULL))
      return false;

   do
   {
      digits[d++] = (wchar_t)(L'0' + session % 10);
      session    /= 10;
   } while (session);

   n = 0;
   for (i = 0; prefix[i]; i++)
      if (n < len)
         out[n++] = prefix[i];
   while (d)
      if (n < len)
         out[n++] = digits[--d];
      else
         d--;
   if (n < len)
      out[n++] = L'.';
   for (i = 0; desktop[i]; i++)
      if (n < len)
         out[n++] = desktop[i];
   if (n + 2 > len)
      return false;
   out[n++] = L']';
   out[n]   = 0;
   return true;
}

bool nvda_controller_init(void)
{
   wchar_t binding[128];
   nvda_rpc_binding_from_string_t from_string;

   if (nvda_binding)
      return true;
   if (!nvda_binding_string(binding, sizeof(binding) / sizeof(binding[0])))
      return false;
   if (!nvda_rpcrt4 && !(nvda_rpcrt4 = LoadLibraryA("rpcrt4.dll")))
      return false;
   nvda_ndr_client_call2 = (nvda_ndr_client_call2_t)(void (*)(void))
      GetProcAddress(nvda_rpcrt4, "NdrClientCall2");
   nvda_rpc_binding_free = (nvda_rpc_binding_free_t)(void (*)(void))
      GetProcAddress(nvda_rpcrt4, "RpcBindingFree");
   from_string           = (nvda_rpc_binding_from_string_t)(void (*)(void))
      GetProcAddress(nvda_rpcrt4, "RpcBindingFromStringBindingW");
   if (     !nvda_ndr_client_call2
         || !nvda_rpc_binding_free
         || !from_string)
      return false;

   nvda_stub_desc.RpcInterfaceInformation = (void*)&nvda_client_interface;
   nvda_stub_desc.pfnAllocate             = nvda_rpc_alloc;
   nvda_stub_desc.pfnFree                 = nvda_rpc_free;
   nvda_stub_desc.IMPLICIT_HANDLE_INFO.pPrimitiveHandle = &nvda_binding;
   nvda_stub_desc.pFormatTypes            = nvda_type_format;
   nvda_stub_desc.fCheckBounds            = 1;
   nvda_stub_desc.Version                 = 0x50002;
   nvda_stub_desc.MIDLVersion             = 0x8010274;
   nvda_stub_desc.CommFaultOffsets        = nvda_comm_fault_offsets;
   nvda_stub_desc.mFlags                  = 1;

   if (from_string((unsigned short*)binding, &nvda_binding) != RPC_S_OK)
   {
      nvda_binding = NULL;
      return false;
   }
   return true;
}

void nvda_controller_deinit(void)
{
   if (nvda_binding)
      nvda_rpc_binding_free(&nvda_binding);
   nvda_binding = NULL;
}

unsigned long nvda_controller_test_if_running(void)
{
   return nvda_call_void(NVDA_PROC_TEST_IF_RUNNING);
}

unsigned long nvda_controller_cancel_speech(void)
{
   return nvda_call_void(NVDA_PROC_CANCEL_SPEECH);
}

unsigned long nvda_controller_speak_text(const wchar_t *text)
{
   return nvda_call_string(NVDA_PROC_SPEAK_TEXT, text);
}

unsigned long nvda_controller_braille_message(const wchar_t *message)
{
   return nvda_call_string(NVDA_PROC_BRAILLE_MESSAGE, message);
}

#else

/* No format strings for this architecture: no NVDA */
bool nvda_controller_init(void) { return false; }
void nvda_controller_deinit(void) { }
unsigned long nvda_controller_test_if_running(void) { return 1; }
unsigned long nvda_controller_cancel_speech(void) { return 1; }
unsigned long nvda_controller_speak_text(const wchar_t *text)
{ (void)text; return 1; }
unsigned long nvda_controller_braille_message(const wchar_t *message)
{ (void)message; return 1; }

#endif

#endif /* HAVE_NVDA */
