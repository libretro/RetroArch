/* Wii: Wii Remotes on the Bluetooth stack, after WiiBrew's description
 * of the remote's HID reports.
 *
 * A remote that connects gets its slot's LED and a status request.
 * The status says whether an extension is plugged in; one that is gets
 * initialised unencrypted (0x55 to 0xa400f0, 0x00 to 0xa400fb), its
 * identifier read from 0xa400fa, and the remote is set to report
 * continuously with the extension's bytes or without.  A status report
 * arrives by itself when an extension comes or goes, and starts that
 * over. */

#include <string.h>

#include <gekko/thread.h>
#include <gekko/wiimote.h>

#include "bt.h"

#define OUT_RUMBLE      0x10
#define OUT_LEDS        0x11
#define OUT_MODE        0x12
#define OUT_STATUS      0x15
#define OUT_WRITE       0x16
#define OUT_READ        0x17

#define IN_STATUS       0x20
#define IN_READ         0x21
#define IN_ACK          0x22

#define MODE_ACCEL      0x31
#define MODE_ACCEL_EXT  0x35
#define CONTINUOUS      0x04

#define EXT_RETRY_US    1000000

enum step
{
   STEP_IDLE = 0,
   STEP_ENABLE,      /* 0x55 to 0xa400f0 sent */
   STEP_PLAIN,       /* 0x00 to 0xa400fb sent */
   STEP_ID           /* the identifier asked for */
};

struct remote
{
   gk_wiimote_t st;
   uint64_t     step_at;
   uint8_t      step;
   uint8_t      rumble;
};

static struct remote     remotes[BT_MAX_LINKS];
static volatile uint8_t  want_rumble[BT_MAX_LINKS];
static volatile uint8_t  want_drop[BT_MAX_LINKS];
static gk_mutex_t        state_lock = GK_MUTEX_INIT;

/* ---- output reports ---- */

static void out(unsigned link, uint8_t id, const uint8_t *p, unsigned len)
{
   uint8_t b[24];
   b[0] = 0xa2;
   b[1] = id;
   memcpy(b + 2, p, len);
   /* Every output report carries the rumble bit. */
   b[2] = (uint8_t)((b[2] & ~1) | remotes[link].rumble);
   bt_send(link, b, len + 2);
}

static void out1(unsigned link, uint8_t id, uint8_t v)
{
   out(link, id, &v, 1);
}

static void set_mode(unsigned link)
{
   uint8_t p[2];
   p[0] = CONTINUOUS;
   p[1] = remotes[link].st.ext != GK_WM_EXT_NONE ? MODE_ACCEL_EXT
      : MODE_ACCEL;
   out(link, OUT_MODE, p, 2);
}

/* The extension's registers, through the remote's memory commands. */
static void reg_write(unsigned link, uint32_t addr, uint8_t value)
{
   uint8_t p[21];
   memset(p, 0, sizeof(p));
   p[0] = 0x04;   /* registers, not EEPROM */
   p[1] = (uint8_t)(addr >> 16);
   p[2] = (uint8_t)(addr >> 8);
   p[3] = (uint8_t)addr;
   p[4] = 1;
   p[5] = value;
   out(link, OUT_WRITE, p, sizeof(p));
}

static void reg_read(unsigned link, uint32_t addr, uint16_t len)
{
   uint8_t p[6];
   p[0] = 0x04;
   p[1] = (uint8_t)(addr >> 16);
   p[2] = (uint8_t)(addr >> 8);
   p[3] = (uint8_t)addr;
   p[4] = (uint8_t)(len >> 8);
   p[5] = (uint8_t)len;
   out(link, OUT_READ, p, sizeof(p));
}

static void step(unsigned link, enum step s)
{
   remotes[link].step    = (uint8_t)s;
   remotes[link].step_at = gk_ticks();
}

/* ---- input reports ---- */

