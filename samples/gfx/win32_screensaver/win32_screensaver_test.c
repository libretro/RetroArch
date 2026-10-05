/* win32_suspend_screensaver() against a kernel32 that keeps count the
 * way Windows does.
 *
 * Every video driver init asks for the screensaver to be held off, so
 * the function is called with true many times per session, and once
 * with false when the setting is turned off.
 *
 *   power         Windows 7 and later.  Every PowerSetRequest on a
 *                 request is counted and needs its own
 *                 PowerClearRequest, so repeated asks must not stack,
 *                 only one request may be created, and turning the
 *                 setting off must leave the display free to sleep.
 *   thread_state  98 to Vista.  SetThreadExecutionState holds the
 *                 display only with ES_CONTINUOUS; without it the call
 *                 resets the idle timer once and holds nothing.
 *   none          95 and NT 4: nothing to call, nothing to fail. */

#include <stdio.h>
#include <string.h>

#include "fake_kernel32.h"

#define ES_SYSTEM_REQUIRED  0x00000001UL
#define ES_DISPLAY_REQUIRED 0x00000002UL
#define ES_CONTINUOUS       0x80000000UL

enum scenario
{
   SCENARIO_POWER = 0,
   SCENARIO_THREAD_STATE,
   SCENARIO_NONE
};

static enum scenario scenario;
static int  kernel32_token;
static int  request_token;
static int  creates;
static int  request_count;
static DWORD thread_state;
static int  failures;

static HANDLE WINAPI fake_power_create_request(void *context)
{
   (void)context;
   creates++;
   return &request_token;
}

static BOOL WINAPI fake_power_set_request(HANDLE request, int type)
{
   if (request != &request_token || type != 0)
      return 0;
   request_count++;
   return 1;
}

static BOOL WINAPI fake_power_clear_request(HANDLE request, int type)
{
   if (request != &request_token || type != 0 || !request_count)
      return 0;
   request_count--;
   return 1;
}

static DWORD WINAPI fake_set_thread_execution_state(DWORD flags)
{
   DWORD previous = thread_state;
   if (flags & ES_CONTINUOUS)
      thread_state = flags;
   return previous | ES_CONTINUOUS;
}

HMODULE GetModuleHandleA(const char *name)
{
   return strcmp(name, "kernel32.dll") ? NULL : &kernel32_token;
}

FARPROC GetProcAddress(HMODULE module, const char *name)
{
   if (module != &kernel32_token)
      return NULL;
   if (scenario == SCENARIO_POWER)
   {
      if (!strcmp(name, "PowerCreateRequest"))
         return (FARPROC)fake_power_create_request;
      if (!strcmp(name, "PowerSetRequest"))
         return (FARPROC)fake_power_set_request;
      if (!strcmp(name, "PowerClearRequest"))
         return (FARPROC)fake_power_clear_request;
   }
   if (scenario != SCENARIO_NONE && !strcmp(name, "SetThreadExecutionState"))
      return (FARPROC)fake_set_thread_execution_state;
   return NULL;
}

static void expect(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

static bool display_held(void)
{
   if (scenario == SCENARIO_POWER)
      return request_count > 0;
   return (thread_state & (ES_CONTINUOUS | ES_DISPLAY_REQUIRED))
      == (ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
}

int main(int argc, char **argv)
{
   int i;

   if (argc < 2)
      return 2;
   if (!strcmp(argv[1], "power"))
      scenario = SCENARIO_POWER;
   else if (!strcmp(argv[1], "thread_state"))
      scenario = SCENARIO_THREAD_STATE;
   else if (!strcmp(argv[1], "none"))
      scenario = SCENARIO_NONE;
   else
      return 2;

   /* Three video driver inits with the setting on. */
   for (i = 0; i < 3; i++)
      expect(win32_suspend_screensaver(NULL, true), "enable reports success");

   if (scenario != SCENARIO_NONE)
      expect(display_held(), "the display is held after enabling");
   if (scenario == SCENARIO_POWER)
   {
      expect(creates == 1, "one power request for the process");
      expect(request_count == 1, "repeated enables do not stack");
   }

   /* The setting turned off. */
   expect(win32_suspend_screensaver(NULL, false), "disable reports success");
   if (scenario != SCENARIO_NONE)
      expect(!display_held(), "the display is free after disabling");

   /* Off twice, then on again. */
   expect(win32_suspend_screensaver(NULL, false), "a second disable succeeds");
   expect(win32_suspend_screensaver(NULL, true), "re-enable reports success");
   if (scenario != SCENARIO_NONE)
      expect(display_held(), "the display is held again after re-enabling");
   if (scenario == SCENARIO_POWER)
   {
      expect(creates == 1, "the request is reused, not recreated");
      expect(request_count == 1, "the request is set exactly once");
   }

   printf("%s: %s\n", argv[1], failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
