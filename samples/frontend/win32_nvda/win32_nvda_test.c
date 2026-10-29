/* frontend/drivers/nvda_controller_win32.c against a stand-in NVDA:
 * an RPC server in this process, on the endpoint NVDA listens on, for
 * the controller interface as widl compiles nvdaController.idl - an
 * NDR implementation of the interface other than the client's, whose
 * format strings are MIDL's. Held to, for x86 and x64 alike:
 *
 * - with no NVDA listening, each call returns an RPC status, nonzero,
 *   and the process goes on;
 * - with it listening, each of the four procedures reaches it, in the
 *   order made, the strings as sent - non-ASCII, empty and long ones
 *   among them - and each returns 0;
 * - a NULL string is refused without a call; after
 *   nvda_controller_deinit() nothing is sent.
 *
 * Given a path, the same calls are first made through that controller
 * client DLL - NVDA's own - so the stand-in is shown to be the server
 * that client talks to. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../../../frontend/drivers/nvda_controller_win32.c"

static unsigned failures;

#define CHECK(c, what) do { if (!(c)) { \
   printf("FAIL: %s (%s:%d)\n", what, __FILE__, __LINE__); \
   failures++; } else printf("   ok   %s\n", what); } while (0)

/* ---- the stand-in NVDA ---- */

static char     calls[64];
static unsigned ncalls;
static wchar_t *last_text;

static void got(char c, const wchar_t *text)
{
   if (ncalls < sizeof(calls) - 1)
      calls[ncalls++] = c;
   calls[ncalls] = 0;
   free(last_text);
   last_text = NULL;
   if (text)
   {
      size_t n  = wcslen(text) + 1;
      last_text = (wchar_t*)malloc(n * sizeof(wchar_t));
      memcpy(last_text, text, n * sizeof(wchar_t));
   }
}

static error_status_t __stdcall s_test_if_running(void)
{ got('T', NULL); return 0; }
static error_status_t __stdcall s_speak_text(const wchar_t *text)
{ got('S', text); return 0; }
static error_status_t __stdcall s_cancel_speech(void)
{ got('C', NULL); return 0; }
static error_status_t __stdcall s_braille_message(const wchar_t *message)
{ got('B', message); return 0; }

/* The procedures as widl generates them for nvdaController.idl */
#ifdef NVDA_CONTROLLER_X64
static const unsigned char s_proc_format[] = {
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0a, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0a, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x0a, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x10, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x0a, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x08, 0x00,
   0x70, 0x00, 0x08, 0x00, 0x10, 0x00,
   0x00
};
static const unsigned short s_proc_offset[4] = { 0, 32, 70, 102 };
#else
static const unsigned char s_proc_format[] = {
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x08, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x08, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x04, 0x00,
   0x70, 0x00, 0x04, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x44, 0x01, 0x08, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x70, 0x00, 0x00, 0x00, 0x10, 0x00,
   0x32, 0x48, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x08, 0x00,
   0x00, 0x00, 0x08, 0x00, 0x46, 0x02, 0x08, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00,
   0x0b, 0x01, 0x00, 0x00, 0x08, 0x00,
   0x70, 0x00, 0x04, 0x00, 0x10, 0x00,
   0x00
};
static const unsigned short s_proc_offset[4] = { 0, 30, 66, 96 };
#endif

static const unsigned char s_type_format[] = {
   0x00, 0x00,
   0x11, 0x08, 0x25, 0x5c,
   0x11, 0x08, 0x25, 0x5c,
   0x00
};

static RPC_DISPATCH_FUNCTION s_dispatch[] = {
   NdrServerCall2, NdrServerCall2, NdrServerCall2, NdrServerCall2, 0
};
static RPC_DISPATCH_TABLE s_dispatch_table = { 4, s_dispatch, 0 };

static const SERVER_ROUTINE s_routines[] = {
   (SERVER_ROUTINE)s_test_if_running,
   (SERVER_ROUTINE)s_speak_text,
   (SERVER_ROUTINE)s_cancel_speech,
   (SERVER_ROUTINE)s_braille_message
};

static const MIDL_SERVER_INFO s_server_info;

