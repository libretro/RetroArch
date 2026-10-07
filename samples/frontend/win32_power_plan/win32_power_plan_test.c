/* The low-latency power plan in frontend/drivers/platform_win32.c,
 * compiled from its shipping text against a fake powrprof that keeps
 * plans, their AC values and the active plan the way Windows does.
 *
 *   capped    the active plan caps the maximum processor state below
 *             100%; the copy must pin minimum and maximum at 100%,
 *             re-evaluate every 5 s and park no cores, and leave the
 *             original's own values alone
 *   recover   a run that ended without restoring: the next start finds
 *             the copy active, reactivates the original and deletes it
 *   switched  the user picked another plan while the copy was active:
 *             turning the setting off leaves that choice alone
 *   refused   a write is refused, as group policy may: nothing is left
 *             behind and the original stays active
 *   none      no powrprof, as before Vista: the setting reports
 *             failure
 *   idle      idle states held off, then let back, while the plan
 *             stays on: one copy at a time, carrying the current choice
 *   missing   the energy preference, PCI Express and USB settings do
 *             not exist: the plan applies without them
 *   idlefail  holding idle states off is refused: nothing is left
 *             behind, and the plan without it still applies
 *   clean     a start with the setting off and no run's copy left
 *             behind loads powrprof not at all
 *   marked    a run ended with the copy active and its mark set; the
 *             next start, told by the mark, loads powrprof, restores
 *             and clears it */

#include <stdio.h>
#include <stdarg.h>

#include "fake_powrprof.h"

enum scenario
{
   SC_CAPPED = 0,
   SC_RECOVER,
   SC_SWITCHED,
   SC_REFUSED,
   SC_NONE,
   SC_IDLE,
   SC_MISSING,
   SC_IDLEFAIL,
   SC_CLEAN,
   SC_MARKED
};

/* The four processor values the copy writes, by index */
enum
{
   V_PERFCHECK = 0,
   V_MIN,
   V_MAX,
   V_PARKING,
   V_EPP,
   V_IDLE,
   V_ASPM,
   V_USB,
   V_COUNT
};

static const GUID g_perfcheck = { 0x4d2b0152, 0x7d5c, 0x498b,
   { 0x88, 0xe2, 0x34, 0x34, 0x53, 0x92, 0xa2, 0xc5 } };
static const GUID g_min       = { 0x893dee8e, 0x2bef, 0x41e0,
   { 0x89, 0xc6, 0xb5, 0x5d, 0x09, 0x29, 0x96, 0x4c } };
static const GUID g_max       = { 0xbc5038f7, 0x23e0, 0x4960,
   { 0x96, 0xda, 0x33, 0xab, 0xaf, 0x59, 0x35, 0xec } };
static const GUID g_parking   = { 0x0cc5b647, 0xc1df, 0x4637,
   { 0x89, 0x1a, 0xde, 0xc3, 0x5c, 0x31, 0x85, 0x83 } };
static const GUID g_sub       = { 0x54533251, 0x82be, 0x4824,
   { 0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00 } };
static const GUID g_epp       = { 0x36687f9e, 0xe3a5, 0x4dbf,
   { 0xb1, 0xdc, 0x15, 0xeb, 0x38, 0x1c, 0x68, 0x63 } };
static const GUID g_idle      = { 0x5d76a2ca, 0xe8c0, 0x402f,
   { 0xa1, 0x33, 0x21, 0x58, 0x49, 0x2d, 0x58, 0xad } };
static const GUID g_sub_pcie  = { 0x501a4d13, 0x42af, 0x4429,
   { 0x9f, 0xd1, 0xa8, 0x21, 0x8c, 0x26, 0x8e, 0x20 } };
static const GUID g_aspm      = { 0xee12f906, 0xd277, 0x404b,
   { 0xb6, 0xda, 0xe5, 0xfa, 0x1a, 0x57, 0x6d, 0xf5 } };
static const GUID g_sub_usb   = { 0x2a737441, 0x1930, 0x4402,
   { 0x8d, 0x77, 0xb2, 0xbe, 0xbb, 0xa3, 0x08, 0xa3 } };
