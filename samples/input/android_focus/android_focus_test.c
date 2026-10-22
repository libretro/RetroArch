/* android_focus_test: RetroArch's focus on Android from its two
 * windows (input/drivers/android_focus.h), through the sequences the
 * AYN Thor's focus modes and a system with one focus produce. Times
 * in ms. */

#include <stdio.h>
#include <string.h>

#include "android_focus.h"

enum
{
   A_ON = 1, /* APP_CMD_GAINED_FOCUS */
   A_OFF,    /* APP_CMD_LOST_FOCUS */
   P_ON,     /* APP_CMD_SECOND_GAINED_FOCUS */
   P_OFF,    /* APP_CMD_SECOND_LOST_FOCUS */
   IN,       /* a key, stick move or touch read from the Presentation */
   POLL      /* android_focus_settle(), which must act as want says */
};

#define KEEP ANDROID_FOCUS_KEEP
#define GAIN ANDROID_FOCUS_GAIN
#define LOSE ANDROID_FOCUS_LOSE

struct step
{
   int ms;
   int what;
   int want;
};

struct focus_case
{
   const char *name;
   const struct step *steps;
   size_t count;
};

#define CASE(s) { #s, s, sizeof(s) / sizeof(s[0]) }

/* No Presentation: the Activity's focus alone, acted on once per poll */
static const struct step no_presentation[] = {
   {    0, POLL, KEEP },  /* nothing before the first focus command */
   {   10, A_ON,  0 }, {   10, POLL, GAIN },
   {  500, A_OFF, 0 }, {  500, POLL, LOSE },
   {  900, A_ON,  0 }, {  900, POLL, GAIN },
   { 1200, A_OFF, 0 }, { 1200, A_ON, 0 }, { 1200, POLL, KEEP },
   { 1300, A_ON,  0 }, { 1300, POLL, KEEP }
};

/* Auto, pad on top: the Presentation takes the bottom screen's focus
 * when it is shown; the keys go to the Activity */
static const struct step auto_pad_top[] = {
   {     0, A_ON, 0 }, {     0, POLL, GAIN },
   { 61000, P_ON, 0 }, { 61000, POLL, KEEP },
   { 62000, POLL, KEEP }
};

/* Auto, a tap on the bottom screen: no focus event, keys arrive there */
static const struct step auto_bottom_tap[] = {
   {     0, A_ON, 0 }, {     0, POLL, GAIN },
   { 61000, P_ON, 0 }, { 61000, POLL, KEEP },
   { 72000, IN,   0 }, { 72000, POLL, KEEP },
   { 72100, IN,   0 }, { 72100, POLL, KEEP }
};

/* Focus mode switched to Bottom: AYN's panel takes the bottom screen,
 * the cover the top; the panel closes and the Presentation has the
 * bottom screen's focus again, so RetroArch runs */
static const struct step to_bottom[] = {
   {      0, A_ON,  0 }, {      0, POLL, GAIN },
   {  61000, P_ON,  0 }, {  61000, POLL, KEEP },
   {  72000, IN,    0 }, {  72000, POLL, KEEP },
   {  81000, P_OFF, 0 }, {  81000, POLL, KEEP },
   {  84000, A_OFF, 0 }, {  84000, POLL, LOSE },
   {  85000, P_ON,  0 }, {  85000, POLL, GAIN },
   {  88000, IN,    0 }, {  88000, POLL, KEEP },
   { 148000, IN,    0 }, { 148000, POLL, KEEP }
};

/* Bottom with keys at the Presentation, then back to Auto: the panel
 * covers the Presentation, the cover leaves the top, the panel closes
 * and keys keep arriving at the bottom screen */
static const struct step bottom_to_auto[] = {
   {      0, A_ON,  0 }, {      0, POLL, GAIN },
   {   1000, P_ON,  0 }, {   1000, POLL, KEEP },
   {   2000, A_OFF, 0 }, {   2000, POLL, LOSE },
   {   3000, IN,    0 }, {   3000, POLL, GAIN },
   { 157000, P_OFF, 0 }, { 157000, POLL, LOSE },
   { 158000, A_ON,  0 }, { 158000, POLL, GAIN },
   { 164000, P_ON,  0 }, { 164000, POLL, KEEP },
   { 165000, IN,    0 }, { 165000, POLL, KEEP }
};

