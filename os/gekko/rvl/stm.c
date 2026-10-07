/* Wii: the state manager (/dev/stm): buttons, power off, restart. */

#include <gekko/ios.h>

#include "../kernel/kernel.h"

#define STM_EVENTHOOK  0x1000
#define STM_HOTRESET   0x2001
#define STM_SHUTDOWN   0x2003
#define STM_RELEASE_EH 0x3002

#define STM_EVENT_RESET 0x00020000u
#define STM_EVENT_POWER 0x00000800u

void gk_power_event(int power);

static int32_t  fd_immediate = -1;
static int32_t  fd_hook      = -1;
static uint32_t hook_buf[8] __attribute__((aligned(32)));
static uint32_t hook_armed;
static uint32_t hook_wanted;

static int open_fds(void)
{
   if (fd_immediate < 0)
      fd_immediate = gk_ios_open("/dev/stm/immediate", 0);
   if (fd_hook < 0)
      fd_hook = gk_ios_open("/dev/stm/eventhook", 0);
   return fd_immediate >= 0 && fd_hook >= 0;
}

static void arm(void);

/* Interrupt context: a button, or the hook released. */
static void hook_done(int32_t result, void *data)
{
   uint32_t ev = hook_buf[0];
   (void)data;
   hook_armed = 0;
   if (result < 0 || !hook_wanted)
      return;
   if (ev & STM_EVENT_POWER)
      gk_power_event(1);
   else if (ev & STM_EVENT_RESET)
      gk_power_event(0);
   arm();
}

static void arm(void)
{
   if (hook_armed || fd_hook < 0)
      return;
   hook_buf[0] = 0;
   if (gk_ios_ioctl_async(fd_hook, STM_EVENTHOOK, NULL, 0,
            hook_buf, sizeof(hook_buf), hook_done, NULL) == 0)
      hook_armed = 1;
}

void gk_stm_watch(int on)
{
   hook_wanted = on;
   if (on)
   {
      if (open_fds())
         arm();
   }
   else if (hook_armed && fd_immediate >= 0)
      gk_ios_ioctl(fd_immediate, STM_RELEASE_EH, NULL, 0, NULL, 0);
}

static int immediate(uint32_t cmd)
{
   static uint32_t in[8]  __attribute__((aligned(32)));
   static uint32_t out[8] __attribute__((aligned(32)));
   if (!open_fds())
      return -1;
   if (hook_armed)
   {
      hook_wanted = 0;
      gk_ios_ioctl(fd_immediate, STM_RELEASE_EH, NULL, 0, NULL, 0);
   }
   return gk_ios_ioctl(fd_immediate, cmd, in, sizeof(in), out, sizeof(out));
}

int gk_stm_power_off(void)
{
   return immediate(STM_SHUTDOWN);
}

int gk_stm_reset(void)
{
   return immediate(STM_HOTRESET);
}