static const RPC_SERVER_INTERFACE s_interface = {
   sizeof(RPC_SERVER_INTERFACE),
   { { 0xdff50b99, 0xf7fd, 0x4ca7,
        { 0xa8, 0x2c, 0xda, 0xeb, 0x3e, 0x02, 0x52, 0x95 } }, { 1, 0 } },
   { { 0x8a885d04, 0x1ceb, 0x11c9,
        { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } }, { 2, 0 } },
   &s_dispatch_table, 0, 0, 0, &s_server_info, 0
};

static const MIDL_STUB_DESC s_stub_desc = {
   (void*)&s_interface, nvda_rpc_alloc, nvda_rpc_free, { 0 },
   0, 0, 0, 0, s_type_format, 1, 0x50002, 0, 0x50200ca, 0, 0, 0, 1,
   0, 0, 0
};

static const MIDL_SERVER_INFO s_server_info = {
   &s_stub_desc, s_routines, s_proc_format, s_proc_offset, 0, 0, 0, 0
};

static wchar_t s_endpoint[128];

static int stand_in_start(void)
{
   RPC_STATUS st;
   /* The client's own binding string, less "ncalrpc:[" and "]" */
   wchar_t b[128];
   size_t  n;
   if (!nvda_binding_string(b, sizeof(b) / sizeof(b[0])))
      return 0;
   n = wcslen(b);
   if (n < 11)
      return 0;
   memcpy(s_endpoint, b + 9, (n - 10) * sizeof(wchar_t));
   s_endpoint[n - 10] = 0;
   st = RpcServerUseProtseqEpW((RPC_WSTR)L"ncalrpc",
         RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)s_endpoint, NULL);
   if (st != RPC_S_OK)
      return 0;
   if (RpcServerRegisterIf((RPC_IF_HANDLE)&s_interface, NULL, NULL)
         != RPC_S_OK)
      return 0;
   return RpcServerListen(1, 20, TRUE) == RPC_S_OK;
}

static void stand_in_stop(void)
{
   RpcMgmtStopServerListening(NULL);
   RpcMgmtWaitServerListen();
   RpcServerUnregisterIf((RPC_IF_HANDLE)&s_interface, NULL, TRUE);
}

/* Wine's NdrMapCommAndFaultStatus writes the comm status, then a zero
 * fault status - and where both are the return value, as NVDA's are,
 * the zero is what is returned. On Windows the comm status is. Under
 * Wine a call that cannot reach NVDA is held to returning at all. */
static int under_wine;

static int failed_call(unsigned long st)
{
   return st != 0 || under_wine;
}

/* ---- the calls, through this client or another ---- */

struct client
{
   unsigned long (*test_if_running)(void);
   unsigned long (*cancel_speech)(void);
   unsigned long (*speak_text)(const wchar_t *);
   unsigned long (*braille_message)(const wchar_t *);
   const char *name;
};

static void calls_reach(const struct client *c)
{
   static wchar_t long_text[2049];
   char msg[160];
   int  i;
   const wchar_t *unicode = L"h\x00e9llo w\x00f6rld \x2713 \xd83d\xde00";

   for (i = 0; i < 2048; i++)
      long_text[i] = (wchar_t)(L'a' + i % 26);
   long_text[2048] = 0;
   ncalls          = 0;
   calls[0]        = 0;

#define C(cond, what) do { snprintf(msg, sizeof(msg), "%s: %s", c->name, what); \
   CHECK(cond, msg); } while (0)
   C(c->test_if_running() == 0, "testIfRunning reaches NVDA");
   C(c->cancel_speech() == 0, "cancelSpeech reaches NVDA");
   C(c->speak_text(unicode) == 0 && last_text
         && !wcscmp(last_text, unicode), "speakText, non-ASCII, as sent");
   C(c->braille_message(L"braille") == 0 && last_text
         && !wcscmp(last_text, L"braille"), "brailleMessage as sent");
   C(c->speak_text(L"") == 0 && last_text && !last_text[0],
         "an empty string");
   C(c->speak_text(long_text) == 0 && last_text
         && !wcscmp(last_text, long_text), "2048 characters");
   C(!strcmp(calls, "TCSBSS"), "each call once, in order");
