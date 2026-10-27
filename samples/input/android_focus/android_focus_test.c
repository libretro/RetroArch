/* android_focus_test: RetroArch's focus on Android from its Activity's
 * window (input/drivers/android_focus.h). Times in ms. */

#include <stdio.h>
#include <string.h>

#include "android_focus.h"

enum
{
   A_ON = 1, /* APP_CMD_GAINED_FOCUS */
   A_OFF,    /* APP_CMD_LOST_FOCUS */
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

/* The Activity's focus, acted on once per poll */
static const struct step activity[] = {
   {    0, POLL, KEEP },  /* nothing before the first focus command */
   {   10, A_ON,  0 }, {   10, POLL, GAIN },
   {  500, A_OFF, 0 }, {  500, POLL, LOSE },
   {  900, A_ON,  0 }, {  900, POLL, GAIN },
   { 1200, A_OFF, 0 }, { 1200, A_ON, 0 }, { 1200, POLL, KEEP },
   { 1300, A_ON,  0 }, { 1300, POLL, KEEP }
};

static const struct focus_case cases[] = {
   CASE(activity)
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
      int got;

      switch (s->what)
      {
         case A_ON:
            android_focus_activity(&f, true);
            break;
         case A_OFF:
            android_focus_activity(&f, false);
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
