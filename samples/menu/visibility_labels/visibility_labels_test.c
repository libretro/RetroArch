/* visibility_labels_test.c -- a "Show '...'" setting names its entry.
 *
 * Menu Item Visibility's settings quote the Main Menu entry each one
 * shows or hides. The English strings are read here as intl/msg_hash_us.h
 * has them, by the name of their id, and each pair below is held to it:
 * the setting's label is "Show '<entry's label>'", and its sublabel
 * quotes the same label. */

#include <stdio.h>
#include <string.h>

struct row
{
   const char *id;
   const char *str;
};

static const struct row rows[] = {
#define MSG_HASH(Id, str) { #Id, str },
#include "intl/msg_hash_us.h"
#undef MSG_HASH
   { NULL, NULL }
};

static const char *us(const char *id)
{
   const struct row *r;
   for (r = rows; r->id; r++)
      if (!strcmp(r->id, id))
         return r->str;
   return NULL;
}

/* The Main Menu entry, and the setting that shows it */
static const char *const pairs[][2] = {
   { "QUIT_RETROARCH",    "MENU_SHOW_QUIT_RETROARCH"    },
   { "RESTART_RETROARCH", "MENU_SHOW_RESTART_RETROARCH" }
};

int main(void)
{
   unsigned i, failures = 0;

   for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
   {
      char id[128], want[160], quoted[160];
      const char *entry, *label, *sublabel;

      snprintf(id, sizeof(id), "MENU_ENUM_LABEL_VALUE_%s", pairs[i][0]);
      entry    = us(id);
      snprintf(id, sizeof(id), "MENU_ENUM_LABEL_VALUE_%s", pairs[i][1]);
      label    = us(id);
      snprintf(id, sizeof(id), "MENU_ENUM_SUBLABEL_%s", pairs[i][1]);
      sublabel = us(id);
      if (!entry || !label || !sublabel)
      {
         printf("   FAIL: %s: a string is gone (entry %s, label %s, sublabel %s)\n",
               pairs[i][1], entry ? "there" : "missing",
               label ? "there" : "missing", sublabel ? "there" : "missing");
         failures++;
         continue;
      }
      snprintf(want,   sizeof(want),   "Show '%s'", entry);
      snprintf(quoted, sizeof(quoted), "'%s'", entry);
      if (strcmp(label, want))
      {
         printf("   FAIL: %s is \"%s\", its entry is \"%s\": want \"%s\"\n",
               pairs[i][1], label, entry, want);
         failures++;
      }
      if (!strstr(sublabel, quoted))
      {
         printf("   FAIL: %s's sublabel \"%s\" does not name %s\n",
               pairs[i][1], sublabel, quoted);
         failures++;
      }
   }
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   puts("visibility labels: each names its entry as the Main Menu shows it");
   return 0;
}