static const GUID g_usb       = { 0x48e6b7a6, 0x50f5, 0x4782,
   { 0xa5, 0xd4, 0x53, 0xbb, 0x8f, 0x07, 0xe2, 0x26 } };

/* The user's plans: one that caps the processor at 99%, and another */
static const GUID g_user  = { 0x11111111, 0x2222, 0x3333,
   { 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 } };
static const GUID g_other = { 0x66666666, 0x7777, 0x8888,
   { 0x99, 0x99, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa } };

#define MAX_PLANS 4

typedef struct
{
   GUID  guid;
   DWORD ac[V_COUNT];
   UCHAR desc[256];
   DWORD desc_size;
   bool  used;
} plan_t;

static enum scenario sc;
static plan_t plans[MAX_PLANS];
static GUID   active;
static int    powrprof_token;
static int    writes;
static int    failures;

static plan_t *find(const GUID *g)
{
   size_t i;
   for (i = 0; i < MAX_PLANS; i++)
      if (plans[i].used && !memcmp(&plans[i].guid, g, sizeof(GUID)))
         return &plans[i];
   return NULL;
}

static plan_t *add(const GUID *g)
{
   size_t i;
   for (i = 0; i < MAX_PLANS; i++)
      if (!plans[i].used)
      {
         memset(&plans[i], 0, sizeof(plans[i]));
         plans[i].guid = *g;
         plans[i].used = true;
         return &plans[i];
      }
   return NULL;
}

static int value_index(const GUID *sub, const GUID *setting)
{
   if (!sub)
      return -1;
   if (!memcmp(sub, &g_sub_pcie, sizeof(GUID)))
      return memcmp(setting, &g_aspm, sizeof(GUID)) ? -1 : V_ASPM;
   if (!memcmp(sub, &g_sub_usb, sizeof(GUID)))
      return memcmp(setting, &g_usb, sizeof(GUID)) ? -1 : V_USB;
   if (memcmp(sub, &g_sub, sizeof(GUID)))
      return -1;
   if (!memcmp(setting, &g_epp, sizeof(GUID)))
      return V_EPP;
   if (!memcmp(setting, &g_idle, sizeof(GUID)))
      return V_IDLE;
   if (!memcmp(setting, &g_perfcheck, sizeof(GUID)))
      return V_PERFCHECK;
   if (!memcmp(setting, &g_min, sizeof(GUID)))
      return V_MIN;
   if (!memcmp(setting, &g_max, sizeof(GUID)))
      return V_MAX;
   if (!memcmp(setting, &g_parking, sizeof(GUID)))
      return V_PARKING;
   return -1;
}

/* --- fake powrprof -------------------------------------------------- */

static DWORD WINAPI fake_get_active(HKEY root, GUID **out)
{
   if (!(*out = (GUID*)malloc(sizeof(GUID))))
      return 1;
   **out = active;
   return ERROR_SUCCESS;
}

static DWORD WINAPI fake_set_active(HKEY root, const GUID *g)
{
   if (!find(g))
      return 2;
   active = *g;
   return ERROR_SUCCESS;
}

/* A GUID passed in is the new plan's; this fake never makes one up */
static DWORD WINAPI fake_duplicate(HKEY root, const GUID *src, GUID **dst)
{
   plan_t *from = find(src);
   plan_t *to;
   if (!from || !dst || !*dst || find(*dst) || !(to = add(*dst)))
      return 3;
   memcpy(to->ac, from->ac, sizeof(to->ac));
   return ERROR_SUCCESS;
}

static DWORD WINAPI fake_remove(HKEY root, const GUID *g)
{
   plan_t *p = find(g);
   if (!p || !memcmp(g, &active, sizeof(GUID)))
      return 4;
   p->used = false;
   return ERROR_SUCCESS;
}

static DWORD WINAPI fake_write_ac(HKEY root, const GUID *scheme,
      const GUID *sub, const GUID *setting, DWORD value)
{
   plan_t *p = find(scheme);
   int i     = value_index(sub, setting);
   if (!p || i < 0)
      return 5;
   if (sc == SC_REFUSED && i == V_MAX)
      return 5; /* access denied by policy */
   if (sc == SC_IDLEFAIL && i == V_IDLE)
      return 5;
   if (sc == SC_MISSING && (i == V_EPP || i == V_ASPM || i == V_USB))
      return 0x490; /* ERROR_NOT_FOUND: no such setting here */
   writes++;
   p->ac[i] = value;
   return ERROR_SUCCESS;
}

