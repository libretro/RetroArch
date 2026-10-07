/* Wii: a Bluetooth host for HID devices, after the Bluetooth Core
 * Specification's HCI (volume 4 part E) and L2CAP (volume 3 part A),
 * over IOS's USB interface to the controller, /dev/usb/oh1/57e/305:
 * commands as control transfers, events from interrupt endpoint 0x81,
 * ACL data in from bulk endpoint 0x82 and out to 0x02.
 *
 * The controller is left page-scanning, so devices it already knows
 * (the remotes synced to the console) connect themselves: the host
 * accepts, answers the link key request from the keys the controller
 * keeps, and accepts the device's HID control and interrupt channels
 * (PSMs 0x11 and 0x13). */

#include <errno.h>
#include <string.h>

#include <gekko/ios.h>
#include <gekko/thread.h>

#include "bt.h"
#include "rvl.h"
#include "../kernel/kernel.h"

#define USB_CTRL 0
#define USB_BULK 1
#define USB_INTR 2

#define EP_EVENT   0x81
#define EP_ACL_IN  0x82
#define EP_ACL_OUT 0x02

#define EVENT_BUF  288
#define ACL_BUF    1056
#define CMD_BUF    288

/* HCI commands: OGF << 10 | OCF. */
#define HCI_ACCEPT_CON      0x0409
#define HCI_REJECT_CON      0x040a
#define HCI_LINK_KEY_REP    0x040b
#define HCI_LINK_KEY_NEG    0x040c
#define HCI_DISCONNECT      0x0406
#define HCI_RESET           0x0c03
#define HCI_READ_STORED_KEY 0x0c0d
#define HCI_WRITE_PAGE_TO   0x0c18
#define HCI_WRITE_SCAN      0x0c1a
#define HCI_READ_BUFFER     0x1005
#define HCI_READ_BDADDR     0x1009

/* HCI events. */
#define EV_CON_COMPLETE     0x03
#define EV_CON_REQUEST      0x04
#define EV_DISCON_COMPLETE  0x05
#define EV_CMD_COMPLETE     0x0e
#define EV_CMD_STATUS       0x0f
#define EV_COMPLETED_PKTS   0x13
#define EV_RETURN_KEYS      0x15
#define EV_LINK_KEY_REQ     0x17
#define EV_LINK_KEY_NOTIFY  0x18

/* L2CAP signalling. */
#define SIG_CID           0x0001
#define SIG_REJECT        0x01
#define SIG_CON_REQ       0x02
#define SIG_CON_RSP       0x03
#define SIG_CFG_REQ       0x04
#define SIG_CFG_RSP       0x05
#define SIG_DISCON_REQ    0x06
#define SIG_DISCON_RSP    0x07
#define SIG_INFO_REQ      0x0a
#define SIG_INFO_RSP      0x0b

#define PSM_HID_CONTROL   0x11
#define PSM_HID_INTERRUPT 0x13

#define MAX_KEYS   16
#define CMD_QUEUE  16
#define TX_QUEUE   16
#define TX_MAX     96

/* A channel: our cid, the device's, and the two configurations. */
struct channel
{
   uint16_t local;
   uint16_t remote;
   uint8_t  open;
   uint8_t  ours_done;     /* the device accepted our configuration */
   uint8_t  theirs_done;   /* we accepted the device's */
};

struct link
{
   uint8_t        bdaddr[6];
   uint16_t       handle;
   uint16_t       rx_len;
   uint8_t        used;
   uint8_t        up;           /* both channels configured */
   struct channel ch[2];        /* control, interrupt */
   uint8_t        rx[1024];     /* L2CAP reassembly */
};

struct key
{
   uint8_t bdaddr[6];
   uint8_t key[16];
};

struct cmd
{
   uint16_t opcode;
   uint8_t  len;
   uint8_t  params[32];
};

struct tx
{
   uint16_t handle;
   uint16_t len;
   uint8_t  data[TX_MAX];
};

static const bt_hid_ops *ops;
static gk_thread_t      *thread;
static gk_mutex_t        start_lock = GK_MUTEX_INIT;
static int32_t           fd = -1;