/* Top after the pad drove the bottom screen: the cover takes it */
static const struct step to_top[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 2000, IN,    0 }, { 2000, POLL, KEEP },
   { 3000, P_OFF, 0 }, { 3000, POLL, KEEP }
};

/* Top, then back to Auto with the keys still at the bottom screen */
static const struct step top_to_auto_sticky[] = {
   {      0, A_ON,  0 }, {      0, POLL, GAIN },
   {   1000, P_ON,  0 }, {   1000, POLL, KEEP },
   { 169000, P_OFF, 0 }, { 169000, POLL, KEEP },
   { 172000, P_ON,  0 }, { 172000, IN, 0 }, { 172000, POLL, KEEP }
};

/* The shade on top while the pad drives the top: pauses, as with the
 * Activity alone */
static const struct step shade_pad_top[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 5000, A_OFF, 0 }, { 5000, POLL, LOSE },
   { 6000, POLL,  KEEP },
   { 9000, A_ON,  0 }, { 9000, POLL, GAIN }
};

/* The shade on top while the pad drives the bottom */
static const struct step shade_pad_bottom[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 2000, IN,    0 }, { 2000, POLL, KEEP },
   { 5000, A_OFF, 0 }, { 5000, POLL, LOSE },
   { 6000, POLL,  KEEP },
   { 9000, A_ON,  0 }, { 9000, POLL, GAIN }
};

/* Touching the bottom screen while the shade opens keeps RetroArch
 * running */
static const struct step shade_touching_bottom[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 5000, A_OFF, 0 }, { 5000, IN, 0 }, { 5000, POLL, KEEP },
   { 5016, IN,    0 }, { 5016, POLL, KEEP },
   { 9000, A_ON,  0 }, { 9000, POLL, KEEP }
};

/* AYN's panel over the Presentation while the pad drives the top */
static const struct step panel_pad_top[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 3000, P_OFF, 0 }, { 3000, POLL, KEEP }
};

/* AYN's panel over the Presentation while the pad drives the bottom:
 * runs, as with the Activity alone */
static const struct step panel_pad_bottom[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 2000, IN,    0 }, { 2000, POLL, KEEP },
   { 3000, P_OFF, 0 }, { 3000, POLL, KEEP },
   { 6000, P_ON,  0 }, { 6000, POLL, KEEP }
};

/* One focus, a tap on the monitor: both halves in one poll; back to
 * the phone; the phone's shade */
static const struct step one_focus_tap[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, IN,    0 }, { 1000, A_OFF, 0 }, { 1001, P_ON, 0 },
   { 1001, POLL,  KEEP },
   { 1100, IN,    0 }, { 1100, POLL, KEEP },
   { 5000, A_ON,  0 }, { 5000, P_OFF, 0 }, { 5000, POLL, KEEP },
   { 8000, A_OFF, 0 }, { 8000, POLL, LOSE }
};

/* One focus: the touch in one poll, both focus halves in the next */
static const struct step one_focus_touch_first[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, IN,    0 }, { 1000, POLL, KEEP },
   { 1016, A_OFF, 0 }, { 1016, P_ON, 0 }, { 1016, POLL, KEEP }
};

/* One focus: the Presentation's half in one poll, the Activity's
 * within 100 ms in the next */
static const struct step one_focus_second_first[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, IN,    0 }, { 1000, P_ON, 0 }, { 1000, POLL, KEEP },
   { 1016, A_OFF, 0 }, { 1016, POLL, KEEP }
};

/* One focus: the Activity's half first; one poll paused */
static const struct step one_focus_activity_first[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, IN,    0 }, { 1000, A_OFF, 0 }, { 1000, POLL, LOSE },
   { 1016, P_ON,  0 }, { 1016, POLL, GAIN }
};

/* Not a hand-off: the Activity's loss 200 ms after the Presentation
 * took display 4 (the Thor, a shade) */
static const struct step not_a_handoff[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 1200, A_OFF, 0 }, { 1200, POLL, LOSE }
};

/* Auto, pad on the bottom, the Presentation shown again: the old one's
 * loss and the new one's gain in one poll, then in two */
