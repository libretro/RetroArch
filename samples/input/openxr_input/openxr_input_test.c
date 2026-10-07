/* The OpenXR input file (input/drivers/openxr_input.c), as it ships,
 * against a runtime the test stands in for: the xr calls it makes and
 * the clock are the test's. No headset can be scripted, and what is
 * held to here is what it does when the runtime has nothing to give.
 *
 * - Held controls are read as the runtime reports them: buttons, a
 *   stick's two axes, the triggers.
 * - A control whose state cannot be had reads as not held - a stick
 *   and a trigger kept their last value.
 * - A sync that fails, or the session without the focus (the headset's
 *   own menu up), is nothing held: buttons, axes and the held menu
 *   button - they all stayed as they were.
 * - The focus back, what is held reads again.
 * - A session that is not running is nothing held.
 * - After deinit nothing is held - the last buttons and axes stayed
 *   readable.
 * - The menu button held a second reads as held on every read until it
 *   comes up, and a failed sync lets it go. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* what the file includes, before ANDROID is defined for its own sake */
#include <boolean.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <features/features_cpu.h>
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../../../input/drivers/openxr_input.h"

/* ---- the runtime ---- */
static XrResult t_sync_result     = XR_SUCCESS;
static bool     t_fail_floats;
static bool     t_fail_vectors;
static bool     t_fail_bools;
static bool     t_inactive;
static bool     t_down[32];        /* by action, in the order they are made */
static float    t_trigger[2];
static float    t_stick[2][2];
static unsigned t_actions_made;
static uint64_t t_clock;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(XrInstance i, const XrActionSetCreateInfo *c, XrActionSet *s)
{ *s = (XrActionSet)(uintptr_t)0x100; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet s, const XrActionCreateInfo *c, XrAction *a)
{ *a = (XrAction)(uintptr_t)(++t_actions_made); return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(XrInstance i, const char *s, XrPath *p)
{ *p = 1; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance i, const XrInteractionProfileSuggestedBinding *b)
{ return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession s, const XrSessionActionSetsAttachInfo *i)
{ return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(XrActionSet s) { return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession s, const XrActionsSyncInfo *i)
{ return t_sync_result; }
/* an action's place in the order they were made, from its handle */
#define T_ACTION(info) ((unsigned)(uintptr_t)(info)->action - 1)
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession s, const XrActionStateGetInfo *g, XrActionStateBoolean *st)
{
   if (t_fail_bools)
      return XR_ERROR_RUNTIME_FAILURE;
   st->isActive     = t_inactive ? XR_FALSE : XR_TRUE;
   st->currentState = t_down[T_ACTION(g)] ? XR_TRUE : XR_FALSE;
   return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession s, const XrActionStateGetInfo *g, XrActionStateFloat *st)
{
   if (t_fail_floats)
      return XR_ERROR_RUNTIME_FAILURE;
   st->isActive     = t_inactive ? XR_FALSE : XR_TRUE;
   st->currentState = t_trigger[T_ACTION(g) - 8];   /* the triggers are the 9th and 10th */
   return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession s, const XrActionStateGetInfo *g, XrActionStateVector2f *st)
{
   if (t_fail_vectors)
      return XR_ERROR_RUNTIME_FAILURE;
   st->isActive       = t_inactive ? XR_FALSE : XR_TRUE;
   st->currentState.x = t_stick[T_ACTION(g) - 10][0]; /* the sticks the 11th and 12th */
   st->currentState.y = t_stick[T_ACTION(g) - 10][1];
   return XR_SUCCESS;
}

/* ---- the rest of what it calls ---- */
retro_time_t cpu_features_get_time_usec(void) { return (retro_time_t)t_clock; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid) { return true; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* the file itself, with the readers the Android build has */
#define ANDROID 1
#include "../../../input/drivers/openxr_input.c"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

#define SESSION ((XrSession)(uintptr_t)0x200)
/* the actions, in the order the file makes them */
enum { T_X, T_Y, T_A, T_B, T_LGRIP, T_RGRIP, T_LCLICK, T_RCLICK,
       T_LTRIGGER, T_RTRIGGER, T_LSTICK, T_RSTICK, T_MENU, T_SYSTEM };

static void hold_some(void)
{
   memset(t_down, 0, sizeof(t_down));
   t_down[T_A]    = true;
   t_down[T_MENU] = true;
   t_stick[0][0]  = 0.75f;  t_stick[0][1] = -0.5f;
   t_stick[1][0]  = -1.0f;  t_stick[1][1] = 0.25f;
   t_trigger[0]   = 0.0f;   t_trigger[1]  = 0.5f;
}

static bool nothing_held(void)
{
   unsigned a;
   static const unsigned keys[] = { AKEYCODE_BUTTON_X, AKEYCODE_BUTTON_Y,
      AKEYCODE_BUTTON_A, AKEYCODE_BUTTON_B, AKEYCODE_BUTTON_L1,
      AKEYCODE_BUTTON_R1, AKEYCODE_BUTTON_THUMBL, AKEYCODE_BUTTON_THUMBR,
      AKEYCODE_BACK };
   for (a = 0; a < sizeof(keys) / sizeof(keys[0]); a++)
      if (openxr_input_button(keys[a]))
         return false;
   for (a = 0; a < 8; a++)
      if (openxr_input_axis(a))
         return false;
   return !openxr_input_menu_long_press();
}

int main(void)
{
   unsigned i;

   CHECK(nothing_held(), "something is held before there is a session");
   CHECK(openxr_input_init((XrInstance)(uintptr_t)0x1), "init failed against the stand-in");
   CHECK(t_actions_made == 14, "%u actions made, the stand-in goes by 14", t_actions_made);
   CHECK(openxr_input_attach(SESSION) && openxr_input_session_active(), "the session did not attach");

   /* held controls, as the runtime reports them */
   hold_some();
   t_clock = 1000000;
   openxr_input_sync(SESSION);
   CHECK(openxr_input_button(AKEYCODE_BUTTON_A) && openxr_input_button(AKEYCODE_BACK),
         "A and menu held do not read held");
   CHECK(!openxr_input_button(AKEYCODE_BUTTON_B) && !openxr_input_button(AKEYCODE_BUTTON_X),
         "a button that is not held reads held");
   CHECK(openxr_input_axis(0) == 24575 && openxr_input_axis(1) == -16383,
         "the left stick at 0.75,-0.5 reads %d,%d", openxr_input_axis(0), openxr_input_axis(1));
   CHECK(openxr_input_axis(2) == -32767 && openxr_input_axis(3) == 8191,
         "the right stick at -1,0.25 reads %d,%d", openxr_input_axis(2), openxr_input_axis(3));
   CHECK(openxr_input_axis(6) == 0 && openxr_input_axis(7) == 16383,
         "the triggers at 0,0.5 read %d,%d", openxr_input_axis(6), openxr_input_axis(7));
   CHECK(openxr_input_axis(4) == 0 && openxr_input_axis(5) == 0 && openxr_input_axis(8) == 0,
         "an axis there is none of reads");

   /* a stick and a trigger whose state cannot be had */
   t_fail_floats = t_fail_vectors = true;
   openxr_input_sync(SESSION);
   CHECK(openxr_input_axis(0) == 0 && openxr_input_axis(1) == 0 && openxr_input_axis(2) == 0
         && openxr_input_axis(7) == 0,
         "with the stick and trigger queries failing they read %d,%d and %d",
         openxr_input_axis(0), openxr_input_axis(1), openxr_input_axis(7));
   CHECK(openxr_input_button(AKEYCODE_BUTTON_A), "... and a button that can be had does not read");
   t_fail_floats = t_fail_vectors = false;
   t_fail_bools  = true;
   openxr_input_sync(SESSION);
   CHECK(!openxr_input_button(AKEYCODE_BUTTON_A) && openxr_input_axis(0) == 24575,
         "with the button queries failing: A %d, the stick %d", openxr_input_button(AKEYCODE_BUTTON_A), openxr_input_axis(0));
   t_fail_bools  = false;

   /* controls the runtime says are not active */
   t_inactive = true;
   openxr_input_sync(SESSION);
   CHECK(nothing_held(), "controls that are not active read held");
   t_inactive = false;

   /* the menu button held a second: held on every read, until it is up */
   t_clock = 10000000;
   openxr_input_sync(SESSION);
   CHECK(!openxr_input_menu_long_press(), "the menu button reads long-held at once");
   t_clock += 999000;
   openxr_input_sync(SESSION);
   CHECK(!openxr_input_menu_long_press(), "the menu button reads long-held before the second is up");
   t_clock += 2000;
   openxr_input_sync(SESSION);
   for (i = 0; i < 3; i++)
      CHECK(openxr_input_menu_long_press(), "the menu button held a second does not read so, read %u", i + 1);

   /* a sync that fails, and the session without the focus: nothing held */
   t_sync_result = XR_ERROR_RUNTIME_FAILURE;
   openxr_input_sync(SESSION);
   CHECK(nothing_held(), "after a sync that failed something is still held");
   t_sync_result = XR_SUCCESS;
   t_clock      += 16000;
   openxr_input_sync(SESSION);
   CHECK(openxr_input_button(AKEYCODE_BUTTON_A) && openxr_input_axis(0) == 24575,
         "with the sync working again what is held does not read");
   CHECK(!openxr_input_menu_long_press(), "the menu button reads long-held straight after a failed sync");
   t_sync_result = XR_SESSION_NOT_FOCUSED;
   openxr_input_sync(SESSION);
   CHECK(nothing_held(), "with the session out of focus something is still held");
   t_sync_result = XR_SUCCESS;
   openxr_input_sync(SESSION);
   CHECK(openxr_input_button(AKEYCODE_BUTTON_A) && openxr_input_axis(7) == 16383,
         "with the focus back what is held does not read");

   /* the session not running */
   openxr_input_idle();
   CHECK(nothing_held(), "with the session not running something is still held");
   openxr_input_sync(SESSION);
   CHECK(openxr_input_button(AKEYCODE_BUTTON_A), "running again, what is held does not read");

   /* gone */
   openxr_input_deinit();
   CHECK(!openxr_input_session_active(), "the session is active after deinit");
   CHECK(nothing_held(), "after deinit something is still held");
   openxr_input_sync(SESSION);
   CHECK(nothing_held(), "a sync with no session attached made something held");

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS openxr_input_test\n");
   return 0;
}
