/* The XInput joypad driver writes rumble from a thread of its own.
 *
 * XInputSetState() returns when the controller's driver has taken the
 * strengths, and it was called from the frame loop. Now the frame
 * loop notes the strengths wanted and returns.
 *
 * The real driver is included here, cross-built with mingw-w64 and run
 * under Wine, and its XInputSetState is stood in for by one that can
 * be made slow and notes what it was given. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "input/drivers_joypad/xinput_joypad.c"

/* ---- what the driver links against --------------------------------- */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ (void)name; (void)display_name; (void)phys; (void)driver; (void)port; (void)vid; (void)pid; return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
dylib_t dylib_load(const char *path) { (void)path; return NULL; }
void dylib_close(dylib_t lib) { (void)lib; }
function_t dylib_proc(dylib_t lib, const char *proc) { (void)lib; (void)proc; return NULL; }

/* ---- XInputSetState, stood in for ----------------------------------- */

static LONG slow_ms;
static LONG calls;
static LONG last_user, last_left, last_right;

static uint32_t __stdcall fake_set_state(uint32_t user, XINPUT_VIBRATION *v)
{
   if (slow_ms)
      Sleep((DWORD)slow_ms);
   InterlockedExchange(&last_user,  (LONG)user);
   InterlockedExchange(&last_left,  (LONG)v->wLeftMotorSpeed);
   InterlockedExchange(&last_right, (LONG)v->wRightMotorSpeed);
   InterlockedIncrement(&calls);
   return ERROR_SUCCESS;
}
#define N(v) InterlockedCompareExchange(&(v), 0, 0)

/* ---- the test ------------------------------------------------------- */

static unsigned failures;
#define CHECK(cond, msg) do { \
   if (!(cond)) { printf("   FAIL %s\n", msg); failures++; } \
} while (0)

static double now_ms(void)
{
   LARGE_INTEGER f, c;
   QueryPerformanceFrequency(&f);
   QueryPerformanceCounter(&c);
   return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

static void settle(LONG want_calls)
{
   int i;
   for (i = 0; i < 400 && N(calls) < want_calls; i++)
      Sleep(5);
   Sleep(20);
}

int main(void)
{
   double t0, took;
   LONG before;
   int i;

   /* the second controller is plugged in */
   g_XInputSetState             = fake_set_state;
   g_xinput_states[1].connected = true;
   xinput_rumble_start();
   CHECK(xinput_rumble_writer != NULL, "the rumble writer did not start");

   /* a controller slow to answer: the call returns at once, and the
    * strength still arrives */
   slow_ms = 60;
   t0      = now_ms();
   CHECK(xinput_joypad_rumble(1, RETRO_RUMBLE_STRONG, 0x8000), "rumble was refused");
   took    = now_ms() - t0;
   CHECK(took < 20.0, "the call waited for a controller that takes 60 ms to answer");
   settle(1);
   CHECK(N(calls) == 1 && N(last_user) == 1 && N(last_left) == 0x8000 && N(last_right) == 0,
         "the strength did not arrive");
   printf("   ok   a controller that takes 60 ms to answer: the call returns in %.2f ms and the strength still arrives\n", took);

   /* many strengths while the controller is busy: it is given the
    * latest, not each in turn */
   before = N(calls);
   t0     = now_ms();
   for (i = 1; i <= 20; i++)
      xinput_joypad_rumble(1, RETRO_RUMBLE_STRONG, (uint16_t)(0x1000 + i));
   took   = now_ms() - t0;
   settle(before + 1);
   Sleep(200);
   CHECK(N(last_left) == 0x1000 + 20, "the last strength written is not the last one asked for");
   CHECK(N(calls) - before < 20, "every strength was written one by one");
   CHECK(took < 20.0, "twenty calls waited for the controller");
   printf("   ok   twenty strengths in %.2f ms: %ld written, the last one last\n",
         took, (long)(N(calls) - before));
   slow_ms = 0;

   /* the weak motor goes with the strong one's strength kept */
   before = N(calls);
   xinput_joypad_rumble(1, RETRO_RUMBLE_WEAK, 0x4000);
   settle(before + 1);
   CHECK(N(last_left) == 0x1000 + 20 && N(last_right) == 0x4000,
         "the weak motor's strength did not arrive beside the strong one's");
   /* the same strength again is not written again */
   before = N(calls);
   xinput_joypad_rumble(1, RETRO_RUMBLE_WEAK, 0x4000);
   Sleep(60);
   CHECK(N(calls) == before, "an unchanged strength was written again");
   /* both stopped */
   xinput_joypad_rumble(1, RETRO_RUMBLE_STRONG, 0);
   xinput_joypad_rumble(1, RETRO_RUMBLE_WEAK, 0);
   settle(before + 1);
   Sleep(60);
   CHECK(N(last_left) == 0 && N(last_right) == 0, "the motors were not stopped");
   printf("   ok   the weak motor, an unchanged strength and a stop are written as they should be\n");

   /* a controller that is not there is refused, and nothing is written */
   before = N(calls);
   CHECK(!xinput_joypad_rumble(3, RETRO_RUMBLE_STRONG, 0x8000),
         "rumble was accepted for a controller that is not plugged in");
   Sleep(40);
   CHECK(N(calls) == before, "a controller that is not plugged in was written to");

   /* the writer stopped: the call is made by the caller, as before */
   xinput_rumble_stop();
   CHECK(xinput_rumble_writer == NULL, "the writer did not stop");
   slow_ms = 30;
   before  = N(calls);
   t0      = now_ms();
   xinput_joypad_rumble(1, RETRO_RUMBLE_STRONG, 0x7000);
   took    = now_ms() - t0;
   CHECK(N(calls) == before + 1 && N(last_left) == 0x7000 && took >= 25.0,
         "with no writer the strength was not written by the caller");
   printf("   ok   with the writer stopped the caller writes, as it used to (%.0f ms)\n", took);

   if (failures)
   {
      printf("FAIL xinput_rumble_test: %u\n", failures);
      return 1;
   }
   printf("PASS xinput_rumble_test\n");
   return 0;
}