static const struct step reshow_auto[] = {
   {     0, A_ON,  0 }, {     0, POLL, GAIN },
   {  1000, P_ON,  0 }, {  1000, POLL, KEEP },
   {  2000, IN,    0 }, {  2000, POLL, KEEP },
   {  9000, P_OFF, 0 }, {  9000, P_ON, 0 }, {  9000, POLL, KEEP },
   { 12000, P_OFF, 0 }, { 12000, POLL, KEEP },
   { 12050, P_ON,  0 }, { 12050, POLL, KEEP }
};

/* Bottom mode, the Presentation shown again: the old one's loss and
 * the new one's gain in one poll, then in two */
static const struct step reshow_bottom[] = {
   {     0, A_ON,  0 }, {     0, POLL, GAIN },
   {  1000, P_ON,  0 }, {  1000, POLL, KEEP },
   {  2000, A_OFF, 0 }, {  2000, POLL, LOSE },
   {  3000, IN,    0 }, {  3000, POLL, GAIN },
   {  9000, P_OFF, 0 }, {  9000, P_ON, 0 }, {  9000, POLL, KEEP },
   { 12000, P_OFF, 0 }, { 12000, POLL, LOSE },
   { 12050, P_ON,  0 }, { 12050, POLL, GAIN }
};

/* Input from a Presentation without focus changes nothing */
static const struct step input_without_focus[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, A_OFF, 0 }, { 1000, POLL, LOSE },
   { 2000, IN,    0 }, { 2000, POLL, KEEP }
};

/* Home: the Activity loses focus and onStop dismisses the Presentation */
static const struct step home[] = {
   {    0, A_ON,  0 }, {    0, POLL, GAIN },
   { 1000, P_ON,  0 }, { 1000, POLL, KEEP },
   { 2000, IN,    0 }, { 2000, POLL, KEEP },
   { 5000, A_OFF, 0 }, { 5001, P_OFF, 0 }, { 5001, POLL, LOSE }
};

static const struct focus_case cases[] = {
   CASE(no_presentation),
   CASE(auto_pad_top),
   CASE(auto_bottom_tap),
   CASE(to_bottom),
   CASE(bottom_to_auto),
   CASE(to_top),
   CASE(top_to_auto_sticky),
   CASE(shade_pad_top),
   CASE(shade_pad_bottom),
   CASE(shade_touching_bottom),
   CASE(panel_pad_top),
   CASE(panel_pad_bottom),
   CASE(one_focus_tap),
   CASE(one_focus_touch_first),
   CASE(one_focus_second_first),
   CASE(one_focus_activity_first),
   CASE(not_a_handoff),
   CASE(reshow_auto),
   CASE(reshow_bottom),
   CASE(input_without_focus),
   CASE(home)
};

static const char *act_name(int act)
{
   if (act == GAIN)
      return "GAIN";
   if (act == LOSE)
      return "LOSE";
   return "KEEP";
}

static int run(const struct focus_case *c)
{
   size_t i;
   android_focus_t f;
   int fails = 0;

   memset(&f, 0, sizeof(f));
   for (i = 0; i < c->count; i++)
   {
      const struct step *s = &c->steps[i];
      int64_t us           = (int64_t)s->ms * 1000;
      int got;

      switch (s->what)
      {
         case A_ON:
            android_focus_activity(&f, true, us);
            break;
         case A_OFF:
            android_focus_activity(&f, false, us);
            break;
         case P_ON:
            android_focus_second(&f, true, us);
            break;
         case P_OFF:
            android_focus_second(&f, false, us);
            break;
         case IN:
            android_focus_second_input(&f);
            break;
         case POLL:
            got = (int)android_focus_settle(&f);
            if (got != s->want)
            {
               printf("FAIL %s: step %u at %d ms acted %s, want %s\n",
                     c->name, (unsigned)i, s->ms, act_name(got),
                     act_name(s->want));
               fails++;
            }
            break;
      }
   }
   return fails;
}

int main(void)
{
   size_t i;
   int fails = 0;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      fails += run(&cases[i]);
   if (fails)
   {
      printf("android_focus_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("PASS android_focus_test (%u cases)\n",
         (unsigned)(sizeof(cases) / sizeof(cases[0])));
   return 0;
}
