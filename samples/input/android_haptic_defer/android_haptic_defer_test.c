/* android_haptic_defer_test.c -- the deferred keypress-haptic path from
 * input/drivers/android_input.c, reproduced and beaten on.
 *
 * The protocol: the overlay raises a keypress haptic from
 * input_driver_poll(), which a core reaches from inside retro_run() -
 * on a libco stack for the cores that use one, where entering Java is
 * not safe. The callback therefore only records the request; the
 * runloop dispatches it on the OS stack at the top of the next
 * iteration, and the lifecycle handlers clear it. The claims a
 * regression would break:
 *
 *   1. The callback never enters Java: every dispatch happens with the
 *      core unwound.
 *   2. Repeats coalesce: any number of presses inside one iteration
 *      buzz once, and a delivered request never buzzes twice.
 *   3. A press is not carried across a break in the interaction. The
 *      runloop blocks in the poll while backgrounded, so a request
 *      left pending over a pause, a stop, a window teardown, a focus
 *      loss or a destroy would surface on the resume that follows.
 *   4. The delivery guards hold on their own, for the command that
 *      lands after the press in the same poll batch.
 *   5. None of it wedges: a press after a full background/resume cycle
 *      still buzzes.
 *
 * Two sabotage modes reproduce the bugs the lanes exist for and are
 * asserted to be caught: dispatching from the callback (the pre-defer
 * behaviour) and dropping the lifecycle clears.
 *
 * Self-contained: the protocol under test is reproduced here rather
 * than linked, because the driver only builds against the NDK. */

#include <stdio.h>

#include <boolean.h>

/* APP_CMD_*, as far as delivery cares. */
enum
{
   CMD_START = 0,
   CMD_RESUME,
   CMD_PAUSE,
   CMD_STOP,
   CMD_INIT_WINDOW,
   CMD_TERM_WINDOW,
   CMD_GAINED_FOCUS,
   CMD_LOST_FOCUS,
   CMD_DESTROY
};

/* Sabotage hooks: 0 = shipping behaviour. */
static int sab_dispatch_in_callback = 0;
static int sab_no_lifecycle_clear   = 0;

/* struct android_app, as far as delivery cares. */
static int  app_activity_state;
static int  app_window;
static int  app_unfocused;
static int  app_destroy_requested;
static int  app_have_method;

static bool pending;
static int  on_core_stack;

static int  buzzes;
static int  buzzes_from_core_stack;

static int  failures;

static void reset_state(void)
{
   app_activity_state     = CMD_RESUME;
   app_window             = 1;
   app_unfocused          = 0;
   app_destroy_requested  = 0;
   app_have_method        = 1;
   pending                = false;
   on_core_stack          = 0;
   buzzes                 = 0;
   buzzes_from_core_stack = 0;
}

/* CALL_VOID_METHOD_PARAM(..., doHapticFeedback, ...) */
static void jni_do_haptic_feedback(void)
{
   buzzes++;
   if (on_core_stack)
      buzzes_from_core_stack++;
}

/* android_input_flush_pending_haptics() */
static void flush_pending_haptics(void)
{
   if (!pending)
      return;

   pending = false;

   if (     app_destroy_requested
         || app_activity_state != CMD_RESUME
         || !app_window
         || app_unfocused
         || !app_have_method)
      return;

   jni_do_haptic_feedback();
}

/* android_input_keypress_vibrate() */
static void keypress_vibrate(void)
{
   if (sab_dispatch_in_callback)
   {
      /* What the driver did before the request was deferred. */
      jni_do_haptic_feedback();
      return;
   }

   pending = true;
}

/* The parts of android_input_poll_main_cmd() delivery depends on. */
static void poll_main_cmd(int cmd)
{
   switch (cmd)
   {
      case CMD_START:
      case CMD_RESUME:
      case CMD_PAUSE:
         app_activity_state = cmd;
         break;
      case CMD_STOP:
         app_activity_state = cmd;
         break;
      case CMD_INIT_WINDOW:
         app_window         = 1;
         break;
      case CMD_TERM_WINDOW:
         app_window         = 0;
         break;
      case CMD_GAINED_FOCUS:
         app_unfocused      = 0;
         break;
      case CMD_LOST_FOCUS:
         app_unfocused      = 1;
         break;
      case CMD_DESTROY:
         app_destroy_requested = 1;
         break;
      default:
         break;
   }

   if (sab_no_lifecycle_clear)
      return;

   switch (cmd)
   {
      case CMD_PAUSE:
      case CMD_STOP:
      case CMD_TERM_WINDOW:
      case CMD_LOST_FOCUS:
      case CMD_DESTROY:
         pending = false;
         break;
      default:
         break;
   }
}

/* One runloop_iterate(): the flush runs first, then the core, whose
 * poll drains the commands and raises the presses. */
static void iterate(const int *cmds, int n_cmds, int presses)
{
   int i;

   flush_pending_haptics();

   on_core_stack = 1;
   for (i = 0; i < n_cmds; i++)
      poll_main_cmd(cmds[i]);
   for (i = 0; i < presses; i++)
      keypress_vibrate();
   on_core_stack = 0;
}