static DWORD WINAPI fake_write_name(HKEY root, const GUID *scheme,
      const GUID *sub, const GUID *setting, UCHAR *buf, DWORD size)
{
   return find(scheme) ? ERROR_SUCCESS : 6;
}

static DWORD WINAPI fake_write_desc(HKEY root, const GUID *scheme,
      const GUID *sub, const GUID *setting, UCHAR *buf, DWORD size)
{
   plan_t *p = find(scheme);
   if (!p || size > sizeof(p->desc))
      return 7;
   memcpy(p->desc, buf, size);
   p->desc_size = size;
   return ERROR_SUCCESS;
}

static DWORD WINAPI fake_read_desc(HKEY root, const GUID *scheme,
      const GUID *sub, const GUID *setting, UCHAR *buf, DWORD *size)
{
   plan_t *p = find(scheme);
   if (!p || !p->desc_size || *size < p->desc_size)
      return 8;
   memcpy(buf, p->desc, p->desc_size);
   *size = p->desc_size;
   return ERROR_SUCCESS;
}

static int loads;

HMODULE LoadLibraryA(const char *name)
{
   loads++;
   if (sc == SC_NONE || strcmp(name, "powrprof.dll"))
      return NULL;
   return &powrprof_token;
}

/* The registry: one value under the user's key, the mark */
static int  reg_marked;
static int  reg_token;

LONG RegOpenKeyExA(HKEY root, const char *sub, DWORD opt, DWORD sam,
      HKEY *out)
{
   (void)sub; (void)opt; (void)sam;
   if (root != HKEY_CURRENT_USER)
      return 2;
   *out = &reg_token;
   return ERROR_SUCCESS;
}

LONG RegCreateKeyExA(HKEY root, const char *sub, DWORD reserved,
      char *cls, DWORD opt, DWORD sam, void *sa, HKEY *out, DWORD *disp)
{
   (void)reserved; (void)cls; (void)sa; (void)disp;
   return RegOpenKeyExA(root, sub, opt, sam, out);
}

LONG RegQueryValueExA(HKEY key, const char *name, DWORD *reserved,
      DWORD *type, LPBYTE data, DWORD *size)
{
   DWORD v = (DWORD)reg_marked;
   (void)name; (void)reserved;
   if (key != &reg_token || !reg_marked || *size < sizeof(v))
      return 2;
   if (type)
      *type = REG_DWORD;
   memcpy(data, &v, sizeof(v));
   *size = sizeof(v);
   return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, const char *name, DWORD reserved,
      DWORD type, const BYTE *data, DWORD size)
{
   DWORD v;
   (void)name; (void)reserved;
   if (key != &reg_token || type != REG_DWORD || size != sizeof(v))
      return 87;
   memcpy(&v, data, sizeof(v));
   reg_marked = v != 0;
   return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, const char *name)
{
   (void)name;
   if (key != &reg_token)
      return 2;
   reg_marked = 0;
   return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key) { (void)key; return ERROR_SUCCESS; }

FARPROC GetProcAddress(HMODULE module, const char *name)
{
   if (module != &powrprof_token)
      return NULL;
   if (!strcmp(name, "PowerGetActiveScheme"))
      return (FARPROC)fake_get_active;
   if (!strcmp(name, "PowerSetActiveScheme"))
      return (FARPROC)fake_set_active;
   if (!strcmp(name, "PowerDuplicateScheme"))
      return (FARPROC)fake_duplicate;
   if (!strcmp(name, "PowerDeleteScheme"))
      return (FARPROC)fake_remove;
   if (!strcmp(name, "PowerWriteACValueIndex"))
      return (FARPROC)fake_write_ac;
   if (!strcmp(name, "PowerWriteFriendlyName"))
      return (FARPROC)fake_write_name;
   if (!strcmp(name, "PowerWriteDescription"))
      return (FARPROC)fake_write_desc;
   if (!strcmp(name, "PowerReadDescription"))
      return (FARPROC)fake_read_desc;
   return NULL;
}

