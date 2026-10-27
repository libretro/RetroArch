/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _INPUT_GROUP_H
#define _INPUT_GROUP_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* Device groups: one controller that reaches the OS as several input
 * devices - a handheld's pad and its extra buttons, a console's pad and
 * the button the system keeps apart - read as one pad on one port.
 *
 * A rule says which devices make a group: the platform model it holds
 * on, the names of its members, and the role each name has. Supporting
 * another such device is a row in a rule table, not a branch in a
 * driver.
 *
 * The first member to arrive leads: its id is the one the driver gives
 * a slot. Later members join and their events are read as the lead's.
 * The group keeps its slot while members come and go; when the lead
 * leaves, the next member leads and the slot is handed its id. The
 * group ends when its last member leaves. Several groups may stand at
 * once, each of its own rule or of the same one.
 *
 * Ids are the OS's device ids; -1 is none. One thread, no allocation,
 * no driver types, so samples/input/input_group builds it alone. */

#define INPUT_GROUPS        4
#define INPUT_GROUP_MEMBERS 4
#define INPUT_GROUP_NAMES   4

/* A member's role: either part of the pad, starting the group or
 * joining it; the pad proper, which starts a group and is a pad of its
 * own where one stands already (a second controller); or a part that
 * only joins (a system button), and is nothing without a group. */
#define INPUT_GROUP_MEMBER  0
#define INPUT_GROUP_LEADS   1
#define INPUT_GROUP_FOLLOWS 2

typedef struct input_group_name
{
   const char *part;    /* a part of the device's name */
   unsigned    role;    /* INPUT_GROUP_LEADS or INPUT_GROUP_FOLLOWS */
} input_group_name_t;

typedef struct input_group_rule
{
   const char *name;    /* the pad's name, for its profile */
   /* parts of the platform model, any of which holds; NULL ends */
   const char *models[INPUT_GROUP_NAMES];
   /* the members' names; NULL ends */
   input_group_name_t members[INPUT_GROUP_NAMES];
   /* on the model, every device is a member, named or not */
   bool any_name;
   /* a member's name holds on any model */
   bool name_alone;
   /* the group's slot is port 1's */
   bool first_port;
} input_group_rule_t;

typedef struct input_group
{
   int32_t  members[INPUT_GROUP_MEMBERS];  /* [0] leads */
   unsigned count;
   int      rule;
} input_group_t;

typedef struct input_groups
{
   input_group_t group[INPUT_GROUPS];
} input_groups_t;

static INLINE void input_groups_clear(input_groups_t *g)
{
   memset(g, 0, sizeof(*g));
}

/* The rule @device_name on @model falls under, and the role it has
 * there; -1 for none. */
static INLINE int input_group_rule_find(const input_group_rule_t *rules,
      size_t num_rules, const char *model, const char *device_name,
      unsigned *role)
{
   size_t r;
   for (r = 0; r < num_rules; r++)
   {
      const input_group_rule_t *rule = &rules[r];
      bool on_model = false;
      unsigned i;
      for (i = 0; i < INPUT_GROUP_NAMES && rule->models[i]; i++)
         if (strstr(model, rule->models[i]))
            on_model = true;
      if (!on_model && !rule->name_alone)
         continue;
      for (i = 0; i < INPUT_GROUP_NAMES && rule->members[i].part; i++)
         if (strstr(device_name, rule->members[i].part))
         {
            *role = rule->members[i].role;
            return (int)r;
         }
      if (on_model && rule->any_name)
      {
         *role = INPUT_GROUP_MEMBER;
         return (int)r;
      }
   }
   return -1;
}

/* The group @id is in, or NULL */
static INLINE input_group_t *input_groups_of(input_groups_t *g, int32_t id)
{
   unsigned i, m;
   for (i = 0; i < INPUT_GROUPS; i++)
      for (m = 0; m < g->group[i].count; m++)
         if (g->group[i].members[m] == id)
            return &g->group[i];
   return NULL;
}

/* The id @id's events are read as: its group's lead, or itself */
static INLINE int32_t input_groups_lead(const input_groups_t *g, int32_t id)
{
   input_group_t *grp = input_groups_of((input_groups_t*)g, id);
   return grp ? grp->members[0] : id;
}

/* @id of @rule arrives in @role. The lead it is read as: @id itself
 * where the driver gives it a slot (it starts a group, or is a pad of
 * its own); another id where it joined that one's group; -1 where it
 * only follows and there is no group, or no room. */
static INLINE int32_t input_groups_join(input_groups_t *g, int rule,
      unsigned role, int32_t id)
{
   unsigned i;
   input_group_t *grp = input_groups_of(g, id);

   if (grp)
      return grp->members[0];
   /* the group of this rule that is there, if any */
   for (i = 0; i < INPUT_GROUPS; i++)
   {
      grp = &g->group[i];
      if (grp->count && grp->rule == rule)
      {
         if (role == INPUT_GROUP_LEADS)
            return id;
         if (grp->count >= INPUT_GROUP_MEMBERS)
            return -1;
         grp->members[grp->count++] = id;
         return grp->members[0];
      }
   }
   if (role == INPUT_GROUP_FOLLOWS)
      return -1;
   for (i = 0; i < INPUT_GROUPS; i++)
   {
      grp = &g->group[i];
      if (!grp->count)
      {
         grp->rule       = rule;
         grp->members[0] = id;
         grp->count      = 1;
         return id;
      }
   }
   return -1;
}

/* @id leaves. The group's lead after it: another member's id when @id
 * led and others remain, so the driver hands that one the slot; -1
 * when @id was in no group or was its last member, and the slot goes
 * as any other. */
static INLINE int32_t input_groups_leave(input_groups_t *g, int32_t id)
{
   unsigned m;
   input_group_t *grp = input_groups_of(g, id);

   if (!grp)
      return -1;
   for (m = 0; m < grp->count && grp->members[m] != id; m++) { }
   for (; m + 1 < grp->count; m++)
      grp->members[m] = grp->members[m + 1];
   if (--grp->count == 0)
      return -1;
   return grp->members[0];
}

/* The group led by @lead, ended: its members leave together, as when a
 * device the rule replaces takes the slot */
static INLINE void input_groups_end(input_groups_t *g, int32_t lead)
{
   input_group_t *grp = input_groups_of(g, lead);
   if (grp)
      grp->count = 0;
}

#endif