static void expect(const char *what, int got, int want)
{
   if (got == want)
      return;
   printf("[fail] %s: got %d, expected %d\n", what, got, want);
   failures++;
}

/* Backgrounded: the poll blocks on the looper, so no iteration runs
 * between the commands and the resume. */
static void background_and_resume(void)
{
   poll_main_cmd(CMD_LOST_FOCUS);
   poll_main_cmd(CMD_PAUSE);
   poll_main_cmd(CMD_STOP);
   poll_main_cmd(CMD_TERM_WINDOW);
   poll_main_cmd(CMD_INIT_WINDOW);
   poll_main_cmd(CMD_START);
   poll_main_cmd(CMD_RESUME);
   poll_main_cmd(CMD_GAINED_FOCUS);
}

static void run_suite(void)
{
   int saved_no_clear = sab_no_lifecycle_clear;
   int cmds[1];

   /* 1. One press buzzes once, on the OS stack, on the next iteration. */
   reset_state();
   iterate(NULL, 0, 1);
   expect("press does not buzz inside the core", buzzes, 0);
   iterate(NULL, 0, 0);
   expect("press buzzes on the next iteration", buzzes, 1);
   expect("no buzz from a core stack", buzzes_from_core_stack, 0);
   iterate(NULL, 0, 0);
   expect("a delivered press does not buzz twice", buzzes, 1);

   /* 2. Repeats inside one iteration coalesce. */
   reset_state();
   iterate(NULL, 0, 5);
   iterate(NULL, 0, 0);
   expect("five presses in one iteration buzz once", buzzes, 1);

   /* 3. A press held across a background does not surface on resume. */
   reset_state();
   iterate(NULL, 0, 1);
   background_and_resume();
   iterate(NULL, 0, 0);
   expect("no buzz after a background/resume cycle", buzzes, 0);

   /* 4. Same, for a destroy. */
   reset_state();
   iterate(NULL, 0, 1);
   poll_main_cmd(CMD_DESTROY);
   iterate(NULL, 0, 0);
   expect("no buzz after a destroy", buzzes, 0);

   /* 5. A press raised after the command, in the same poll batch, is
    *    past the clear and reaches the delivery guards instead. Held
    *    with the clears out of the way so only the guards can pass it. */
   sab_no_lifecycle_clear = 1;

   reset_state();
   cmds[0] = CMD_PAUSE;
   iterate(cmds, 1, 1);
   iterate(NULL, 0, 0);
   expect("paused state suppresses delivery", buzzes, 0);

   reset_state();
   cmds[0] = CMD_LOST_FOCUS;
   iterate(cmds, 1, 1);
   iterate(NULL, 0, 0);
   expect("lost focus suppresses delivery", buzzes, 0);

   reset_state();
   cmds[0] = CMD_TERM_WINDOW;
   iterate(cmds, 1, 1);
   iterate(NULL, 0, 0);
   expect("a gone window suppresses delivery", buzzes, 0);

   reset_state();
   cmds[0] = CMD_DESTROY;
   iterate(cmds, 1, 1);
   iterate(NULL, 0, 0);
   expect("a pending destroy suppresses delivery", buzzes, 0);

   sab_no_lifecycle_clear = saved_no_clear;

   /* 6. An unresolved jmethodID drops the request rather than sticking. */
   reset_state();
   app_have_method = 0;
   iterate(NULL, 0, 1);
   iterate(NULL, 0, 0);
   expect("no method, no buzz", buzzes, 0);
   app_have_method = 1;
   iterate(NULL, 0, 0);
   expect("the dropped request does not resurface", buzzes, 0);

   /* 7. Nothing wedges: a press after a full cycle still buzzes. */
   reset_state();
   iterate(NULL, 0, 1);
   background_and_resume();
   iterate(NULL, 0, 1);
   iterate(NULL, 0, 0);
   expect("a press after a resume still buzzes", buzzes, 1);
   expect("still nothing from a core stack", buzzes_from_core_stack, 0);
}

int main(void)
{
   int sabotaged;

   run_suite();
   if (failures)
   {
      printf("[FAIL] android_haptic_defer_test: %d\n", failures);
      return 1;
   }
   printf("[pass] shipping behaviour\n");

   /* Negative tests: each sabotage must be caught by the suite above. */
   sab_dispatch_in_callback = 1;
   failures                 = 0;
   printf("--- expected failures (dispatch from the callback) ---\n");
   run_suite();
   sabotaged                = failures;
   sab_dispatch_in_callback = 0;
   if (!sabotaged)
   {
      printf("[FAIL] dispatching from the callback went undetected\n");
      return 1;
   }
   printf("[pass] dispatch from the callback caught (%d)\n", sabotaged);

   sab_no_lifecycle_clear = 1;
   failures               = 0;
   printf("--- expected failures (no lifecycle clears) ---\n");
   run_suite();
   sabotaged              = failures;
   sab_no_lifecycle_clear = 0;
   if (!sabotaged)
   {
      printf("[FAIL] dropping the lifecycle clears went undetected\n");
      return 1;
   }
   printf("[pass] missing lifecycle clears caught (%d)\n", sabotaged);

   printf("[PASS] android_haptic_defer_test\n");
   return 0;
}
