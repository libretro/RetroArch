/* Which input driver a window gets (input/input_driver_choice.c), with
 * stand-in drivers.
 *
 * The functions under test are the real ones. What is stood in for is
 * everything they call: the settings, the driver objects, and
 * input_driver_init_wrap(), which here starts a driver or fails to
 * according to a mask and writes down each attempt in order.
 *
 * Built for the host this runs the X11 and the no-window-system
 * choices; built with mingw and run under Wine, the Windows one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

#include "../../../input/input_driver.h"
#include "../../../configuration.h"

/* ---- stand-ins ------------------------------------------------- */

input_driver_t input_udev;
input_driver_t input_x;
input_driver_t input_linuxraw;
input_driver_t input_winraw;
input_driver_t input_dinput;
input_driver_t input_sdl3;
input_driver_t input_sdl1;
input_driver_t input_wayland;

static settings_t *test_settings;
settings_t *config_get_ptr(void) { return test_settings; }

#define D_UDEV     1
#define D_X        2
#define D_LINUXRAW 4
#define D_WINRAW   8
#define D_DINPUT   16
#define D_SDL3     32
#define D_SDL1     64

static unsigned starts;          /* which drivers start */
static char tried[64];           /* the attempts, in order */

static const char *name_of(const input_driver_t *d)
{
   if (d == &input_udev)     return "udev";
   if (d == &input_x)        return "x";
   if (d == &input_linuxraw) return "linuxraw";
   if (d == &input_winraw)   return "raw";
   if (d == &input_dinput)   return "dinput";
   if (d == &input_sdl3)     return "sdl3";
   if (d == &input_sdl1)     return "sdl";
   if (d == &input_wayland)  return "wayland";
   return d ? "?" : "none";
}

static unsigned bit_of(const input_driver_t *d)
{
   if (d == &input_udev)     return D_UDEV;
   if (d == &input_x)        return D_X;
   if (d == &input_linuxraw) return D_LINUXRAW;
   if (d == &input_winraw)   return D_WINRAW;
   if (d == &input_dinput)   return D_DINPUT;
   if (d == &input_sdl3)     return D_SDL3;
   if (d == &input_sdl1)     return D_SDL1;
   return 0;
}

static const char *joypad_seen;

void *input_driver_init_wrap(input_driver_t *input, const char *name)
{
   if (tried[0])
      strcat(tried, ",");
   strcat(tried, name_of(input));
   joypad_seen = name;
   /* a started driver's data: any non-NULL pointer that is its own */
   return (starts & bit_of(input)) ? (void*)input : NULL;
}

/* a driver left running across a video driver restart, if any */
static input_driver_t *kept;
bool input_driver_take_kept(input_driver_t **input, void **input_data)
{
   if (!kept)
      return false;
   *input      = kept;
   *input_data = (void*)kept;
   kept        = NULL;
   return true;
}

/* the Wayland input driver's start, and the joypad drivers' */
static bool wl_starts;
static unsigned joypads_started;
bool input_wl_init(void *data, const char *joypad_name)
{
   joypad_seen = joypad_name;
   return data && wl_starts;
}
void input_driver_init_joypads(void) { joypads_started++; }

/* ---- the cases ------------------------------------------------- */

typedef void (*choice_fn)(const char *, input_driver_t **, void **);

static int failures;