static int8_t clamp8(int v)
{
   return (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
}

static void decode_ext(gk_wiimote_t *st, const uint8_t *e)
{
   if (st->ext == GK_WM_EXT_NUNCHUK)
   {
      st->stick[0][0] = clamp8(e[0] - 128);
      st->stick[0][1] = clamp8(e[1] - 128);
      st->ext_buttons = (uint16_t)(~e[5] & 3);
   }
   else if (st->ext == GK_WM_EXT_CLASSIC)
   {
      int lx = e[0] & 0x3f, ly = e[1] & 0x3f;
      int rx = ((e[0] >> 6) << 3) | ((e[1] >> 6) << 1) | (e[2] >> 7);
      int ry = e[2] & 0x1f;
      int lt = (((e[2] >> 5) & 3) << 3) | (e[3] >> 5), rt = e[3] & 0x1f;
      st->stick[0][0] = clamp8((lx - 32) * 4);
      st->stick[0][1] = clamp8((ly - 32) * 4);
      st->stick[1][0] = clamp8((rx - 16) * 8);
      st->stick[1][1] = clamp8((ry - 16) * 8);
      st->trigger[0]  = (uint8_t)(lt * 255 / 31);
      st->trigger[1]  = (uint8_t)(rt * 255 / 31);
      /* Active low; bit 8 is always set. */
      st->ext_buttons = (uint16_t)(~((e[4] << 8) | e[5]) & 0xfeff);
   }
}

static void bt_input(unsigned link, const uint8_t *d, unsigned len)
{
   struct remote *r = &remotes[link];
   gk_wiimote_t *st = &r->st;
   uint8_t id;
   const uint8_t *p;
   if (len < 4 || d[0] != 0xa1)
      return;
   id = d[1];
   p  = d + 2;
   len -= 2;
   gk_mutex_lock(&state_lock);
   st->buttons = (uint16_t)(((p[0] << 8) | p[1]) & 0x1f9f);
   switch (id)
   {
      case IN_STATUS:
         if (len >= 6)
         {
            int present = (p[2] & 0x02) != 0;
            st->battery = p[5];
            if (present && r->step == STEP_IDLE)
            {
               st->ext = GK_WM_EXT_NONE;
               reg_write(link, 0xa400f0, 0x55);
               step(link, STEP_ENABLE);
            }
            else if (!present)
            {
               st->ext = GK_WM_EXT_NONE;
               st->ext_buttons = 0;
               memset(st->stick, 0, sizeof(st->stick));
               memset(st->trigger, 0, sizeof(st->trigger));
               step(link, STEP_IDLE);
            }
            /* A status report stops the data reports until the mode is
             * set again. */
            set_mode(link);
         }
         break;
      case IN_ACK:
         if (len >= 4 && p[2] == OUT_WRITE)
         {
            if (r->step == STEP_ENABLE)
            {
               reg_write(link, 0xa400fb, 0x00);
               step(link, STEP_PLAIN);
            }
            else if (r->step == STEP_PLAIN)
            {
               reg_read(link, 0xa400fa, 6);
               step(link, STEP_ID);
            }
         }
         break;
      case IN_READ:
         if (len >= 21 && r->step == STEP_ID)
         {
            const uint8_t *idb = p + 5;
            if (p[2] & 0x0f)
               st->ext = GK_WM_EXT_OTHER;
            else if (idb[2] == 0xa4 && idb[3] == 0x20 && idb[4] == 0
                  && idb[5] == 0)
               st->ext = GK_WM_EXT_NUNCHUK;
            else if (idb[2] == 0xa4 && idb[3] == 0x20 && idb[4] == 1
                  && idb[5] == 1)
               st->ext = GK_WM_EXT_CLASSIC;
            else
               st->ext = GK_WM_EXT_OTHER;
            step(link, STEP_IDLE);
            set_mode(link);
         }
         break;
      case 0x31:
      case 0x35:
         if (len >= 5)
         {
            st->accel[0] = (uint16_t)((p[2] << 2) | ((p[0] >> 5) & 3));
            st->accel[1] = (uint16_t)((p[3] << 2) | ((p[1] >> 4) & 2));
            st->accel[2] = (uint16_t)((p[4] << 2) | ((p[1] >> 5) & 2));
         }
         if (id == 0x35 && len >= 21)
            decode_ext(st, p + 5);
         st->reports++;
         break;
      default:
         if (id >= 0x30 && id <= 0x3f)
            st->reports++;
         break;
   }
   gk_mutex_unlock(&state_lock);
}

/* ---- connections ---- */

static void bt_connected(unsigned link)
{
   struct remote *r = &remotes[link];
   gk_mutex_lock(&state_lock);
   memset(r, 0, sizeof(*r));
   r->st.connected = 1;
   gk_mutex_unlock(&state_lock);
   out1(link, OUT_LEDS, (uint8_t)(0x10 << link));
   out1(link, OUT_STATUS, 0);
}

static void bt_disconnected(unsigned link)
{
   gk_mutex_lock(&state_lock);
   memset(&remotes[link], 0, sizeof(remotes[link]));
   gk_mutex_unlock(&state_lock);
}

static void bt_tick(void)
{
   unsigned i;
   uint64_t now = gk_ticks();
   for (i = 0; i < BT_MAX_LINKS; i++)
   {
      struct remote *r = &remotes[i];
      if (!r->st.connected)
         continue;
      if (want_drop[i])
      {
         want_drop[i] = 0;
         bt_disconnect(i);
      }
      if (r->rumble != want_rumble[i])
      {
         r->rumble = want_rumble[i];
         out1(i, OUT_RUMBLE, 0);
      }
      /* An extension that stopped answering: ask again. */
      if (r->step != STEP_IDLE
            && now - r->step_at > GK_US_TO_TICKS(EXT_RETRY_US))
      {
         step(i, STEP_IDLE);
         out1(i, OUT_STATUS, 0);
      }
   }
}

static const bt_hid_ops hid_ops =
{
   bt_connected, bt_disconnected, bt_input, bt_tick
};

int gk_wiimote_init(void)
{
   return bt_start(&hid_ops);
}

int gk_wiimote_read(unsigned slot, gk_wiimote_t *st)
{
   if (slot >= GK_WIIMOTE_SLOTS || slot >= BT_MAX_LINKS)
   {
      memset(st, 0, sizeof(*st));
      return -1;
   }
   gk_mutex_lock(&state_lock);
   *st = remotes[slot].st;
   gk_mutex_unlock(&state_lock);
   return st->connected ? 0 : -1;
}

void gk_wiimote_rumble(unsigned slot, int on)
{
   if (slot < BT_MAX_LINKS && want_rumble[slot] != (on != 0))
   {
      want_rumble[slot] = (uint8_t)(on != 0);
      bt_wake();
   }
}

void gk_wiimote_disconnect(unsigned slot)
{
   if (slot < BT_MAX_LINKS)
   {
      want_drop[slot] = 1;
      bt_wake();
   }
}
