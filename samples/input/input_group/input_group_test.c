/* input_group_test.c -- device groups (input/input_group.h) and the
 * Android driver's rules for its built-in controllers
 * (input/drivers/android_groups.h).
 *
 *   1. The rules take the devices the driver's chain of special cases
 *      took, in its order: a reference of that chain and the table agree
 *      on every model and device name tried.
 *   2. Either join order gives one pad: the first member has the slot,
 *      the second is read as the first.
 *   3. A member leaving keeps the pad; the lead leaving hands the slot
 *      to the next member; the last leaving ends the group.
 *   4. Two groups stand at once, each with its own lead.
 *   5. A member that only joins is nothing without a group; a second
 *      controller where a group stands is a pad of its own.
 */
#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "drivers/android_groups.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* The driver's chain before the table, which of its branches a device
 * fell in: 0 to 5 as the rules, -1 none. The "NVIDIA" rename sat after
 * the three SHIELD branches and before the rest. */
static int ref_branch(const char *m, const char *n)
{
   if (strstr(m, "SHIELD Android TV") && (strstr(n, "Virtual")
            || strstr(n, "NVIDIA Corporation NVIDIA Controller v01.0")))
      return ANDROID_GROUP_SHIELD_TV;
   if (strstr(m, "SHIELD") && (strstr(n, "Virtual") || strstr(n, "gpio")
            || strstr(n, "NVIDIA Corporation NVIDIA Controller v01.01")
            || strstr(n, "NVIDIA Corporation NVIDIA Controller v01.02")))
      return ANDROID_GROUP_SHIELD_PORTABLE;
   if (strstr(m, "SHIELD") && (strstr(n, "Virtual") || strstr(n, "gpio")
            || strstr(n, "NVIDIA Corporation NVIDIA Controller v01.03")))
      return ANDROID_GROUP_SHIELD_GAMEPAD;
   if (strstr(n, "NVIDIA"))
      return -1;
   if (strstr(m, "XD") && (strstr(n, "Virtual") || strstr(n, "rk29-keypad")
            || strstr(n, "Playstation3") || strstr(n, "XBOX")))
      return ANDROID_GROUP_GPD_XD;
   if (     !strncmp(m, "R800", 4) || strstr(m, "Xperia Play")
         || strstr(m, "Play") || strstr(m, "SO-01D")
         || strstr(n, "keypad-game-zeus") || strstr(n, "keypad-zeus")
         || strstr(n, "Android Gamepad"))
      return ANDROID_GROUP_XPERIA_PLAY;
   if (strstr(m, "ARCHOS GAMEPAD") && (strstr(n, "joy_key")
            || strstr(n, "joystick")))
      return ANDROID_GROUP_ARCHOS;
   return -1;
}

static int table_branch(const char *m, const char *n, unsigned *role)
{
   int g = input_group_rule_find(android_group_rules, ANDROID_GROUP_RULES,
         m, n, role);
   if (g > ANDROID_GROUP_SHIELD_GAMEPAD && strstr(n, "NVIDIA"))
      return -1;
   return g;
}

static void lane_rules(void)
{
   static const char *models[] = {
      "SHIELD Android TV", "SHIELD Tablet", "SHIELD Portable", "GPD XD",
      "R800i", "Xperia Play", "SO-01D", "ARCHOS GAMEPAD", "Pixel 8",
      "AFTMM", "" };
   static const char *names[] = {
      "Virtual", "gpio-keys", "NVIDIA Corporation NVIDIA Controller v01.01",
      "NVIDIA Corporation NVIDIA Controller v01.02",
      "NVIDIA Corporation NVIDIA Controller v01.03",
      "NVIDIA Corporation NVIDIA Controller v01.04", "rk29-keypad",
      "Sony Playstation3 Controller", "XBOX 360 pad", "keypad-game-zeus",
      "keypad-zeus", "Android Gamepad", "joy_key", "joystick",
      "Xbox Wireless Controller", "SHIELD Remote", "" };
   size_t i, j;
   unsigned bad = 0;
   for (i = 0; i < sizeof(models) / sizeof(models[0]); i++)
      for (j = 0; j < sizeof(names) / sizeof(names[0]); j++)
      {
         unsigned role;
         if (ref_branch(models[i], names[j])
               != table_branch(models[i], names[j], &role))
         {
            fprintf(stderr, "  %s / %s: chain %d, table %d\n", models[i],
                  names[j], ref_branch(models[i], names[j]),
                  table_branch(models[i], names[j], &role));
            bad++;
         }
      }
   CHECK(!bad, "the rules do not take the devices the driver's chain took");
}

static void lane_join_orders(void)
{
   unsigned order;
   for (order = 0; order < 2; order++)
   {
      input_groups_t g;
      int32_t a = order ? 21 : 20, b = order ? 20 : 21;
      input_groups_clear(&g);
      CHECK(input_groups_join(&g, ANDROID_GROUP_XPERIA_PLAY,
               INPUT_GROUP_MEMBER, a) == a, "the first member has no slot");
      CHECK(input_groups_join(&g, ANDROID_GROUP_XPERIA_PLAY,
               INPUT_GROUP_MEMBER, b) == a, "the second member is not read as the first");
      CHECK(input_groups_lead(&g, b) == a && input_groups_lead(&g, a) == a,
            "a member's events are not read as the lead's");
      CHECK(input_groups_join(&g, ANDROID_GROUP_XPERIA_PLAY,
               INPUT_GROUP_MEMBER, b) == a, "a member seen again joined twice");
      CHECK(input_groups_lead(&g, 99) == 99, "a device in no group was moved");
   }
}