static void expect(const char *what, choice_fn fn,
      const char *setting, unsigned starting,
      const char *want_driver, const char *want_tried)
{
   input_driver_t *input = (input_driver_t*)0x1; /* must be overwritten */
   void *input_data      = (void*)0x1;
   const char *got;
   int ok;

   strcpy(test_settings->arrays.input_driver, setting);
   starts      = starting;
   tried[0]    = '\0';
   joypad_seen = NULL;

   fn("the-joypad-driver", &input, &input_data);

   got = name_of(input);
   ok  =    !strcmp(got, want_driver)
         && !strcmp(tried, want_tried)
         /* both set or both NULL, and the data is that driver's */
         && ((input == NULL) == (input_data == NULL))
         && (!input || input_data == (void*)input)
         /* the joypad driver's name is passed through untouched */
         && (!tried[0] || (joypad_seen
               && !strcmp(joypad_seen, "the-joypad-driver")));

   printf("%s %-8s setting %-7s -> %-8s (tried: %s)\n",
         ok ? "[ok]  " : "[FAIL]", what, setting, got,
         tried[0] ? tried : "nothing");
   if (!ok)
   {
      printf("        wanted %s, tried: %s\n", want_driver,
            want_tried[0] ? want_tried : "nothing");
      failures++;
   }
}

