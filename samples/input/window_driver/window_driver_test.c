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

static settings_t *test_settings;
settings_t *config_get_ptr(void) { return test_settings; }

#define D_UDEV     1
#define D_X        2
#define D_LINUXRAW 4
#define D_WINRAW   8
#define D_DINPUT   16

static unsigned starts;          /* which drivers start */
static char tried[64];           /* the attempts, in order */

static const char *name_of(const input_driver_t *d)
{
   if (d == &input_udev)     return "udev";
   if (d == &input_x)        return "x";
   if (d == &input_linuxraw) return "linuxraw";
   if (d == &input_winraw)   return "raw";
   if (d == &input_dinput)   return "dinput";
   return d ? "?" : "none";
}

static unsigned bit_of(const input_driver_t *d)
{
   if (d == &input_udev)     return D_UDEV;
   if (d == &input_x)        return D_X;
   if (d == &input_linuxraw) return D_LINUXRAW;
   if (d == &input_winraw)   return D_WINRAW;
   if (d == &input_dinput)   return D_DINPUT;
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
#endif

   free(test_settings);
   if (failures)
   {
      printf("FAIL window_driver_test: %d case(s)\n", failures);
      return 1;
   }
   printf("PASS window_driver_test\n");
   return 0;
}