static void lane_leave(void)
{
   input_groups_t g;
   input_groups_clear(&g);
   input_groups_join(&g, ANDROID_GROUP_SHIELD_PORTABLE, INPUT_GROUP_MEMBER, 5);
   input_groups_join(&g, ANDROID_GROUP_SHIELD_PORTABLE, INPUT_GROUP_MEMBER, 6);
   input_groups_join(&g, ANDROID_GROUP_SHIELD_PORTABLE, INPUT_GROUP_MEMBER, 7);
   CHECK(input_groups_lead(&g, 7) == 5, "a third member is not read as the lead");
   /* a member goes: the pad stays */
   CHECK(input_groups_leave(&g, 6) == 5, "a member leaving moved the slot");
   CHECK(input_groups_lead(&g, 6) == 6, "a member that left is still read as the lead");
   CHECK(input_groups_lead(&g, 7) == 5, "the members left were dropped");
   /* the lead goes: the next one leads */
   CHECK(input_groups_leave(&g, 5) == 7, "the lead leaving did not hand the slot on");
   CHECK(input_groups_lead(&g, 7) == 7, "the next member does not lead");
   /* it comes back: it joins the group that stands */
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_PORTABLE,
            INPUT_GROUP_MEMBER, 8) == 7, "a member back did not join the group");
   CHECK(input_groups_leave(&g, 7) == 8 && input_groups_leave(&g, 8) == -1,
         "the last member leaving did not end the group");
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_PORTABLE,
            INPUT_GROUP_MEMBER, 9) == 9, "a group ended still took members");
   CHECK(input_groups_leave(&g, 42) == -1, "a device in no group left one");
}

static void lane_two_groups(void)
{
   input_groups_t g;
   input_groups_clear(&g);
   CHECK(input_groups_join(&g, ANDROID_GROUP_GPD_XD, INPUT_GROUP_MEMBER, 1) == 1
         && input_groups_join(&g, ANDROID_GROUP_ARCHOS, INPUT_GROUP_MEMBER, 2) == 2,
         "a second group did not start");
   CHECK(input_groups_join(&g, ANDROID_GROUP_ARCHOS, INPUT_GROUP_MEMBER, 3) == 2
         && input_groups_join(&g, ANDROID_GROUP_GPD_XD, INPUT_GROUP_MEMBER, 4) == 1,
         "members joined the wrong group");
   CHECK(input_groups_lead(&g, 3) == 2 && input_groups_lead(&g, 4) == 1,
         "two groups' members mixed");
   input_groups_end(&g, 1);
   CHECK(input_groups_lead(&g, 4) == 4 && input_groups_lead(&g, 3) == 2,
         "ending one group touched the other");
}

static void lane_roles(void)
{
   input_groups_t g;
   unsigned role = 99;
   input_groups_clear(&g);
   /* the NVIDIA button before its controller: nothing to join */
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_GAMEPAD,
            INPUT_GROUP_FOLLOWS, 10) == -1, "a following member took a slot alone");
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_GAMEPAD,
            INPUT_GROUP_LEADS, 11) == 11, "the controller did not start its group");
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_GAMEPAD,
            INPUT_GROUP_FOLLOWS, 10) == 11, "the button did not join its controller");
   /* a second controller is a pad of its own */
   CHECK(input_groups_join(&g, ANDROID_GROUP_SHIELD_GAMEPAD,
            INPUT_GROUP_LEADS, 12) == 12
         && input_groups_lead(&g, 12) == 12,
         "a second controller was merged into the first");
   CHECK(table_branch("SHIELD Android TV", "Virtual", &role)
            == ANDROID_GROUP_SHIELD_TV && role == INPUT_GROUP_FOLLOWS,
         "the SHIELD TV's button does not only follow");
   CHECK(table_branch("R800i", "some keypad", &role)
            == ANDROID_GROUP_XPERIA_PLAY && role == INPUT_GROUP_MEMBER,
         "on the Xperia Play a device of another name is not a member");
   /* full: no more members than it holds */
   {
      int32_t id;
      input_groups_clear(&g);
      for (id = 1; id <= INPUT_GROUP_MEMBERS; id++)
         input_groups_join(&g, ANDROID_GROUP_GPD_XD, INPUT_GROUP_MEMBER, id);
      CHECK(input_groups_join(&g, ANDROID_GROUP_GPD_XD, INPUT_GROUP_MEMBER, 50) == -1,
            "a full group took another member");
   }
}

int main(void)
{
   lane_rules();
   lane_join_orders();
   lane_leave();
   lane_two_groups();
   lane_roles();
   if (failures)
   {
      fprintf(stderr, "FAIL input_group_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS input_group_test: the rules take what the chain"
         " took; either join order, members and the lead leaving, two groups"
         " at once, roles\n");
   return 0;
}