int main(void)
{
   test_settings = (settings_t*)calloc(1, sizeof(*test_settings));
   if (!test_settings)
      return 2;

#if defined(_WIN32)
   /* raw input when it is the setting and it starts; DirectInput
    * otherwise, including when raw input fails to start */
   expect("windows", input_driver_init_windows, "raw",    D_WINRAW | D_DINPUT, "raw",    "raw");
   expect("windows", input_driver_init_windows, "raw",    D_DINPUT,            "dinput", "raw,dinput");
   expect("windows", input_driver_init_windows, "dinput", D_WINRAW | D_DINPUT, "dinput", "dinput");
   expect("windows", input_driver_init_windows, "sdl2",   D_WINRAW | D_DINPUT, "dinput", "dinput");
   expect("windows", input_driver_init_windows, "raw",    0,                   "none",   "raw,dinput");
   /* a driver left running across a video driver restart is handed
    * back, and none is started */
   kept = &input_winraw;
   expect("windows", input_driver_init_windows, "raw",    D_WINRAW | D_DINPUT, "raw",    "");
   expect("windows", input_driver_init_windows, "raw",    D_WINRAW | D_DINPUT, "raw",    "raw");

   /* an SDL 3 window: raw input or DirectInput when one is the
    * setting and starts; the SDL 3 driver otherwise */
   expect("sdl3", input_driver_init_sdl3, "raw",    D_WINRAW | D_DINPUT | D_SDL3, "raw",    "raw");
   expect("sdl3", input_driver_init_sdl3, "dinput", D_WINRAW | D_DINPUT | D_SDL3, "dinput", "dinput");
   expect("sdl3", input_driver_init_sdl3, "raw",    D_SDL3,                       "sdl3",   "raw,sdl3");
   expect("sdl3", input_driver_init_sdl3, "sdl3",   D_WINRAW | D_DINPUT | D_SDL3, "sdl3",   "sdl3");
   expect("sdl3", input_driver_init_sdl3, "raw",    0,                            "none",   "raw,sdl3");
#else
   /* an X11 window: udev only when it is the setting and it starts */
   expect("x11", input_driver_init_x11, "udev", D_UDEV | D_X, "udev", "udev");
   expect("x11", input_driver_init_x11, "udev", D_X,          "x",    "udev,x");
   expect("x11", input_driver_init_x11, "x",    D_UDEV | D_X, "x",    "x");
   expect("x11", input_driver_init_x11, "sdl2", D_UDEV | D_X, "x",    "x");
   expect("x11", input_driver_init_x11, "udev", 0,            "none", "udev,x");

   /* no window system: "x" and "udev" both mean udev, then linuxraw;
    * anything else is left to the frontend */
   expect("kms", input_driver_init_kms, "x",        D_UDEV | D_LINUXRAW, "udev",     "udev");
   expect("kms", input_driver_init_kms, "udev",     D_UDEV | D_LINUXRAW, "udev",     "udev");
   expect("kms", input_driver_init_kms, "udev",     D_LINUXRAW,          "linuxraw", "udev,linuxraw");
   expect("kms", input_driver_init_kms, "x",        0,                   "none",     "udev,linuxraw");
   expect("kms", input_driver_init_kms, "linuxraw", D_UDEV | D_LINUXRAW, "none",     "");
   expect("kms", input_driver_init_kms, "sdl2",     D_UDEV | D_LINUXRAW, "none",     "");

   /* an SDL 3 window: udev or linuxraw when one is the setting and
    * starts; the SDL 3 driver for any other setting, a window
    * system's included, and when the one named does not start */
#ifdef HAVE_SDL3
   expect("sdl3", input_driver_init_sdl3, "udev",     D_UDEV | D_LINUXRAW | D_SDL3, "udev",     "udev");
   expect("sdl3", input_driver_init_sdl3, "linuxraw", D_UDEV | D_LINUXRAW | D_SDL3, "linuxraw", "linuxraw");
   expect("sdl3", input_driver_init_sdl3, "udev",     D_SDL3,                       "sdl3",     "udev,sdl3");
   expect("sdl3", input_driver_init_sdl3, "x",        D_UDEV | D_X | D_SDL3,        "sdl3",     "sdl3");
   expect("sdl3", input_driver_init_sdl3, "sdl3",     D_UDEV | D_SDL3,              "sdl3",     "sdl3");
   expect("sdl3", input_driver_init_sdl3, "udev",     0,                            "none",     "udev,sdl3");
#endif

#if defined(HAVE_SDL) && !defined(HAVE_SDL2) && !defined(HAVE_SDL3)
   /* an SDL 1.2 window: the SDL 1.2 driver, whatever the setting */
   expect("sdl1", input_driver_init_sdl1, "udev", D_UDEV | D_X | D_SDL1, "sdl",  "sdl");
   expect("sdl1", input_driver_init_sdl1, "sdl",  D_SDL1,                "sdl",  "sdl");
   expect("sdl1", input_driver_init_sdl1, "x",    D_UDEV | D_X,          "none", "sdl");
#endif

   /* a Wayland surface: the Wayland driver, on the seat's state the
    * video context handed over, and the joypad drivers started once;
    * nothing without that state, or if it does not start */
   {
      int seat                = 0;
      input_driver_t *input   = (input_driver_t*)0x1;
      void *input_data        = (void*)0x1;
      int ok;

      wl_starts       = true;
      joypads_started = 0;
      joypad_seen     = NULL;
      input_driver_init_wayland("the-joypad-driver", &seat, &input, &input_data);
      ok =     input == &input_wayland && input_data == (void*)&seat
            && joypads_started == 1
            && joypad_seen && !strcmp(joypad_seen, "the-joypad-driver");
      printf("%s wayland  with the seat's state -> %s, joypad drivers started %u time(s)\n",
            ok ? "[ok]  " : "[FAIL]", name_of(input), joypads_started);
      if (!ok)
         failures++;

      input = (input_driver_t*)0x1; input_data = (void*)0x1; joypads_started = 0;
      input_driver_init_wayland("the-joypad-driver", NULL, &input, &input_data);
      ok = !input && !input_data && joypads_started == 0;
      printf("%s wayland  with no state handed over -> %s\n",
            ok ? "[ok]  " : "[FAIL]", name_of(input));
      if (!ok)
         failures++;

      wl_starts = false;
      input = (input_driver_t*)0x1; input_data = (void*)0x1;
      input_driver_init_wayland("the-joypad-driver", &seat, &input, &input_data);
      ok = !input && !input_data && joypads_started == 0;
      printf("%s wayland  that does not start -> %s\n",
            ok ? "[ok]  " : "[FAIL]", name_of(input));
      if (!ok)
         failures++;
   }
#endif

   /* a platform with an input driver of its own: this host is not
    * one, so nothing is started and the frontend's default follows */
   expect("platform", input_driver_init_platform, "udev", ~0, "none", "");

   free(test_settings);
   if (failures)
   {
      printf("FAIL window_driver_test: %d case(s)\n", failures);
      return 1;
   }
   printf("PASS window_driver_test\n");
   return 0;
}