/* Asynchronous reads: their buffers, and what came back. */
static uint8_t          *ev_buf, *acl_buf, *ev_par, *acl_par;
static uint8_t          *ctl_par, *ctl_buf, *out_par, *out_buf;
static volatile int32_t  ev_result, acl_result;
static volatile uint32_t ev_ready, acl_ready;
static volatile uint32_t wake_seq;
static volatile uint32_t ev_out, acl_out;   /* transfers IOS holds */
static volatile uint32_t stopping;

static struct link       links[BT_MAX_LINKS];
static struct key        keys[MAX_KEYS];
static unsigned          num_keys;
static struct cmd        cmds[CMD_QUEUE];
static unsigned          cmd_head, cmd_count, cmd_credits = 1;
static struct tx         txq[TX_QUEUE];
static unsigned          tx_head, tx_count, acl_credits;
static uint8_t           sig_ident;

/* ---- little-endian fields ---- */

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void put16(uint8_t *p, uint16_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

/* ---- the USB side ---- */

static void ev_done(int32_t result, void *data)
{
   (void)data;
   ev_result = result;
   ev_out    = 0;
   ev_ready  = 1;
   wake_seq++;
   gk_futex_wake(&wake_seq, 1);
}

static void acl_done(int32_t result, void *data)
{
   (void)data;
   acl_result = result;
   acl_out    = 0;
   acl_ready  = 1;
   wake_seq++;
   gk_futex_wake(&wake_seq, 1);
}

/* An interrupt or bulk transfer's two parameter vectors: the endpoint
 * at 0, the length (big-endian) at 32. */
static void data_vecs(gk_ios_vec_t *vec, uint8_t *par, uint8_t ep,
      void *buf, uint16_t len)
{
   par[0]  = ep;
   par[32] = (uint8_t)(len >> 8);
   par[33] = (uint8_t)len;
   vec[0].data = par;
   vec[0].len  = 1;
   vec[1].data = par + 32;
   vec[1].len  = 2;
   vec[2].data = buf;
   vec[2].len  = len;
}

static void arm_event(void)
{
   gk_ios_vec_t vec[3];
   data_vecs(vec, ev_par, EP_EVENT, ev_buf, EVENT_BUF);
   ev_out = 1;
   if (gk_ios_ioctlv_async(fd, USB_INTR, 2, 1, vec, ev_done, NULL))
      ev_out = 0;
}

static void arm_acl(void)
{
   gk_ios_vec_t vec[3];
   data_vecs(vec, acl_par, EP_ACL_IN, acl_buf, ACL_BUF);
   acl_out = 1;
   if (gk_ios_ioctlv_async(fd, USB_BULK, 2, 1, vec, acl_done, NULL))
      acl_out = 0;
}

/* An HCI command: a class request to the device, the packet as data.
 * The parameters are the request's fields, one vector each. */
static int send_command(const struct cmd *c)
{
   gk_ios_vec_t vec[7];
   unsigned len = 3u + c->len;
   ctl_par[0]   = 0x20;   /* host to device, class, device */
   ctl_par[32]  = 0;
   put16(ctl_par + 64, 0);
   put16(ctl_par + 96, 0);
   put16(ctl_par + 128, (uint16_t)len);
   ctl_par[160] = 0;
   put16(ctl_buf, c->opcode);
   ctl_buf[2] = c->len;
   memcpy(ctl_buf + 3, c->params, c->len);
   vec[0].data = ctl_par;       vec[0].len = 1;
   vec[1].data = ctl_par + 32;  vec[1].len = 1;
   vec[2].data = ctl_par + 64;  vec[2].len = 2;
   vec[3].data = ctl_par + 96;  vec[3].len = 2;
   vec[4].data = ctl_par + 128; vec[4].len = 2;
   vec[5].data = ctl_par + 160; vec[5].len = 1;
   vec[6].data = ctl_buf;       vec[6].len = len;
   return gk_ios_ioctlv(fd, USB_CTRL, 6, 1, vec) < 0 ? -1 : 0;
}

static int send_acl_now(const struct tx *t)
{
   gk_ios_vec_t vec[3];
   put16(out_buf, (uint16_t)(t->handle | 0x2000));   /* first, point to point */
   put16(out_buf + 2, t->len);
   memcpy(out_buf + 4, t->data, t->len);
   data_vecs(vec, out_par, EP_ACL_OUT, out_buf, (uint16_t)(t->len + 4));
   return gk_ios_ioctlv(fd, USB_BULK, 2, 1, vec) < 0 ? -1 : 0;
}

/* ---- queues ---- */

static void command(uint16_t opcode, const void *params, unsigned len)
{
   struct cmd *c;
   if (cmd_count == CMD_QUEUE || len > sizeof(c->params))
      return;
   c = &cmds[(cmd_head + cmd_count++) % CMD_QUEUE];
   c->opcode = opcode;
   c->len    = (uint8_t)len;
   memcpy(c->params, params, len);
}

static void run_commands(void)
{
   while (cmd_count && cmd_credits)
   {
      cmd_credits--;
      send_command(&cmds[cmd_head]);
      cmd_head = (cmd_head + 1) % CMD_QUEUE;
      cmd_count--;
   }
}

/* An L2CAP frame to a link: the basic header, then payload. */
static int l2cap_send(struct link *l, uint16_t cid, const uint8_t *data,
      unsigned len)
{
   struct tx *t;
   if (tx_count == TX_QUEUE || len + 4 > TX_MAX)
      return -1;
   t = &txq[(tx_head + tx_count++) % TX_QUEUE];
   t->handle = l->handle;
   t->len    = (uint16_t)(len + 4);
   put16(t->data, (uint16_t)len);
   put16(t->data + 2, cid);
   memcpy(t->data + 4, data, len);
   return 0;
}

static void run_tx(void)
{
   /* The controller has acl_credits buffers; each completed packet
    * gives one back. */
   while (tx_count && acl_credits)
   {
      acl_credits--;
      send_acl_now(&txq[tx_head]);
      tx_head = (tx_head + 1) % TX_QUEUE;
      tx_count--;
   }
}

static void sig_send(struct link *l, uint8_t code, uint8_t ident,
      const uint8_t *data, unsigned len)
{
   uint8_t b[64];
   b[0] = code;
   b[1] = ident;
   put16(b + 2, (uint16_t)len);
   memcpy(b + 4, data, len);
   l2cap_send(l, SIG_CID, b, len + 4);
}

/* ---- links and channels ---- */

static struct link *by_handle(uint16_t handle)
{
   unsigned i;
   for (i = 0; i < BT_MAX_LINKS; i++)
      if (links[i].used && links[i].handle == handle)
         return &links[i];
   return NULL;
}

static struct channel *by_local(struct link *l, uint16_t cid)
{
   unsigned i;
   for (i = 0; i < 2; i++)
      if (l->ch[i].open && l->ch[i].local == cid)
         return &l->ch[i];
   return NULL;
}

static void check_up(struct link *l)
{
   unsigned i;
   for (i = 0; i < 2; i++)
      if (!l->ch[i].open || !l->ch[i].ours_done || !l->ch[i].theirs_done)
         return;
   if (!l->up)
   {
      l->up = 1;
      ops->connected((unsigned)(l - links));
   }
}

static void link_down(struct link *l)
{
   if (l->up && ops)
      ops->disconnected((unsigned)(l - links));
   memset(l, 0, sizeof(*l));
}

static void signalling(struct link *l, const uint8_t *p, unsigned len)
{
   while (len >= 4)
   {
      uint8_t code = p[0], ident = p[1], r[16];
      unsigned n = le16(p + 2);
      const uint8_t *d = p + 4;
      if (n + 4 > len)
         return;
      switch (code)
      {
         case SIG_CON_REQ:
         {
            uint16_t psm = le16(d), scid = le16(d + 2);
            int i = psm == PSM_HID_CONTROL ? 0
               : psm == PSM_HID_INTERRUPT ? 1 : -1;
            struct channel *c = i >= 0 ? &l->ch[i] : NULL;
            put16(r + 2, scid);
            put16(r + 6, 0);
            if (!c || c->open)
            {
               put16(r, 0);
               put16(r + 4, c ? 0x0004 : 0x0002);   /* no resources; bad PSM */
               sig_send(l, SIG_CON_RSP, ident, r, 8);
               break;
            }
            memset(c, 0, sizeof(*c));
            c->open   = 1;
            c->local  = (uint16_t)(0x40 + i);
            c->remote = scid;
            put16(r, c->local);
            put16(r + 4, 0);
            sig_send(l, SIG_CON_RSP, ident, r, 8);
            /* Our configuration: the defaults, so no options. */
            put16(r, c->remote);
            put16(r + 2, 0);
            if (!++sig_ident)
               sig_ident = 1;
            sig_send(l, SIG_CFG_REQ, sig_ident, r, 4);
            break;
         }
         case SIG_CFG_REQ:
         {
            struct channel *c = n >= 4 ? by_local(l, le16(d)) : NULL;
            if (!c)
            {
               put16(r, 0x0002);   /* invalid CID */
               put16(r + 2, le16(d));
               put16(r + 4, 0);
               sig_send(l, SIG_REJECT, ident, r, 6);
               break;
            }
            /* Whatever MTU and flush timeout it asks for suit us. */
            put16(r, c->remote);
            put16(r + 2, 0);
            put16(r + 4, 0);
            sig_send(l, SIG_CFG_RSP, ident, r, 6);
            if (!(le16(d + 2) & 1))   /* no continuation */
               c->theirs_done = 1;
            check_up(l);
            break;
         }
         case SIG_CFG_RSP:
         {
            struct channel *c = n >= 6 ? by_local(l, le16(d)) : NULL;
            if (c && le16(d + 4) == 0)
               c->ours_done = 1;
            check_up(l);
            break;
         }
         case SIG_DISCON_REQ:
         {
            struct channel *c = n >= 4 ? by_local(l, le16(d)) : NULL;
            put16(r, le16(d));
            put16(r + 2, le16(d + 2));
            sig_send(l, SIG_DISCON_RSP, ident, r, 4);
            if (c)
            {
               if (l->up)
               {
                  l->up = 0;
                  ops->disconnected((unsigned)(l - links));
               }
               c->open = 0;
            }
            break;
         }
         case SIG_INFO_REQ:
            put16(r, le16(d));
            put16(r + 2, 0x0001);   /* not supported */
            sig_send(l, SIG_INFO_RSP, ident, r, 4);
            break;
         case SIG_CON_RSP:
         case SIG_DISCON_RSP:
         case SIG_REJECT:
         case SIG_INFO_RSP:
            break;
         default:
            put16(r, 0);   /* not understood */
            sig_send(l, SIG_REJECT, ident, r, 2);
            break;
      }
      p   += n + 4;
      len -= n + 4;
   }
}

static void l2cap_frame(struct link *l, const uint8_t *p, unsigned len)
{
   uint16_t cid = le16(p + 2);
   struct channel *c;
   p   += 4;
   len -= 4;
   if (cid == SIG_CID)
      signalling(l, p, len);
   else if ((c = by_local(l, cid)) && c == &l->ch[1] && l->up)
      ops->input((unsigned)(l - links), p, len);
}

static void acl_in(const uint8_t *p, unsigned len)
{
   struct link *l;
   uint16_t hdr, n;
   if (len < 4)
      return;
   hdr = le16(p);
   n   = le16(p + 2);
   if (n + 4u > len || !(l = by_handle(hdr & 0x0fff)))
      return;
   p += 4;
   if (((hdr >> 12) & 3) != 1)
      l->rx_len = 0;   /* a first fragment */
   if (l->rx_len + n > sizeof(l->rx))
   {
      l->rx_len = 0;
      return;
   }
   memcpy(l->rx + l->rx_len, p, n);
   l->rx_len = (uint16_t)(l->rx_len + n);
   if (l->rx_len >= 4 && l->rx_len >= le16(l->rx) + 4u)
   {
      l2cap_frame(l, l->rx, le16(l->rx) + 4u);
      l->rx_len = 0;
   }
}

/* ---- HCI events ---- */

static const struct key *find_key(const uint8_t *bdaddr)
{
   unsigned i;
   for (i = 0; i < num_keys; i++)
      if (!memcmp(keys[i].bdaddr, bdaddr, 6))
         return &keys[i];
   return NULL;
}

static void store_key(const uint8_t *bdaddr, const uint8_t *key)
{
   struct key *k = (struct key*)find_key(bdaddr);
   if (!k && num_keys < MAX_KEYS)
      k = &keys[num_keys++];
   if (k)
   {
      memcpy(k->bdaddr, bdaddr, 6);
      memcpy(k->key, key, 16);
   }
}

static void event(const uint8_t *p, unsigned len)
{
   const uint8_t *d = p + 2;
   uint8_t r[32];
   unsigned i;
   if (len < 2 || p[1] + 2u > len)
      return;
   switch (p[0])
   {
      case EV_CMD_COMPLETE:
         cmd_credits = d[0];
         if (le16(d + 1) == HCI_READ_BUFFER && !d[3])
            acl_credits = le16(d + 7);
         break;
      case EV_CMD_STATUS:
         cmd_credits = d[1];
         break;
      case EV_RETURN_KEYS:
         for (i = 0; i < d[0] && 1 + (i + 1) * 22u <= p[1]; i++)
            store_key(d + 1 + i * 22, d + 1 + i * 22 + 6);
         break;
      case EV_LINK_KEY_NOTIFY:
         store_key(d, d + 6);
         break;
      case EV_CON_REQUEST:
      {
         int free_link = 0;
         for (i = 0; i < BT_MAX_LINKS; i++)
            free_link |= !links[i].used;
         memcpy(r, d, 6);
         if (d[9] == 1 && free_link)
         {
            r[6] = 0x00;   /* we become master */
            command(HCI_ACCEPT_CON, r, 7);
         }
         else
         {
            r[6] = 0x0d;   /* limited resources */
            command(HCI_REJECT_CON, r, 7);
         }
         break;
      }
      case EV_LINK_KEY_REQ:
      {
         const struct key *k = find_key(d);
         memcpy(r, d, 6);
         if (k)
         {
            memcpy(r + 6, k->key, 16);
            command(HCI_LINK_KEY_REP, r, 22);
         }
         else
            command(HCI_LINK_KEY_NEG, r, 6);
         break;
      }
      case EV_CON_COMPLETE:
         if (!d[0] && d[9] == 1)
            for (i = 0; i < BT_MAX_LINKS; i++)
               if (!links[i].used)
               {
                  memset(&links[i], 0, sizeof(links[i]));
                  links[i].used   = 1;
                  links[i].handle = le16(d + 1) & 0x0fff;
                  memcpy(links[i].bdaddr, d + 3, 6);
                  break;
               }
         break;
      case EV_DISCON_COMPLETE:
      {
         struct link *l = by_handle(le16(d + 1) & 0x0fff);
         if (!d[0] && l)
         {
            unsigned k;
            /* Its queued packets go nowhere now. */
            for (k = 0; k < tx_count; k++)
               if (txq[(tx_head + k) % TX_QUEUE].handle == l->handle)
                  txq[(tx_head + k) % TX_QUEUE].len = 0;
            link_down(l);
         }
         break;
      }
      case EV_COMPLETED_PKTS:
         for (i = 0; i < d[0] && 1 + (i + 1) * 4u <= p[1]; i++)
            acl_credits += le16(d + 1 + i * 4 + 2);
         break;
   }
}

/* ---- the thread ---- */

static void *bt_thread(void *arg)
{
   static const uint8_t all_keys[7] = { 0, 0, 0, 0, 0, 0, 1 };
   static const uint8_t page_to[2]  = { 0x00, 0x20 };
   static const uint8_t page_scan[1] = { 0x02 };
   unsigned i;
   (void)arg;

   command(HCI_RESET, NULL, 0);
   command(HCI_READ_BUFFER, NULL, 0);
   command(HCI_READ_BDADDR, NULL, 0);
   command(HCI_READ_STORED_KEY, all_keys, 7);
   command(HCI_WRITE_PAGE_TO, page_to, 2);
   command(HCI_WRITE_SCAN, page_scan, 1);
   arm_event();
   arm_acl();

   while (!stopping)
   {
      uint32_t seq = wake_seq;
      if (ev_ready)
      {
         ev_ready = 0;
         if (ev_result > 0)
            event(ev_buf, (unsigned)ev_result);
         arm_event();
      }
      if (acl_ready)
      {
         acl_ready = 0;
         if (acl_result > 0)
            acl_in(acl_buf, (unsigned)acl_result);
         arm_acl();
      }
      run_commands();
      /* Dropped packets (len 0) leave the queue without a credit. */
      while (tx_count && !txq[tx_head].len)
      {
         tx_head = (tx_head + 1) % TX_QUEUE;
         tx_count--;
      }
      run_tx();
      if (ops->tick)
         ops->tick();
      run_tx();
      if (wake_seq == seq)
         gk_futex_wait(&wake_seq, seq, GK_US_TO_TICKS(10000));
   }

   /* IOS answers the transfers it holds when the device closes, or
    * drops them; give it a moment either way. */
   gk_ios_close(fd);
   fd = -1;
   for (i = 0; i < 10 && (ev_out || acl_out); i++)
      gk_sleep_us(10000);
   return NULL;
}

static void bt_stop(void)
{
   stopping = 1;
   bt_wake();
   gk_thread_join(thread);
}

static struct gk_exit_hook stop_hook = { bt_stop, NULL };

int bt_start(const bt_hid_ops *hid)
{
   int ret = 0;
   gk_mutex_lock(&start_lock);
   if (thread)
      goto out;
   ev_buf  = (uint8_t*)gk_iobuf_get(EVENT_BUF);
   acl_buf = (uint8_t*)gk_iobuf_get(ACL_BUF);
   ev_par  = (uint8_t*)gk_iobuf_get(64);
   acl_par = (uint8_t*)gk_iobuf_get(64);
   ctl_par = (uint8_t*)gk_iobuf_get(192);
   ctl_buf = (uint8_t*)gk_iobuf_get(CMD_BUF);
   out_par = (uint8_t*)gk_iobuf_get(64);
   out_buf = (uint8_t*)gk_iobuf_get(TX_MAX + 32);
   if (!ev_buf || !acl_buf || !ev_par || !acl_par || !ctl_par || !ctl_buf
         || !out_par || !out_buf)
   {
      ret = -ENOMEM;
      goto out;
   }
   if ((fd = gk_ios_open("/dev/usb/oh1/57e/305", 0)) < 0)
   {
      ret = -ENODEV;
      goto out;
   }
   ops = hid;
   if (!(thread = gk_thread_create(bt_thread, NULL, NULL, 16384,
               GK_PRIO_DEFAULT + 8)))
   {
      gk_ios_close(fd);
      fd  = -1;
      ret = -ENOMEM;
   }
   else
      gk_exit_hook_add(&stop_hook);
out:
   gk_mutex_unlock(&start_lock);
   return ret;
}

int bt_send(unsigned link, const uint8_t *data, unsigned len)
{
   if (link >= BT_MAX_LINKS || !links[link].up)
      return -1;
   return l2cap_send(&links[link], links[link].ch[1].remote, data, len);
}

void bt_disconnect(unsigned link)
{
   uint8_t r[3];
   if (link >= BT_MAX_LINKS || !links[link].used)
      return;
   put16(r, links[link].handle);
   r[2] = 0x13;   /* remote user terminated */
   command(HCI_DISCONNECT, r, 3);
}

void bt_wake(void)
{
   wake_seq++;
   gk_futex_wake(&wake_seq, 1);
}