#undef C
}

static unsigned long ours_test(void)
{ return nvda_controller_test_if_running(); }
static unsigned long ours_cancel(void)
{ return nvda_controller_cancel_speech(); }
static unsigned long ours_speak(const wchar_t *t)
{ return nvda_controller_speak_text(t); }
static unsigned long ours_braille(const wchar_t *t)
{ return nvda_controller_braille_message(t); }

static unsigned long (__stdcall *dll_test)(void);
static unsigned long (__stdcall *dll_cancel)(void);
static unsigned long (__stdcall *dll_speak)(const wchar_t *);
static unsigned long (__stdcall *dll_braille)(const wchar_t *);
static unsigned long via_dll_test(void) { return dll_test(); }
static unsigned long via_dll_cancel(void) { return dll_cancel(); }
static unsigned long via_dll_speak(const wchar_t *t) { return dll_speak(t); }
static unsigned long via_dll_braille(const wchar_t *t)
{ return dll_braille(t); }

int main(int argc, char **argv)
{
   static const struct client ours = {
      ours_test, ours_cancel, ours_speak, ours_braille, "this client" };
   static const struct client theirs = {
      via_dll_test, via_dll_cancel, via_dll_speak, via_dll_braille,
      "the DLL" };

   under_wine = GetProcAddress(GetModuleHandleA("ntdll.dll"),
         "wine_get_version") != NULL;
   printf("%s%s\n", sizeof(void*) == 8 ? "x64" : "x86",
         under_wine ? ", under Wine" : "");

   CHECK(nvda_controller_test_if_running() != 0,
         "before init: nothing sent");
   CHECK(nvda_controller_init(), "bound to the NVDA endpoint");
   CHECK(nvda_controller_init(), "bound again: kept");

   /* No NVDA */
   CHECK(failed_call(nvda_controller_test_if_running()),
         "no NVDA: testIfRunning fails");
   CHECK(failed_call(nvda_controller_speak_text(L"x")),
         "no NVDA: speakText fails");
   CHECK(failed_call(nvda_controller_braille_message(L"x")),
         "no NVDA: brailleMessage fails");
   CHECK(failed_call(nvda_controller_cancel_speech()),
         "no NVDA: cancelSpeech fails");

   CHECK(stand_in_start(), "the stand-in NVDA listening");

   if (argc > 1)
   {
      HMODULE dll = LoadLibraryA(argv[1]);
      CHECK(dll != NULL, "the controller client DLL loaded");
      if (dll)
      {
         dll_test    = (unsigned long (__stdcall*)(void))(void (*)(void))
            GetProcAddress(dll, "nvdaController_testIfRunning");
         dll_cancel  = (unsigned long (__stdcall*)(void))(void (*)(void))
            GetProcAddress(dll, "nvdaController_cancelSpeech");
         dll_speak   = (unsigned long (__stdcall*)(const wchar_t*))
            (void (*)(void))GetProcAddress(dll, "nvdaController_speakText");
         dll_braille = (unsigned long (__stdcall*)(const wchar_t*))
            (void (*)(void))GetProcAddress(dll,
                  "nvdaController_brailleMessage");
         if (dll_test && dll_cancel && dll_speak && dll_braille)
            calls_reach(&theirs);
         else
            CHECK(0, "the DLL's four exports");
      }
   }

   calls_reach(&ours);

   ncalls = 0;
   CHECK(nvda_controller_speak_text(NULL) == RPC_X_NULL_REF_POINTER
         && ncalls == 0, "a NULL string refused, nothing sent");

   stand_in_stop();
   ncalls = 0;
   CHECK(failed_call(nvda_controller_test_if_running()) && ncalls == 0,
         "NVDA gone: testIfRunning fails again");

   nvda_controller_deinit();
   ncalls = 0;
   CHECK(nvda_controller_test_if_running() == RPC_S_INVALID_BINDING
         && nvda_controller_speak_text(L"x") == RPC_S_INVALID_BINDING
         && ncalls == 0, "after deinit: nothing sent");

   free(last_text);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("PASS win32_nvda_test\n");
   return 0;
}
