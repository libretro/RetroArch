/* Wii: Wii Remotes on the Bluetooth stack, after WiiBrew's description
 * of the remote's HID reports and IR camera.
 *
 * A remote that connects gets its slot's LED, its IR camera turned on
 * and configured, and a status request.  The status says whether an
 * extension is plugged in; one that is gets initialised unencrypted
 * (0x55 to 0xa400f0, 0x00 to 0xa400fb) and its identifier read from
 * 0xa400fa.  A status report arrives by itself when an extension comes
 * or goes, and starts that over.  The remote reports continuously in
 * mode 0x37: buttons, accelerometer, the camera's basic dots and the
 * extension's bytes, whether there is an extension or not.
 *
 * Writes to the remote's registers and reads from them go one at a
 * time, each when the remote has answered the last.
 * ir.c turns the camera's dots into the pointer. */

#include <string.h>

#include <gekko/conf.h>
#include <gekko/thread.h>
#include <gekko/wiimote.h>

#include "bt.h"
#include "ir.h"

#define OUT_RUMBLE      0x10
#define OUT_LEDS        0x11
#define OUT_MODE        0x12
#define OUT_IR_CLOCK    0x13
#define OUT_STATUS      0x15
#define OUT_WRITE       0x16
#define OUT_READ        0x17
#define OUT_IR_LOGIC    0x1a

#define IN_STATUS       0x20
#define IN_READ         0x21
#define IN_ACK          0x22

#define MODE_ACCEL_IR_EXT 0x37
#define CONTINUOUS      0x04

#define MEM_RETRY_US    1000000

/* The sensor bar's power, a GPIO the processor drives. */
#define HW_GPIOB_OUT    0xcd0000c0u
#define HW_GPIOB_DIR    0xcd0000c4u
#define GPIO_SENSOR_BAR 0x100u
#define MEM_QUEUE       12

/* The camera's sensitivity blocks for the system's settings 1 to 5. */
static const uint8_t ir_block1[5][9] = {
   { 0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x64, 0x00, 0xfe },
   { 0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x96, 0x00, 0xb4 },
   { 0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64 },
   { 0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xc8, 0x00, 0x36 },
   { 0x07, 0x00, 0x00, 0x71, 0x01, 0x00, 0x72, 0x00, 0x20 }
};
static const uint8_t ir_block2[5][2] = {
   { 0xfd, 0x05 }, { 0xb3, 0x04 }, { 0x63, 0x03 }, { 0x35, 0x03 },
   { 0x1f, 0x03 }
};

struct memop
{
   uint32_t addr;
   uint8_t  len;
   uint8_t  read;
   uint8_t  data[16];
};

struct remote
{
   gk_wiimote_t st;
   struct memop q[MEM_QUEUE];
   uint64_t     sent_at;
   ir_track     ir;
   uint8_t      q_head;
   uint8_t      q_count;
   uint8_t      busy;        /* the head is out */
   uint8_t      ext_pending; /* the extension's identifier asked for */
   uint8_t      rumble;
   uint8_t      whammy_lo;   /* the guitar's whammy bar, as far as */
   uint8_t      whammy_hi;   /* it has gone either way */
};

static struct remote     remotes[BT_MAX_LINKS];
static volatile uint8_t  want_rumble[BT_MAX_LINKS];
static volatile uint8_t  want_drop[BT_MAX_LINKS];
static gk_mutex_t        state_lock = GK_MUTEX_INIT;
static int               bar_on_top = -1;
static int               sensitivity = -1;

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
   p[1] = MODE_ACCEL_IR_EXT;
   out(link, OUT_MODE, p, 2);
}

/* ---- register writes and reads, one at a time ---- */

static void mem_pump(unsigned link)
{
   struct remote *r = &remotes[link];
   struct memop  *m;
   uint8_t p[21];
   if (r->busy || !r->q_count)
      return;
   m = &r->q[r->q_head];
   memset(p, 0, sizeof(p));
   p[0] = 0x04;   /* registers, not EEPROM */
   p[1] = (uint8_t)(m->addr >> 16);
   p[2] = (uint8_t)(m->addr >> 8);
   p[3] = (uint8_t)m->addr;
   if (m->read)
   {
      p[4] = 0;
      p[5] = m->len;
      out(link, OUT_READ, p, 6);
   }
   else
   {
      p[4] = m->len;
      memcpy(p + 5, m->data, m->len);
      out(link, OUT_WRITE, p, sizeof(p));
   }
   r->busy    = 1;
   r->sent_at = gk_ticks();
}

static void mem_push(unsigned link, uint32_t addr, const uint8_t *data,
      unsigned len, int read)
{
   struct remote *r = &remotes[link];
   struct memop  *m;
   if (r->q_count == MEM_QUEUE || len > 16)
      return;
   m       = &r->q[(r->q_head + r->q_count++) % MEM_QUEUE];
   m->addr = addr;
   m->len  = (uint8_t)len;
   m->read = (uint8_t)read;
   if (data)
      memcpy(m->data, data, len);
   mem_pump(link);
}

static void mem_write1(unsigned link, uint32_t addr, uint8_t v)
{
   mem_push(link, addr, &v, 1, 0);
}

/* The head has been answered: the next goes out. */
static void mem_done(unsigned link)
{
   struct remote *r = &remotes[link];
   r->q_head  = (uint8_t)((r->q_head + 1) % MEM_QUEUE);
   r->q_count--;
   r->busy    = 0;
   mem_pump(link);
}

