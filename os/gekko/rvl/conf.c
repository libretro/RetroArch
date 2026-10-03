/* Wii: console settings from SYSCONF and the system menu's
 * setting.txt, read once on first use. */

#include <string.h>

#include <gekko/conf.h>
#include <gekko/ios.h>
#include <gekko/thread.h>

#define SYSCONF_SIZE 0x4000
#define SETTING_SIZE 0x100

enum
{
   SC_BIGARRAY = 1,
   SC_SMALLARRAY,
   SC_BYTE,
   SC_SHORT,
   SC_LONG,
   SC_LONGLONG,
   SC_BOOL
};

static uint8_t   sysconf[SYSCONF_SIZE] __attribute__((aligned(32)));
static char      setting[SETTING_SIZE + 1] __attribute__((aligned(32)));
static int       loaded;
static int       have_sysconf;
static int       have_setting;
static gk_mutex_t load_lock = GK_MUTEX_INIT;

static int read_file(const char *path, void *buf, uint32_t len)
{
   int32_t n;
   int32_t fd = gk_ios_open(path, GK_IOS_READ);
   if (fd < 0)
      return 0;
   n = gk_ios_read(fd, buf, len);
   gk_ios_close(fd);
   return n == (int32_t)len;
}

/* setting.txt is XORed with a key rotated left once per byte. */
static void decrypt_setting(void)
{
   uint32_t key = 0x73b5dbfau;
   unsigned i;
   for (i = 0; i < SETTING_SIZE; i++)
   {
      setting[i] ^= (char)(key & 0xff);
      key = (key << 1) | (key >> 31);
   }
   setting[SETTING_SIZE] = '\0';
}

static void load(void)
{
   if (loaded)
      return;
   gk_mutex_lock(&load_lock);
   if (!loaded)
   {
      have_sysconf = read_file("/shared2/sys/SYSCONF", sysconf, SYSCONF_SIZE)
            && !memcmp(sysconf, "SCv0", 4);
      if ((have_setting = read_file(
                  "/title/00000001/00000002/data/setting.txt",
                  setting, SETTING_SIZE)))
         decrypt_setting();
      loaded = 1;
   }
   gk_mutex_unlock(&load_lock);
}

/* The value of a SYSCONF item and its size, or NULL. */
static const uint8_t *sysconf_item(const char *name, unsigned *size)
{
   unsigned count, i;
   size_t   name_len = strlen(name);
   if (!have_sysconf)
      return NULL;
   count = (sysconf[4] << 8) | sysconf[5];
   for (i = 0; i < count && 6 + i * 2 + 1 < SYSCONF_SIZE; i++)
   {
      unsigned off = (sysconf[6 + i * 2] << 8) | sysconf[7 + i * 2];
      const uint8_t *p;
      unsigned type, len;
      if (off + 1 >= SYSCONF_SIZE)
         break;
      p    = sysconf + off;
      type = p[0] >> 5;
      len  = (p[0] & 0x1f) + 1;
      if (len != name_len || off + 1 + len >= SYSCONF_SIZE
            || memcmp(p + 1, name, len))
         continue;
      p += 1 + len;
      switch (type)
      {
         case SC_BIGARRAY:
            *size = ((p[0] << 8) | p[1]) + 1;
            p    += 2;
            break;
         case SC_SMALLARRAY:
            *size = p[0] + 1;
            p    += 1;
            break;
         case SC_BYTE:
         case SC_BOOL:
            *size = 1;
            break;
         case SC_SHORT:
            *size = 2;
            break;
         case SC_LONG:
            *size = 4;
            break;
         case SC_LONGLONG:
            *size = 8;
            break;
         default:
            return NULL;
      }
      if ((size_t)(p - sysconf) + *size > SYSCONF_SIZE)
         return NULL;
      return p;
   }
   return NULL;
}

static int sysconf_byte(const char *name)
{
   unsigned size;
   const uint8_t *p;
   load();
   if (!(p = sysconf_item(name, &size)) || size != 1)
      return -1;
   return p[0];
}

int gk_conf_counter_bias(uint32_t *bias)
{
   unsigned size;
   const uint8_t *p;
   load();
   if (!(p = sysconf_item("IPL.CB", &size)) || size != 4)
      return -1;
   *bias = ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
   return 0;
}

int gk_conf_wide(void)
{
   int v = sysconf_byte("IPL.AR");
   return v < 0 ? -1 : v != 0;
}

int gk_conf_progressive(void)
{
   int v = sysconf_byte("IPL.PGS");
   return v < 0 ? -1 : v != 0;
}

int gk_conf_eurgb60(void)
{
   int v = sysconf_byte("IPL.E60");
   return v < 0 ? -1 : v != 0;
}

int gk_conf_display_offset_h(void)
{
   int v = sysconf_byte("IPL.DH");
   if (v < 0)
      return 0;
   return (int)(int8_t)v;
}

int gk_conf_video(void)
{
   const char *p;
   load();
   if (!have_setting || !(p = strstr(setting, "VIDEO=")))
      return -1;
   p += 6;
   if (!strncmp(p, "NTSC", 4))
      return GK_CONF_VIDEO_NTSC;
   if (!strncmp(p, "MPAL", 4))
      return GK_CONF_VIDEO_MPAL;
   if (!strncmp(p, "PAL", 3))
      return GK_CONF_VIDEO_PAL;
   return -1;
}

int gk_conf_sensor_bar_top(void)
{
   int v = sysconf_byte("BT.BAR");
   return v < 0 ? -1 : v != 0;
}

int gk_conf_ir_sensitivity(void)
{
   unsigned size;
   const uint8_t *p;
   load();
   if (!(p = sysconf_item("BT.SENS", &size)) || size != 4)
      return -1;
   return p[3];
}