void *LocalFree(void *mem)
{
   free(mem);
   return NULL;
}

void fake_log(const char *fmt, ...) { (void)fmt; }

/* --- the shipping code ---------------------------------------------- */

#include "power_plan_sim.c"

/* --- checks ---------------------------------------------------------- */

static void expect(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

static int plan_count(void)
{
   int i, n = 0;
   for (i = 0; i < MAX_PLANS; i++)
      n += plans[i].used;
   return n;
}

static bool is_active(const GUID *g)
{
   return !memcmp(&active, g, sizeof(GUID));
}

static void expect_original_untouched(void)
{
   plan_t *u = find(&g_user);
   expect(u && u->ac[V_PERFCHECK] == 15 && u->ac[V_MIN] == 5
         && u->ac[V_MAX] == 99 && u->ac[V_PARKING] == 10
         && u->ac[V_EPP] == 33 && u->ac[V_IDLE] == 0
         && u->ac[V_ASPM] == 2 && u->ac[V_USB] == 1,
         "the user's own plan keeps its values");
}

static void expect_copy_active(void)
{
   plan_t *c = find(&win32_power_plan_guid);
   expect(c != NULL, "the copy exists");
   expect(is_active(&win32_power_plan_guid), "the copy is active");
   if (!c)
      return;
   expect(c->ac[V_PERFCHECK] == 5000, "performance is re-evaluated every 5 s");
   expect(c->ac[V_MIN] == 100, "minimum processor state is 100%");
   expect(c->ac[V_MAX] == 100,
         "maximum processor state is 100% though the original caps it");
   expect(c->ac[V_PARKING] == 100, "no cores are parked");
   if (sc == SC_MISSING)
      expect(c->ac[V_EPP] == 33 && c->ac[V_ASPM] == 2 && c->ac[V_USB] == 1,
            "settings the system lacks are left as copied");
   else
   {
      expect(c->ac[V_EPP] == 0, "the energy preference favours performance");
      expect(c->ac[V_ASPM] == 0, "PCI Express link power management is off");
      expect(c->ac[V_USB] == 0, "USB selective suspend is off");
   }
}

static void expect_idle(DWORD want, const char *what)
{
   plan_t *c = find(&win32_power_plan_guid);
   expect(c && c->ac[V_IDLE] == want, what);
}

static void expect_restored(const GUID *want)
{
   expect(is_active(want), "the plan in use before is active again");
   expect(!find(&win32_power_plan_guid), "the copy is deleted");
   expect(plan_count() == 2, "nothing else is left behind");
}

int main(int argc, char **argv)
{
   static const char *names[] = {
      "capped", "recover", "switched", "refused", "none",
      "idle", "missing", "idlefail", "clean", "marked" };
   plan_t *u;
   size_t i;

   if (argc < 2)
      return 2;
   for (i = 0; i < ARRAY_SIZE(names); i++)
      if (!strcmp(argv[1], names[i]))
         break;
   if (i == ARRAY_SIZE(names))
      return 2;
   sc = (enum scenario)i;

   u = add(&g_user);
   u->ac[V_PERFCHECK] = 15;
   u->ac[V_MIN]       = 5;
   u->ac[V_MAX]       = 99;
   u->ac[V_PARKING]   = 10;
   u->ac[V_EPP]       = 33;
   u->ac[V_IDLE]      = 0;
   u->ac[V_ASPM]      = 2;
   u->ac[V_USB]       = 1;
   add(&g_other);
   active = g_user;

   switch (sc)
   {
      case SC_CAPPED:
         expect(frontend_win32_set_power_plan(true, false), "the plan is applied");
         expect_copy_active();
         expect_original_untouched();
         expect(frontend_win32_set_power_plan(true, false),
               "applying it again succeeds");
         expect(plan_count() == 3, "and makes no second copy");
         expect(frontend_win32_set_power_plan(false, false), "the plan is turned off");
         expect_restored(&g_user);
         expect_original_untouched();
         break;
      case SC_RECOVER:
         expect(frontend_win32_set_power_plan(true, false), "the plan is applied");
         expect(reg_marked, "the copy is marked as active");
         /* The process ends here without restoring; the next one
          * starts with the setting off. */
         win32_power_plan_flags = WIN32_POWER_PLAN_LOADED;
         expect(frontend_win32_set_power_plan(false, false),
               "the next start turns it off");
         expect_restored(&g_user);
         expect(!reg_marked, "the mark is cleared with the copy");
         break;
      case SC_CLEAN:
         expect(frontend_win32_set_power_plan(false, false),
               "a start with the setting off succeeds");
         expect(loads == 0, "and loads powrprof not at all");
         expect(is_active(&g_user) && plan_count() == 2,
               "and touches no plan");
         expect(frontend_win32_set_power_plan(false, false),
               "turning it off again succeeds");
         expect(loads == 0, "still without powrprof");
         break;
      case SC_MARKED:
         /* A run that ended with the copy active, its mark set: the
          * copy names the plan it came from, as apply writes it */
         {
            plan_t *c = add(&win32_power_plan_guid);
            WCHAR desc[37];
            win32_power_guid_to_wstr(&g_user, desc);
            memcpy(c->desc, desc, sizeof(desc));
            c->desc_size = sizeof(desc);
            active       = win32_power_plan_guid;
            reg_marked   = 1;
         }
         expect(frontend_win32_set_power_plan(false, false),
               "the next start turns it off");
         expect(loads == 1, "the mark has powrprof loaded");
         expect(!find(&win32_power_plan_guid), "the copy is deleted");
         expect(!reg_marked, "the mark is cleared");
         break;
      case SC_SWITCHED:
         expect(frontend_win32_set_power_plan(true, false), "the plan is applied");
         active = g_other;
         expect(frontend_win32_set_power_plan(false, false), "the plan is turned off");
         expect(is_active(&g_other), "the plan the user picked stays active");
         expect(!find(&win32_power_plan_guid), "the copy is deleted");
         break;
      case SC_REFUSED:
         expect(!frontend_win32_set_power_plan(true, false),
               "a refused write reports failure");
         expect(is_active(&g_user), "the user's plan stays active");
         expect(!find(&win32_power_plan_guid), "no copy is left behind");
         expect_original_untouched();
         break;
      case SC_NONE:
         expect(!frontend_win32_set_power_plan(true, false),
               "without powrprof the setting reports failure");
         expect(is_active(&g_user) && plan_count() == 2 && !writes,
               "and nothing is touched");
         break;
      case SC_IDLE:
         expect(frontend_win32_set_power_plan(true, false), "the plan is applied");
         expect_idle(0, "idle states are allowed by default");
         expect(frontend_win32_set_power_plan(true, true),
               "idle states are held off");
         expect_copy_active();
         expect_idle(1, "the copy keeps the processor out of idle states");
         expect(plan_count() == 3, "one copy at a time");
         expect(frontend_win32_set_power_plan(true, false),
               "idle states are let back");
         expect_copy_active();
         expect_idle(0, "the new copy allows idle states again");
         expect(plan_count() == 3, "still one copy");
         expect(frontend_win32_set_power_plan(false, true),
               "the plan is turned off with idle disable still set");
         expect_restored(&g_user);
         expect_original_untouched();
         break;
      case SC_MISSING:
         expect(frontend_win32_set_power_plan(true, false),
               "the plan applies without the settings this system lacks");
         expect_copy_active();
         expect(frontend_win32_set_power_plan(false, false), "the plan is turned off");
         expect_restored(&g_user);
         break;
      case SC_IDLEFAIL:
         expect(!frontend_win32_set_power_plan(true, true),
               "a refused idle write reports failure");
         expect(is_active(&g_user), "the user's plan stays active");
         expect(!find(&win32_power_plan_guid), "no copy is left behind");
         expect(frontend_win32_set_power_plan(true, false),
               "the plan without it still applies");
         expect_copy_active();
         expect_idle(0, "and allows idle states");
         expect(frontend_win32_set_power_plan(false, false), "the plan is turned off");
         expect_restored(&g_user);
         expect_original_untouched();
         break;
   }

   for (i = 0; i < MAX_PLANS; i++)
      plans[i].used = false;

   printf("%s: %s\n", argv[1], failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