static void ext_start(unsigned link)
{
   remotes[link].ext_pending = 1;
   mem_write1(link, 0xa400f0, 0x55);
   mem_write1(link, 0xa400fb, 0x00);
   mem_push(link, 0xa400fa, NULL, 6, 1);
}

/* The camera on, its sensitivity, the basic (10-byte) dot format. */
static void ir_start(unsigned link)
{
   int sens = sensitivity >= 1 && sensitivity <= 5 ? sensitivity - 1 : 2;
   out1(link, OUT_IR_CLOCK, 0x04);
   out1(link, OUT_IR_LOGIC, 0x04);
   mem_write1(link, 0xb00030, 0x08);
   mem_push(link, 0xb00000, ir_block1[sens], 9, 0);
   mem_push(link, 0xb0001a, ir_block2[sens], 2, 0);
   mem_write1(link, 0xb00033, 0x01);
   mem_write1(link, 0xb00030, 0x08);
}

/* ---- input reports ---- */

static int8_t clamp8(int v)
{
   return (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
}

static void decode_ext(struct remote *r, const uint8_t *e)
{
   gk_wiimote_t *st = &r->st;
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
   else if (st->ext == GK_WM_EXT_GUITAR)
   {
      /* The Classic Controller's layout: its left stick, and buttons
       * where the Classic has theirs.  The whammy bar rests somewhere
       * past 0; its travel is what it has been seen to cover. */
      int w = e[3] & 0x1f;
      st->stick[0][0] = clamp8(((e[0] & 0x3f) - 32) * 4);
      st->stick[0][1] = clamp8(((e[1] & 0x3f) - 32) * 4);
      if (w < r->whammy_lo)
         r->whammy_lo = (uint8_t)w;
      if (w > r->whammy_hi)
         r->whammy_hi = (uint8_t)w;
      st->trigger[1]  = (uint8_t)(r->whammy_hi - r->whammy_lo < 4 ? 0
            : (w - r->whammy_lo) * 255 / (r->whammy_hi - r->whammy_lo));
      st->ext_buttons = (uint16_t)(~((e[4] << 8) | e[5]) & GK_GH_ALL);
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
            if (present && !r->ext_pending)
            {
               st->ext = GK_WM_EXT_NONE;
               ext_start(link);
            }
            else if (!present)
            {
               st->ext = GK_WM_EXT_NONE;
               st->ext_buttons = 0;
               memset(st->stick, 0, sizeof(st->stick));
               memset(st->trigger, 0, sizeof(st->trigger));
            }
            /* A status report stops the data reports until the mode is
             * set again. */
            set_mode(link);
         }
         break;
      case IN_ACK:
         if (len >= 4 && p[2] == OUT_WRITE && r->busy
               && !r->q[r->q_head].read)
            mem_done(link);
         break;
      case IN_READ:
         if (len >= 21 && r->busy && r->q[r->q_head].read)
         {
            const uint8_t *idb = p + 5;
            if (r->q[r->q_head].addr == 0xa400fa)
            {
               if (p[2] & 0x0f)
                  st->ext = GK_WM_EXT_OTHER;
               else if (idb[2] == 0xa4 && idb[3] == 0x20 && idb[4] == 0
                     && idb[5] == 0)
                  st->ext = GK_WM_EXT_NUNCHUK;
               else if (idb[2] == 0xa4 && idb[3] == 0x20 && idb[4] == 1
                     && idb[5] == 1)
                  st->ext = GK_WM_EXT_CLASSIC;
               else if (idb[0] == 0 && idb[2] == 0xa4 && idb[3] == 0x20
                     && idb[4] == 1 && idb[5] == 3)
               {
                  st->ext      = GK_WM_EXT_GUITAR;
                  r->whammy_lo = 0x1f;
                  r->whammy_hi = 0;
               }
               else
                  st->ext = GK_WM_EXT_OTHER;
               r->ext_pending = 0;
               set_mode(link);
            }
            mem_done(link);
         }
         break;
      case MODE_ACCEL_IR_EXT:
         if (len >= 21)
         {
            st->accel[0] = (uint16_t)((p[2] << 2) | ((p[0] >> 5) & 3));
            st->accel[1] = (uint16_t)((p[3] << 2) | ((p[1] >> 4) & 2));
            st->accel[2] = (uint16_t)((p[4] << 2) | ((p[1] >> 5) & 2));
            ir_pointer(&r->ir, p + 5, bar_on_top, &st->ir_x, &st->ir_y,
                  &st->ir_dots, &st->ir_valid);
            if (st->ext != GK_WM_EXT_NONE)
               decode_ext(r, p + 15);
         }
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
   ir_start(link);
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
      /* A remote that stopped answering its register commands:
       * start them over. */
      if (r->busy && now - r->sent_at > GK_US_TO_TICKS(MEM_RETRY_US))
      {
         gk_mutex_lock(&state_lock);
         r->q_count     = 0;
         r->busy        = 0;
         r->ext_pending = 0;
         ir_start(i);
         out1(i, OUT_STATUS, 0);
         gk_mutex_unlock(&state_lock);
      }
   }
}

static const bt_hid_ops hid_ops =
{
   bt_connected, bt_disconnected, bt_input, bt_tick
};

int gk_wiimote_init(void)
{
   if (bar_on_top < 0)
   {
      int v       = gk_conf_sensor_bar_top();
      bar_on_top  = v > 0;
      sensitivity = gk_conf_ir_sensitivity();
   }
   GK_REG32(HW_GPIOB_DIR) |= GPIO_SENSOR_BAR;
   GK_REG32(HW_GPIOB_OUT) |= GPIO_SENSOR_BAR;
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
