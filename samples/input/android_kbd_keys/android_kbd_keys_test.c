/* android_kbd_keys_test.c -- key events from the Android system
 * keyboard, through the pipeline they take to the menu text line.
 *
 * With "Use System Keyboard" on, the text lives in a Java-side field
 * and the menu line is a copy: every change to the field sends the
 * whole text. A soft keyboard commits most text straight into the
 * field, but sends some keys as key events - Backspace when nothing
 * is being composed, digits on the AOSP keyboard. RetroActivity is a
 * NativeActivity, so those go to the native input queue first and
 * reach the field only if native code finishes them unhandled.
 *
 * The bug this pins: the driver finished them handled. Typing
 * "192.168.17888", Backspace twice and "." left "192.168.17888." -
 * the field never saw the Backspaces, so the next snapshot put the
 * two characters back.
 *
 * The claims:
 *
 *   1. A Backspace key event from the soft keyboard edits the field,
 *      and the line follows.
 *   2. So does a character sent as a key event.
 *   3. Such a key is not also typed by the native line editor.
 *   4. BACK is still consumed natively while a session is open.
 *   5. A gamepad button is still consumed natively while one is open.
 *   6. With no session open, nothing is handed back.
 *
 * The decision is the driver's own, from android_kbd_route.h. The
 * framework's side - the field, and the stage that forwards an
 * unhandled event to it - is reproduced here, because the driver
 * only builds against the NDK. A sabotage mode restores the old
 * behaviour and is asserted to be caught. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "android_kbd_route.h"

/* android/keycodes.h and android/input.h, as far as this cares. */
#define KEYCODE_BACK              4
#define KEYCODE_0                 7
#define KEYCODE_DEL               67
#define KEYCODE_BUTTON_A          96
#define KEY_FLAG_SOFT_KEYBOARD    0x2
#define DEVICE_VIRTUAL_KEYBOARD   (-1)
#define DEVICE_GAMEPAD            7

#define TEXT_SIZE 512

/* Sabotage hook: 0 = shipping behaviour. */
static int sab_consume_all = 0;

static bool session_open;
static char field[TEXT_SIZE];    /* the Java EditText */
static char line[TEXT_SIZE];     /* the menu keyboard line */
static int  native_keys;         /* key events native code consumed */
static int  native_typed;        /* of those, typed into the line */

/* TextWatcher.afterTextChanged -> onSystemKeyboardInput -> poll. */
static void field_changed(void)
{
   if (session_open)
      strcpy(line, field);
}

/* InputConnection.commitText: straight into the field. */
static void ime_commit(const char *text)
{
   strcat(field, text);
   field_changed();
}

/* The field's own key handling, reached through the view hierarchy. */
static void field_key_down(int keycode)
{
   size_t len = strlen(field);

   if (keycode == KEYCODE_DEL)
   {
      if (!len)
         return;
      field[len - 1] = '\0';
   }
   else if (keycode >= KEYCODE_0 && keycode <= KEYCODE_0 + 9)
   {
      field[len]     = (char)('0' + (keycode - KEYCODE_0));
      field[len + 1] = '\0';
   }
   else
      return;
   field_changed();
}

/* The driver's key case; returns what it hands AInputQueue_finishEvent. */
static int native_key_down(int keycode, int flags, int device_id)
{
   bool from_ime = (flags & KEY_FLAG_SOFT_KEYBOARD)
         || device_id == DEVICE_VIRTUAL_KEYBOARD;

   if (     !sab_consume_all
         && android_kbd_key_goes_to_ime(session_open, from_ime,
               keycode == KEYCODE_BACK))
      return 0;

   native_keys++;
   /* input_keyboard_event -> the native line editor. */
   if (session_open && keycode == KEYCODE_DEL)
   {
      size_t len = strlen(line);
      if (len)
         line[len - 1] = '\0';
      native_typed++;
   }
   return 1;
}

/* NativePostImeInputStage: native code first, the views if unhandled. */
static void dispatch_key_down(int keycode, int flags, int device_id)
{
   if (!native_key_down(keycode, flags, device_id))
      field_key_down(keycode);
}

static void session_start(void)
{
   field[0]     = '\0';
   line[0]      = '\0';
   native_keys  = 0;
   native_typed = 0;
   session_open = true;
}

static int failures;

static void expect(bool ok, const char *what)
{
   if (ok)
      return;
   failures++;
   if (!sab_consume_all)
      printf("[FAIL] %s (line \"%s\", field \"%s\")\n", what, line, field);
}

static int run(void)
{
   failures = 0;

   /* 1, 3: the reported sequence, both ways an IME marks its events. */
   session_start();
   ime_commit("192.168.17888");
   dispatch_key_down(KEYCODE_DEL, KEY_FLAG_SOFT_KEYBOARD,
         DEVICE_VIRTUAL_KEYBOARD);
   dispatch_key_down(KEYCODE_DEL, 0, DEVICE_VIRTUAL_KEYBOARD);
   ime_commit(".");
   expect(!strcmp(line, "192.168.178."), "Backspace edits the line");
   expect(native_typed == 0, "Backspace is not typed natively too");

   session_start();
   ime_commit("ab");
   dispatch_key_down(KEYCODE_DEL, KEY_FLAG_SOFT_KEYBOARD, 3);
   expect(!strcmp(line, "a"), "Backspace by flag alone edits the line");

   /* 2: digits as key events. */
   session_start();
   dispatch_key_down(KEYCODE_0 + 4, KEY_FLAG_SOFT_KEYBOARD,
         DEVICE_VIRTUAL_KEYBOARD);
   dispatch_key_down(KEYCODE_0 + 2, KEY_FLAG_SOFT_KEYBOARD,
         DEVICE_VIRTUAL_KEYBOARD);
   expect(!strcmp(line, "42"), "a digit key event reaches the line");

   /* 4, 5: what stays native during a session. */
   session_start();
   dispatch_key_down(KEYCODE_BACK, KEY_FLAG_SOFT_KEYBOARD,
         DEVICE_VIRTUAL_KEYBOARD);
   expect(native_keys == 1, "BACK stays native");
   dispatch_key_down(KEYCODE_BUTTON_A, 0, DEVICE_GAMEPAD);
   expect(native_keys == 2, "a gamepad button stays native");

   /* 6: no session, nothing handed back. */
   session_open = false;
   native_keys  = 0;
   dispatch_key_down(KEYCODE_DEL, KEY_FLAG_SOFT_KEYBOARD,
         DEVICE_VIRTUAL_KEYBOARD);
   expect(native_keys == 1, "closed session keeps keys native");

   return failures;
}

int main(void)
{
   int caught;

   if (run())
      return 1;
   printf("[pass] system keyboard key events reach the text field\n");

   /* The old behaviour: every key event finished handled. */
   sab_consume_all = 1;
   caught          = run();
   sab_consume_all = 0;
   if (!caught)
   {
      printf("[FAIL] consuming the IME's key events went unnoticed\n");
      return 1;
   }
   printf("[pass] sabotage caught (%d checks)\n", caught);
   return 0;
}
