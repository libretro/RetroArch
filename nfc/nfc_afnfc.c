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

/* AF_NFC hardware backend (Linux).
 *
 * Driver-agnostic: it uses only the kernel NFC interfaces -- the "nfc"
 * generic-netlink family and AF_NFC raw sockets, as neard does -- so it
 * works with any adapter on the Linux NFC stack. It talks to the kernel
 * directly and does NOT require (or start) neard; RetroArch drives polling
 * itself. If neard is already running it may contend for the adapter --
 * that is a deployment choice left to the user, not a build dependency.
 *
 * Tag commands are standard by default: NTAG/Type 2 READ 0x30, FAST_READ
 * 0x3A (falls back to READ on tags without it) and WRITE 0xA2 (verified by
 * read-back), and Mifare Classic as the PN53x-style AUTH (0x60/0x61 + key
 * + UID) followed by READ 0x30 / WRITE 0xA0. Drivers with their own
 * command extensions are detected from sysfs and use them as a fast path,
 * still falling back to the standard commands (see afnfc_dialect).
 *
 * Flow (see net/nfc and the nfc uapi):
 *   CTRL_CMD_GETFAMILY("nfc") + join the "events" multicast group
 *   NFC_CMD_GET_DEVICE (dump)         -> pick an adapter (RETRO_NFC_DEV)
 *   NFC_CMD_DEV_UP / NFC_CMD_START_POLL(IM_PROTOCOLS)
 *   NFC_EVENT_TARGETS_FOUND           -> NFC_CMD_GET_TARGET (dump, drained
 *                                        fully to release the device ref)
 *   connect AF_NFC SOCK_SEQPACKET / NFC_SOCKPROTO_RAW to the target
 *   transceive ISO14443-3 (see above for the command set)
 *   NFC_EVENT_TARGET_LOST / DEVICE_REMOVED -> drop
 *
 * The kernel's raw socket (net/nfc/rawsock.c) prepends one status byte to
 * each response; that header is stripped here generically.
 *
 * Overrides: RETRO_NFC_DEV = adapter index (default: first enumerated);
 * RETRO_NFC_AFNFC_DIALECT = standard | pn53x | nintendo (default: detect).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#ifndef NETLINK_NO_ENOBUFS
#define NETLINK_NO_ENOBUFS 5
#endif

#include <libretro.h>

#include <compat/strl.h>

#include "nfc_backend.h"
#include "../verbosity.h"

/* ---- NFC uapi constants (include/uapi/linux/nfc.h), inlined so the
 * backend has no build dependency beyond netlink. ------------------- */
#define NFC_GENL_NAME       "nfc"
#define NFC_GENL_MCAST      "events"

enum {
   NFC_CMD_GET_DEVICE       = 1,
   NFC_CMD_DEV_UP           = 2,
   NFC_CMD_DEV_DOWN         = 3,
   NFC_CMD_START_POLL       = 6,
   NFC_CMD_STOP_POLL        = 7,
   NFC_CMD_GET_TARGET       = 8,
   NFC_EVENT_TARGETS_FOUND  = 9,
   NFC_EVENT_DEVICE_REMOVED = 11,
   NFC_EVENT_TARGET_LOST    = 12
};
enum {
   NFC_ATTR_DEVICE_INDEX    = 1,
   NFC_ATTR_PROTOCOLS       = 3,
   NFC_ATTR_TARGET_INDEX    = 4,
   NFC_ATTR_TARGET_NFCID1   = 7,
   NFC_ATTR_IM_PROTOCOLS    = 13
};

#define NFC_PROTO_JEWEL     1
#define NFC_PROTO_MIFARE    2
#define NFC_PROTO_ISO14443  4

/* Poll mask. NTAG21x / amiibo are Type 2 (ISO14443-3A), which this stack
 * classes as NFC_PROTO_MIFARE -- the exact value nfc-probe polls with and
 * the tera driver expects. Overridable for other stacks via RETRO_NFC_PROTO_MASK. */
#define AFNFC_POLL_MASK  (1u << NFC_PROTO_MIFARE)
/* Connect protocol for NTAG/Type 2 tags. */
#define AFNFC_CONN_PROTO NFC_PROTO_MIFARE

#ifndef AF_NFC
#define AF_NFC              39
#endif
#define NFC_SOCKPROTO_RAW   0
#define NFC_RAW_HEADER_LEN  1     /* kernel rawsock prepends one byte */

struct sockaddr_nfc {
   unsigned short sa_family;
   uint32_t dev_idx;
   uint32_t target_idx;
   uint32_t nfc_protocol;
};

#define NLA_ALIGN4(n) (((n) + 3) & ~3)

/* ---- state ---- */
static int      afnfc_nl_fd      = -1;
static int      afnfc_nl_family  = 0;
static uint32_t afnfc_nl_seq     = 1;
static int      afnfc_dev_idx    = -1;
static int      afnfc_sock         = -1;
static bool     afnfc_ready      = false;
static bool     afnfc_dev_up     = false;
static bool     afnfc_polling    = false;
static bool     afnfc_present    = false;
static bool     afnfc_scanning   = false;	/* persistent poll active (start_scan) */
static uint32_t afnfc_target_idx = 0;
static uint8_t  afnfc_uid[10];
static unsigned afnfc_uid_len    = 0;

/* Command dialect of the open adapter, beyond the standard set every
 * driver passes through:
 *  - PN53X: Mifare Classic via the PN53x AUTH frame (the chip runs
 *    Crypto1 inside InDataExchange).
 *  - NINTENDO: the Switch Joy-Con / Pro Controller reader. Adds a batched
 *    NTAG write (0xA8) and takes Mifare Classic commands with the key
 *    inline (its MCU authenticates itself) rather than a separate AUTH.
 *  - UNKNOWN: Mifare tries the PN53x frame, then the inline-key form, and
 *    remembers whichever works. NTAG always uses the standard commands. */
enum afnfc_dialect
{
   AFNFC_DIALECT_UNKNOWN = 0,
   AFNFC_DIALECT_PN53X,
   AFNFC_DIALECT_NINTENDO
};
static enum afnfc_dialect afnfc_dialect = AFNFC_DIALECT_UNKNOWN;
/* Set once FAST_READ failed but READ worked on the current tag (e.g. an
 * original Mifare Ultralight); cleared when a different tag is connected. */
static bool     afnfc_no_fast_read = false;

/* ---- genl plumbing ---- */

static void put_attr(char *buf, int *off, uint16_t type,
      const void *d, int len)
{
   struct nlattr *na = (struct nlattr *)(buf + *off);
   na->nla_type = type;
   na->nla_len  = sizeof(*na) + len;
   memcpy(buf + *off + sizeof(*na), d, len);
   *off += NLA_ALIGN4(na->nla_len);
}

static int nl_send(uint8_t cmd, uint16_t flags, int have_dev, uint32_t dev,
      int extra_type, uint32_t extra)
{
   char buf[256];
   struct nlmsghdr  *nlh = (struct nlmsghdr *)buf;
   struct genlmsghdr *gh;
   int off;

   memset(buf, 0, sizeof(buf));
   nlh->nlmsg_type  = afnfc_nl_family;
   nlh->nlmsg_flags = NLM_F_REQUEST | flags;
   nlh->nlmsg_seq   = afnfc_nl_seq++;
   nlh->nlmsg_pid   = 0;
   gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
   gh->cmd     = cmd;
   gh->version = 1;
   off = NLMSG_ALIGN(sizeof(*nlh)) + NLMSG_ALIGN(sizeof(*gh));
   if (have_dev)
      put_attr(buf, &off, NFC_ATTR_DEVICE_INDEX, &dev, 4);
   if (extra_type)
      put_attr(buf, &off, (uint16_t)extra_type, &extra, 4);
   nlh->nlmsg_len = off;

   if (send(afnfc_nl_fd, buf, off, 0) < 0)
      return -errno;
   return 0;
}

struct attrs { const void *p[32]; int len[32]; };

static void parse_attrs(struct nlmsghdr *nlh, struct attrs *a)
{
   struct genlmsghdr *gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
   int   off  = NLMSG_ALIGN(sizeof(*gh));
   char *base = (char *)gh;
   int   rem  = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*gh));

   memset(a, 0, sizeof(*a));
   while (rem >= (int)sizeof(struct nlattr))
   {
      struct nlattr *na = (struct nlattr *)(base + off);
      int alen = na->nla_len;
      if (alen < (int)sizeof(*na) || alen > rem)
         break;
      if (na->nla_type < 32)
      {
         a->p[na->nla_type]   = (char *)na + sizeof(*na);
         a->len[na->nla_type] = alen - sizeof(*na);
      }
      off += NLA_ALIGN4(alen);
      rem -= NLA_ALIGN4(alen);
   }
}

/* recv one datagram. flags passed to recv(); timeout_ms>0 sets a finite
 * SO_RCVTIMEO (0 leaves it as-is). Use MSG_DONTWAIT for non-blocking. */
static int nl_recv(char *buf, int cap, int flags, int timeout_ms)
{
   if (timeout_ms > 0)
   {
      struct timeval tv;
      tv.tv_sec  = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      setsockopt(afnfc_nl_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   }
   return recv(afnfc_nl_fd, buf, cap, flags);
}

static int resolve_family(void)
{
   char buf[1024];
   struct nlmsghdr  *nlh = (struct nlmsghdr *)buf;
   struct genlmsghdr *gh;
   struct attrs a;
   int off, n, grp_id = -1;

   memset(buf, 0, sizeof(buf));
   nlh->nlmsg_type  = GENL_ID_CTRL;
   nlh->nlmsg_flags = NLM_F_REQUEST;
   nlh->nlmsg_seq   = afnfc_nl_seq++;
   gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
   gh->cmd     = CTRL_CMD_GETFAMILY;
   gh->version = 1;
   off = NLMSG_ALIGN(sizeof(*nlh)) + NLMSG_ALIGN(sizeof(*gh));
   put_attr(buf, &off, CTRL_ATTR_FAMILY_NAME,
         NFC_GENL_NAME, sizeof(NFC_GENL_NAME));
   nlh->nlmsg_len = off;
   if (send(afnfc_nl_fd, buf, off, 0) < 0)
      return -errno;

   n = nl_recv(buf, sizeof(buf), 0, 2000);
   if (n < 0)
      return -errno;
   nlh = (struct nlmsghdr *)buf;
   if (nlh->nlmsg_type == NLMSG_ERROR)
      return -1;

   parse_attrs(nlh, &a);
   if (!a.p[CTRL_ATTR_FAMILY_ID])
      return -1;
   afnfc_nl_family = *(uint16_t *)a.p[CTRL_ATTR_FAMILY_ID];

   /* Find + join the "events" multicast group so TARGETS_FOUND /
    * TARGET_LOST arrive asynchronously. */
   if (a.p[CTRL_ATTR_MCAST_GROUPS])
   {
      char *gbase = (char *)a.p[CTRL_ATTR_MCAST_GROUPS];
      int   grem  = a.len[CTRL_ATTR_MCAST_GROUPS];
      int   goff  = 0;
      while (grem > (int)sizeof(struct nlattr))
      {
         struct nlattr *grp = (struct nlattr *)(gbase + goff);
         char *ib = (char *)grp + sizeof(*grp);
         int   ir = grp->nla_len - sizeof(*grp), io = 0;
         const char *nm = NULL;
         int id = -1;
         while (ir > (int)sizeof(struct nlattr))
         {
            struct nlattr *ia = (struct nlattr *)(ib + io);
            void *v = (char *)ia + sizeof(*ia);
            if (ia->nla_type == CTRL_ATTR_MCAST_GRP_NAME)
               nm = (const char *)v;
            else if (ia->nla_type == CTRL_ATTR_MCAST_GRP_ID)
               id = *(int *)v;
            io += NLA_ALIGN4(ia->nla_len);
            ir -= NLA_ALIGN4(ia->nla_len);
         }
         if (nm && !strcmp(nm, NFC_GENL_MCAST))
            grp_id = id;
         goff += NLA_ALIGN4(grp->nla_len);
         grem -= NLA_ALIGN4(grp->nla_len);
      }
   }
   if (grp_id >= 0)
   {
      if (setsockopt(afnfc_nl_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
            &grp_id, sizeof(grp_id)) < 0)
         RARCH_WARN("[NFC] afnfc: join events group %d failed (%s).\n",
               grp_id, strerror(errno));
   }
   RARCH_LOG("[NFC] afnfc: genl family %d, events group %d (%s).\n",
         afnfc_nl_family, grp_id,
         grp_id >= 0 ? "joined" : "NOT FOUND -> no async events");
   return 0;
}

/* Enumerate adapters; pick RETRO_NFC_DEV if set, else the first. */
static int find_device(void)
{
   char buf[2048];
   const char *env  = getenv("RETRO_NFC_DEV");
   int   want       = (env && *env) ? atoi(env) : -1;
   int   chosen     = -1;

   nl_send(NFC_CMD_GET_DEVICE, NLM_F_DUMP, 0, 0, 0, 0);
   for (;;)
   {
      int n = nl_recv(buf, sizeof(buf), 0, 1000);
      struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
      struct attrs a;
      if (n < 0)
         break;
      if (nlh->nlmsg_type == NLMSG_DONE || nlh->nlmsg_type == NLMSG_ERROR)
         break;
      parse_attrs(nlh, &a);
      if (a.p[NFC_ATTR_DEVICE_INDEX])
      {
         int idx = (int)*(uint32_t *)a.p[NFC_ATTR_DEVICE_INDEX];
         if (want >= 0)
         {
            if (idx == want)
               chosen = idx;
         }
         else if (chosen < 0)
            chosen = idx;
      }
   }
   return chosen;
}

/* ---- AF_NFC transceive ---- */

static int af_connect(uint32_t tidx)
{
   struct sockaddr_nfc sa;
   sa.sa_family    = AF_NFC;
   sa.dev_idx      = (uint32_t)afnfc_dev_idx;
   sa.target_idx   = tidx;
   sa.nfc_protocol = AFNFC_CONN_PROTO;

   afnfc_sock = socket(AF_NFC, SOCK_SEQPACKET, NFC_SOCKPROTO_RAW);
   if (afnfc_sock < 0)
      return -errno;
   if (connect(afnfc_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
   {
      RARCH_WARN("[NFC] afnfc: connect nfc%d/target%u failed (%s).\n",
            afnfc_dev_idx, tidx, strerror(errno));
      close(afnfc_sock);
      afnfc_sock = -1;
      return -1;
   }
   return 0;
}

/* Send one command, receive the response, strip the rawsock header. */
/* Number of back-to-back transceive attempts. A lone read/write can hit an
 * in-flight poll_work maintenance WAIT and come back a Sequence Error; the
 * retry lands after the driver has seen `reading` and gone quiet (same
 * reason a burst dump succeeds after its first read). Collisions fail fast
 * (driver returns an error, not a timeout), so the retries are cheap. */
#define AFNFC_XCEIVE_TRIES 4

/* allow_empty: a header-only reply is a valid answer (e.g. AUTH or WRITE
 * on readers that swallow the tag's ACK) and returns 0. Otherwise it is
 * treated as a transient collision and retried. */
static int af_xceive(const uint8_t *tx, int txn, uint8_t *out, int outcap,
      bool allow_empty)
{
   uint8_t rx[300];
   int n, try;
   struct timeval tv;
   if (afnfc_sock < 0)
      return -1;
   tv.tv_sec  = 1;
   tv.tv_usec = 0;
   setsockopt(afnfc_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

   for (try = 0; try < AFNFC_XCEIVE_TRIES; try++)
   {
      if (send(afnfc_sock, tx, txn, 0) < 0)
         return -errno;               /* socket dead -- don't retry */
      n = recv(afnfc_sock, rx, sizeof(rx), 0);
      if (allow_empty && n == NFC_RAW_HEADER_LEN)
         return 0;
      if (n >= NFC_RAW_HEADER_LEN + 1) /* header + at least one payload byte */
      {
         n -= NFC_RAW_HEADER_LEN;
         if (n > outcap)
            n = outcap;
         memcpy(out, rx + NFC_RAW_HEADER_LEN, n);
         return n;
      }
      /* recv error or header-only/empty reply => transient collision
       * (Sequence Error from an in-flight WAIT); retry back-to-back. */
   }
   return -1;
}

/* ---- target lifecycle ---- */

static void af_drop(void)
{
   if (afnfc_sock >= 0)
      close(afnfc_sock);
   afnfc_sock       = -1;
   afnfc_present  = false;
   afnfc_uid_len  = 0;
}

static uint32_t afnfc_poll_mask(void)
{
   const char *e = getenv("RETRO_NFC_PROTO_MASK");
   if (e && *e)
      return (uint32_t)strtoul(e, NULL, 0);
   return AFNFC_POLL_MASK;
}

/* Bring the reader into a clean polling state, the way a fresh nfc-probe run
 * does. DEV_DOWN clears the driver's internal "reported" latch (set on the
 * first TARGETS_FOUND and NOT cleared by START_POLL alone), so without this a
 * bare START_POLL -- this run, or a new process that inherited an adapter a
 * prior run left up -- never gets a fresh target announcement. STOP_POLL /
 * DEV_DOWN on an already-idle adapter return EINVAL harmlessly. */
static void afnfc_teardown(void)
{
   /* Return the adapter to a fully torn-down state, exactly as nfc-probe's
    * atexit cleanup does: close the transceive socket, STOP_POLL, DEV_DOWN.
    * This resets any wedged (state 0x07) tag the driver was holding so the
    * next acquire starts from a clean detection. */
   af_drop();
   if (afnfc_polling)
   {
      nl_send(NFC_CMD_STOP_POLL, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
      afnfc_polling = false;
   }
   if (afnfc_dev_up)
   {
      nl_send(NFC_CMD_DEV_DOWN, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
      afnfc_dev_up = false;
   }
}

/* On TARGETS_FOUND: read the target list (GET_TARGET dump, fully drained
 * to release the device ref the kernel holds for the dump), then connect
 * the first target. */

/* Detect-only: GET_TARGET dump to cache the present target's UID without
 * connecting. Used by get_status during a persistent scan so a status poll
 * can report TAG_PRESENT; the actual connect happens at read time. */
static bool af_peek_target(void)
{
   char buf[2048];
   int  have = 0;

   nl_send(NFC_CMD_GET_TARGET, NLM_F_DUMP, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   for (;;)
   {
      int n = nl_recv(buf, sizeof(buf), 0, 300);
      struct nlmsghdr  *nlh = (struct nlmsghdr *)buf;
      struct genlmsghdr *gh;
      struct attrs a;
      if (n < 0)
         break;
      if (nlh->nlmsg_type == NLMSG_DONE || nlh->nlmsg_type == NLMSG_ERROR)
         break;
      gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
      if (gh->cmd == NFC_EVENT_TARGETS_FOUND)
         continue;
      parse_attrs(nlh, &a);
      if (!have && a.p[NFC_ATTR_TARGET_INDEX])
      {
         afnfc_target_idx = *(uint32_t *)a.p[NFC_ATTR_TARGET_INDEX];
         afnfc_uid_len    = 0;
         if (a.p[NFC_ATTR_TARGET_NFCID1])
         {
            int ul = a.len[NFC_ATTR_TARGET_NFCID1];
            if (ul > (int)sizeof(afnfc_uid))
               ul = (int)sizeof(afnfc_uid);
            memcpy(afnfc_uid, a.p[NFC_ATTR_TARGET_NFCID1], ul);
            afnfc_uid_len = (unsigned)ul;
         }
         have = 1;
      }
   }
   if (have)
      afnfc_present = true;
   return have != 0;
}

static bool af_grab_target(void)
{
   char buf[2048];
   int  have = 0;

   nl_send(NFC_CMD_GET_TARGET, NLM_F_DUMP, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   for (;;)
   {
      int n = nl_recv(buf, sizeof(buf), 0, 1000);
      struct nlmsghdr  *nlh = (struct nlmsghdr *)buf;
      struct genlmsghdr *gh;
      struct attrs a;
      if (n < 0)
         break;
      if (nlh->nlmsg_type == NLMSG_DONE || nlh->nlmsg_type == NLMSG_ERROR)
         break;
      gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
      if (gh->cmd == NFC_EVENT_TARGETS_FOUND)
         continue;
      parse_attrs(nlh, &a);
      if (!have && a.p[NFC_ATTR_TARGET_INDEX])
      {
         uint8_t  old_uid[10];
         unsigned old_len = afnfc_uid_len;
         memcpy(old_uid, afnfc_uid, sizeof(old_uid));
         afnfc_target_idx = *(uint32_t *)a.p[NFC_ATTR_TARGET_INDEX];
         afnfc_uid_len    = 0;
         if (a.p[NFC_ATTR_TARGET_NFCID1])
         {
            int ul = a.len[NFC_ATTR_TARGET_NFCID1];
            if (ul > (int)sizeof(afnfc_uid))
               ul = (int)sizeof(afnfc_uid);
            memcpy(afnfc_uid, a.p[NFC_ATTR_TARGET_NFCID1], ul);
            afnfc_uid_len = (unsigned)ul;
         }
         if (afnfc_uid_len != old_len || memcmp(afnfc_uid, old_uid, afnfc_uid_len))
            afnfc_no_fast_read = false;   /* different tag */
         have = 1;
      }
   }
   RARCH_LOG("[NFC] afnfc: GET_TARGET -> %s (idx %u, uid_len %u).\n",
         have ? "found" : "none", afnfc_target_idx, afnfc_uid_len);
   if (!have)
      return false;
   if (af_connect(afnfc_target_idx) < 0)
      return false;
   afnfc_present = true;
   RARCH_LOG("[NFC] afnfc: target %u connected on nfc%d.\n",
         afnfc_target_idx, afnfc_dev_idx);
   return true;
}

/* Name of the kernel driver (or its module) bound to adapter nfcN, read
 * from sysfs. Empty if unavailable. */
static void afnfc_driver_name(int idx, const char *leaf, char *out, size_t size)
{
   char path[128];
   char link[256];
   ssize_t n;
   const char *base;
   out[0] = '\0';
   snprintf(path, sizeof(path), "/sys/class/nfc/nfc%d/device/%s", idx, leaf);
   n = readlink(path, link, sizeof(link) - 1);
   if (n <= 0)
      return;
   link[n] = '\0';
   base    = strrchr(link, '/');
   strlcpy(out, base ? base + 1 : link, size);
}

static enum afnfc_dialect afnfc_detect_dialect(int idx)
{
   char drv[64], mod[64];
   const char *env = getenv("RETRO_NFC_AFNFC_DIALECT");
   if (env && *env)
   {
      if (!strcmp(env, "nintendo"))
         return AFNFC_DIALECT_NINTENDO;
      if (!strcmp(env, "pn53x"))
         return AFNFC_DIALECT_PN53X;
      return AFNFC_DIALECT_UNKNOWN;   /* "standard": no driver extensions */
   }
   afnfc_driver_name(idx, "driver",        drv, sizeof(drv));
   afnfc_driver_name(idx, "driver/module", mod, sizeof(mod));
   RARCH_LOG("[NFC] afnfc: nfc%d driver \"%s\" (module \"%s\").\n",
         idx, drv, mod);
   if (     strstr(drv, "nintendo") || strstr(mod, "nintendo")
         || strstr(drv, "joycon")   || strstr(mod, "joycon")
         || strstr(drv, "tera")     || strstr(mod, "tera"))
      return AFNFC_DIALECT_NINTENDO;
   if (!strncmp(drv, "pn53", 4) || !strncmp(mod, "pn53", 4))
      return AFNFC_DIALECT_PN53X;
   return AFNFC_DIALECT_UNKNOWN;
}

/* ---- backend vtable ---- */

static bool afnfc_init(void)
{
   afnfc_nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
   if (afnfc_nl_fd < 0)
   {
      RARCH_ERR("[NFC] afnfc: NETLINK_GENERIC socket failed (%s).\n",
            strerror(errno));
      return false;
   }
   {
      /* The driver fires TARGETS_FOUND/TARGET_LOST every poll cycle; under
       * read-spam that floods this multicast socket. Without headroom the
       * kernel sets ENOBUFS and the socket stops delivering until re-opened --
       * which is why detection stalled every few reads and only a scanner
       * re-entry recovered it. Give it a big receive buffer and ask the kernel
       * to drop silently (NO_ENOBUFS) rather than wedge the socket; the driver
       * re-fires presence constantly, so a dropped event self-corrects. */
      int one = 1;
      int rcvbuf = 1 << 20;   /* 1 MiB */
      setsockopt(afnfc_nl_fd, SOL_NETLINK, NETLINK_NO_ENOBUFS, &one, sizeof(one));
      setsockopt(afnfc_nl_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
   }
   if (resolve_family() < 0)
   {
      RARCH_ERR("[NFC] afnfc: no \"nfc\" genl family "
                "(CONFIG_NFC + a driver loaded?).\n");
      close(afnfc_nl_fd);
      afnfc_nl_fd = -1;
      return false;
   }
   afnfc_dev_idx = find_device();
   if (afnfc_dev_idx < 0)
   {
      RARCH_WARN("[NFC] afnfc: no NFC adapter found.\n");
      close(afnfc_nl_fd);
      afnfc_nl_fd = -1;
      return false;
   }
   afnfc_ready              = true;
   afnfc_dialect      = afnfc_detect_dialect(afnfc_dev_idx);
   afnfc_no_fast_read = false;
   RARCH_LOG("[NFC] afnfc: using adapter nfc%d (%s commands).\n", afnfc_dev_idx,
           afnfc_dialect == AFNFC_DIALECT_NINTENDO ? "nintendo"
         : afnfc_dialect == AFNFC_DIALECT_PN53X    ? "pn53x"
         :                                           "standard");
   return true;
}

static void afnfc_deinit(void)
{
   af_drop();
   if (afnfc_polling)
      nl_send(NFC_CMD_STOP_POLL, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   if (afnfc_dev_up)
      nl_send(NFC_CMD_DEV_DOWN, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   if (afnfc_nl_fd >= 0)
      close(afnfc_nl_fd);
   afnfc_nl_fd   = -1;
   afnfc_ready   = false;
   afnfc_dev_up  = false;
   afnfc_polling = false;
}

static bool afnfc_start_scan(void)
{
   if (!afnfc_ready)
      return false;
   /* Start a persistent poll so get_status() can report TAG_PRESENT to a
    * polling consumer (e.g. Azahar). Reads stay on-demand (they teardown +
    * re-acquire), and get_status() re-arms the poll afterwards. */
   afnfc_teardown();
   nl_send(NFC_CMD_DEV_UP, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   afnfc_dev_up = true;
   nl_send(NFC_CMD_START_POLL, 0, 1, (uint32_t)afnfc_dev_idx,
         NFC_ATTR_IM_PROTOCOLS, afnfc_poll_mask());
   afnfc_polling  = true;
   afnfc_present  = false;
   afnfc_scanning = true;
   return true;
}

static void afnfc_stop_scan(void)
{
   afnfc_scanning = false;
   afnfc_present  = false;
   afnfc_teardown();
}

static enum retro_nfc_status afnfc_get_status(void)
{
   char buf[2048];
   int  n;

   if (!afnfc_ready)
      return RETRO_NFC_STATUS_UNSUPPORTED;
   if (!afnfc_scanning)
      return RETRO_NFC_STATUS_IDLE;

   /* Re-arm the poll if a prior read tore it down. */
   if (!afnfc_polling)
   {
      nl_send(NFC_CMD_DEV_UP, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
      afnfc_dev_up = true;
      nl_send(NFC_CMD_START_POLL, 0, 1, (uint32_t)afnfc_dev_idx,
            NFC_ATTR_IM_PROTOCOLS, afnfc_poll_mask());
      afnfc_polling = true;
      afnfc_present = false;
   }

   /* Drain pending events (non-blocking): a TARGETS_FOUND means a tag is on
    * the reader (peek its UID, no connect); TARGET_LOST/REMOVED clears it. */
   while ((n = recv(afnfc_nl_fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
   {
      struct nlmsghdr   *nlh = (struct nlmsghdr *)buf;
      struct genlmsghdr *gh;
      if (!NLMSG_OK(nlh, n))
         continue;
      if (nlh->nlmsg_type == NLMSG_ERROR || nlh->nlmsg_type == NLMSG_DONE)
         continue;
      gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
      if (gh->cmd == NFC_EVENT_TARGETS_FOUND)
      {
         afnfc_present = true;                    /* UID fetched lazily in get_tag_info */
      }
      else if (gh->cmd == NFC_EVENT_TARGET_LOST ||
               gh->cmd == NFC_EVENT_DEVICE_REMOVED)
      {
         afnfc_present = false;
         afnfc_uid_len = 0;
         /* NFC core is poll-once: a TARGET_LOST leaves it stopped, so the
          * next detection needs a fresh START_POLL. Re-arm above fires when
          * !polling. (Pairs with the driver now calling nfc_target_lost() on
          * session release, so a re-detected tag is a clean new target.) */
         afnfc_polling = false;
      }
   }

   return afnfc_present ? RETRO_NFC_STATUS_TAG_PRESENT
                  : RETRO_NFC_STATUS_SCANNING;
}

static bool afnfc_get_tag_info(struct retro_nfc_tag_info *info)
{
   if (!afnfc_present || !info)
      return false;
   if (!afnfc_uid_len)
      af_peek_target();                     /* lazy UID fetch, off the hot path */
   memset(info, 0, sizeof(*info));
   /* The genl layer reports the UID but not the memory size; amiibo are
    * NTAG215. Refine via GET_VERSION (0x60) if you need to tell 213/216
    * or Mifare apart. */
   /* Distinguish by NFCID1 length, same marker the kernel driver uses:
    * 4-byte UID = Mifare Classic, 7-byte = NTAG/Ultralight. */
   if (afnfc_uid_len == 4)
   {
      info->type     = RETRO_NFC_TAG_TYPE_MIFARE_CLASSIC_1K;
      info->mem_size = 1024;
      info->caps     = RETRO_NFC_CAP_HARDWARE
                     | RETRO_NFC_CAP_MIFARE_CLASSIC
                     | RETRO_NFC_CAP_RAW_TRANSCEIVE;
   }
   else
   {
      info->type     = RETRO_NFC_TAG_TYPE_NTAG215;
      info->mem_size = 540;
      info->caps     = RETRO_NFC_CAP_HARDWARE
                     | RETRO_NFC_CAP_NTAG
                     | RETRO_NFC_CAP_RAW_TRANSCEIVE;
   }
   if (afnfc_uid_len)
   {
      memcpy(info->uid, afnfc_uid, afnfc_uid_len);
      info->uid_len = afnfc_uid_len;
   }
   return true;
}


static long afnfc_last_xceive_ms = 0;

static long afnfc_now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* nfc-probe-style scan+connect, coupled to a read/write so there is no
 * idle gap for the tag to drop in. (Re)arm the scan and block up to
 * timeout_ms for the driver to report TARGETS_FOUND, then connect it.
 * The tag is physically on the reader during a read/write, so a fresh
 * detection lands within a few hundred ms. */
static bool afnfc_acquire(int timeout_ms)
{
   char buf[2048];
   long start;
   if (!afnfc_ready)
      return false;

   /* Clean slate first (nfc-probe's between-run state), then bring the
    * adapter up and poll from scratch -- this is the whole reason it works:
    * a fresh DEV_UP + START_POLL yields a fresh TARGETS_FOUND well inside the
    * ~1s hold window, before the driver's re-scan can wedge the tag. */
   afnfc_teardown();
   while (recv(afnfc_nl_fd, buf, sizeof(buf), MSG_DONTWAIT) > 0)
      ;                                  /* drain stale events */

   nl_send(NFC_CMD_DEV_UP, 0, 1, (uint32_t)afnfc_dev_idx, 0, 0);
   afnfc_dev_up = true;
   nl_send(NFC_CMD_START_POLL, 0, 1, (uint32_t)afnfc_dev_idx,
         NFC_ATTR_IM_PROTOCOLS, afnfc_poll_mask());
   afnfc_polling = true;

   start = afnfc_now_ms();
   for (;;)
   {
      int remaining = timeout_ms - (int)(afnfc_now_ms() - start);
      int n;
      struct nlmsghdr   *nlh = (struct nlmsghdr *)buf;
      struct genlmsghdr *gh;
      if (remaining <= 0)
         break;
      n = nl_recv(buf, sizeof(buf), 0, remaining);
      if (n < 0)
         break;
      if (!NLMSG_OK(nlh, n))
         continue;
      if (nlh->nlmsg_type == NLMSG_ERROR || nlh->nlmsg_type == NLMSG_DONE)
         continue;
      gh = (struct genlmsghdr *)NLMSG_DATA(nlh);
      if (gh->cmd == NFC_EVENT_TARGETS_FOUND && af_grab_target())
         return true;                    /* connected to a fresh target */
   }
   afnfc_teardown();
   return false;
}

/* Per-acquire byte budget: read/write this much per fresh detection so a
 * large transfer never outruns the ~1s hold window. 128 B = 32 pages. */
#define AFNFC_CHUNK_BYTES 128
/* FAST_READ (0x3A) pages per transceive: one round-trip returns this many
 * pages instead of 4, so a full amiibo is ~9 reads not ~34. 16 pages (64 B)
 * is a size the Joy-Con MCU returns cleanly and fits any reader's frame. */
#define AFNFC_READ_CHUNK_PAGES 16

static int afnfc_read(unsigned offset, uint8_t *buf, unsigned len)
{
   unsigned best      = 0;
   int      max_tries = 2;
   bool     fr_failed = false;   /* FAST_READ failed during this call */
   int tries;
   if (!buf)
      return -1;
   /* Reuse a live connection across coupled ops (e.g. write->verify, or a
    * dump split over calls); only teardown + re-acquire when it's stale
    * (>1s since the last transceive), because a wedged MCU can't be reset
    * by DEV_DOWN and a fresh re-acquire would fail. */
   if (afnfc_sock >= 0 && (afnfc_now_ms() - afnfc_last_xceive_ms) > 1000)
      afnfc_teardown();
   for (tries = 0; tries < max_tries; tries++)
   {
      unsigned done = 0;
      bool     fast = !afnfc_no_fast_read && !fr_failed;
      uint8_t  frame[AFNFC_READ_CHUNK_PAGES * 4 + 16];
      if (afnfc_sock < 0 && !afnfc_acquire(2000))
         break;
      while (done < len)
      {
         unsigned abs    = offset + done;
         unsigned page   = abs / 4;
         unsigned within = abs % 4;
         unsigned last   = (offset + len - 1) / 4;   /* last page needed */
         unsigned end    = page + AFNFC_READ_CHUNK_PAGES - 1;
         unsigned avail, take;
         uint8_t  cmd[3];
         int      got;
         if (end > last)
            end = last;                              /* don't over-read */
         if (fast)
         {
            cmd[0] = 0x3A;                            /* FAST_READ start..end */
            cmd[1] = (uint8_t)page;
            cmd[2] = (uint8_t)end;
            got = af_xceive(cmd, 3, frame, sizeof(frame), false);
         }
         else
         {
            cmd[0] = 0x30;                            /* READ: 4 pages */
            cmd[1] = (uint8_t)page;
            got = af_xceive(cmd, 2, frame, sizeof(frame), false);
         }
         if (got < (int)(within + 1))
         {
            if (fast && !fr_failed)
            {
               /* Retry the whole call with plain READ after a fresh
                * acquire (a failed command can halt the tag). */
               fr_failed = true;
               max_tries++;
            }
            break;
         }
         avail = (unsigned)got - within;
         take  = (len - done < avail) ? (len - done) : avail;
         memcpy(buf + done, frame + within, take);
         done += take;
      }
      afnfc_last_xceive_ms = afnfc_now_ms();
      if (done > best)
         best = done;
      if (done >= len)
      {
         if (!fast && fr_failed && !afnfc_no_fast_read)
         {
            RARCH_LOG("[NFC] afnfc: tag has no FAST_READ, using READ.\n");
            afnfc_no_fast_read = true;
         }
         return (int)done;             /* keep the connection alive */
      }
      afnfc_teardown();                 /* partial -> reset, retry fresh */
   }
   return (best > 0) ? (int)best : -1;
}

/* Nintendo-driver batched write (fast path). Returns len or -1. */
static int afnfc_write_batch(unsigned offset, const uint8_t *buf, unsigned len)
{
   int tries;
   for (tries = 0; tries < 2; tries++)
   {
      unsigned done = 0;
      if (afnfc_sock < 0 && !afnfc_acquire(2000))
         return -1;
      if (afnfc_uid_len != 7)
         return -1;
      while (done < len)
      {
         unsigned want = len - done;
         unsigned pages, i, flen;
         uint8_t  frame[10 + 5 * 32];
         uint8_t  ack[16];
         int      got;
         if (want > AFNFC_CHUNK_BYTES)
            want = AFNFC_CHUNK_BYTES;
         pages = want / 4;
         /*
          * Driver 0xA8 batch write (what nfc-probe sends):
          *   a8 07 <UID x7> <N> [page(1) data(4)] x N   (N<=32)
          * ACK after the AF_NFC header: 0x00 ok / 0xFF miss.
          */
         frame[0] = 0xA8;
         frame[1] = 0x07;
         memcpy(frame + 2, afnfc_uid, 7);
         frame[9] = (uint8_t)pages;
         flen = 10;
         for (i = 0; i < pages; i++)
         {
            frame[flen++] = (uint8_t)((offset + done + i * 4) / 4);
            memcpy(frame + flen, buf + done + i * 4, 4);
            flen += 4;
         }
         got = af_xceive(frame, (int)flen, ack, sizeof(ack), false);
         if (got < 1 || ack[0] != 0x00)
            break;
         done += pages * 4;
      }
      afnfc_last_xceive_ms = afnfc_now_ms();
      if (done >= len)
         return (int)done;             /* keep connection for verify-read */
      afnfc_teardown();
   }
   return -1;
}

/* Standard NTAG/Type 2 WRITE (0xA2 <page> <4 bytes>), one page at a time.
 * Readers differ in whether the tag's 4-bit ACK reaches userspace, so
 * success is decided by reading the range back. Returns len or -1. */
static int afnfc_write_std(unsigned offset, const uint8_t *buf, unsigned len)
{
   int      tries;
   int      ret   = -1;
   uint8_t *check = (uint8_t *)malloc(len);
   if (!check)
      return -1;
   for (tries = 0; tries < 2 && ret < 0; tries++)
   {
      unsigned done = 0;
      if (afnfc_sock < 0 && !afnfc_acquire(2000))
         break;
      while (done < len)
      {
         uint8_t cmd[6], ack[16];
         cmd[0] = 0xA2;
         cmd[1] = (uint8_t)((offset + done) / 4);
         memcpy(cmd + 2, buf + done, 4);
         if (af_xceive(cmd, 6, ack, sizeof(ack), true) < 0)
            break;
         done += 4;
      }
      afnfc_last_xceive_ms = afnfc_now_ms();
      if (     done >= len
            && afnfc_read(offset, check, len) == (int)len
            && !memcmp(check, buf, len))
         ret = (int)len;               /* keep connection alive */
      else
         afnfc_teardown();
   }
   free(check);
   return ret;
}

static int afnfc_write(unsigned offset, const uint8_t *buf, unsigned len)
{
   if (!buf || !len)
      return -1;
   if ((offset % 4) || (len % 4))
      return -1;
   if (afnfc_sock >= 0 && (afnfc_now_ms() - afnfc_last_xceive_ms) > 1000)
      afnfc_teardown();
   if (afnfc_dialect == AFNFC_DIALECT_NINTENDO)
   {
      int r = afnfc_write_batch(offset, buf, len);
      if (r >= 0)
         return r;
      RARCH_WARN("[NFC] afnfc: batched write failed, using WRITE 0xA2.\n");
   }
   return afnfc_write_std(offset, buf, len);
}

/* Lightweight runtime probe: is an NFC adapter present right now? Readers can
 * come and go with a controller, so this is queried live (cached ~1s) to offer
 * a hardware/software choice only while hardware is actually connected. */
bool nfc_afnfc_available(void)
{
   static bool     cached = false;
   static uint64_t last   = 0;
   uint64_t now           = afnfc_now_ms();
   int  saved;
   bool ok;

   if (afnfc_ready)                        /* already acquired -> present */
      return true;
   if (last && (now - last) < 1000)  /* re-probe at most ~1/s */
      return cached;

   saved = afnfc_nl_fd;
   afnfc_nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
   if (afnfc_nl_fd < 0)
   {
      afnfc_nl_fd  = saved;
      last   = now;
      cached = false;
      return false;
   }
   ok      = (resolve_family() >= 0) && (find_device() >= 0);
   close(afnfc_nl_fd);
   afnfc_nl_fd   = saved;
   last    = now;
   cached  = ok;
   return ok;
}

/* ---- interface version 5: Mifare Classic block I/O + raw transceive ---- */

/* Mifare Classic (sector, block-within-sector) -> absolute block. Sectors
 * 0-31 hold 4 blocks; 32-39 (4K) hold 16. */
static unsigned afnfc_mf_abs_block(unsigned sector, unsigned block)
{
   if (sector < 32)
      return sector * 4 + block;
   return 128 + (sector - 32) * 16 + block;
}

/* Validate (sector, block) and make sure a fresh link to the tag is up. */
static int afnfc_mf_prepare(unsigned sector, unsigned block,
      const void *key, const void *buf)
{
   if (!key || !buf || sector >= 40 || block >= (sector < 32 ? 4u : 16u))
      return RETRO_NFC_ERR_PARAM;
   if (afnfc_sock >= 0 && (afnfc_now_ms() - afnfc_last_xceive_ms) > 1000)
      afnfc_teardown();                     /* drop a stale/wedged link */
   if (afnfc_sock < 0 && !afnfc_acquire(2000))
      return RETRO_NFC_ERR_NO_TAG;
   return RETRO_NFC_OK;
}

/* PN53x-style AUTH: 0x60/0x61 <block> <key6> <uid4>. The reader runs
 * Crypto1; an empty reply means the sector is open. */
static int afnfc_mf_auth_pn53x(unsigned abs_block,
      enum retro_nfc_mifare_key_type key_type, const uint8_t *key)
{
   uint8_t cmd[12], rx[16];
   if (afnfc_uid_len < 4)
      return RETRO_NFC_ERR_PARAM;
   cmd[0] = (key_type == RETRO_NFC_MIFARE_KEY_B) ? 0x61 : 0x60;
   cmd[1] = (uint8_t)abs_block;
   memcpy(&cmd[2], key, RETRO_NFC_MIFARE_KEY_LEN);
   memcpy(&cmd[8], afnfc_uid + afnfc_uid_len - 4, 4);   /* last 4 UID bytes */
   return (af_xceive(cmd, 12, rx, sizeof(rx), true) < 0)
      ? RETRO_NFC_ERR_AUTH : RETRO_NFC_OK;
}

static int afnfc_mf_read_pn53x(unsigned abs_block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, uint8_t *out)
{
   uint8_t cmd[2], rx[32];
   int got, r = afnfc_mf_auth_pn53x(abs_block, key_type, key);
   if (r != RETRO_NFC_OK)
      return r;
   cmd[0] = 0x30;
   cmd[1] = (uint8_t)abs_block;
   got    = af_xceive(cmd, 2, rx, sizeof(rx), false);
   if (got < RETRO_NFC_MIFARE_BLOCK_LEN)
      return RETRO_NFC_ERR_IO;
   memcpy(out, rx, RETRO_NFC_MIFARE_BLOCK_LEN);
   return RETRO_NFC_OK;
}

static int afnfc_mf_write_pn53x(unsigned abs_block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, const uint8_t *data)
{
   uint8_t cmd[2 + RETRO_NFC_MIFARE_BLOCK_LEN], rx[16];
   int r = afnfc_mf_auth_pn53x(abs_block, key_type, key);
   if (r != RETRO_NFC_OK)
      return r;
   /* The reader performs both halves of the Mifare write (0xA0 <block>,
    * then the 16 data bytes) and reports a NAK as an error. */
   cmd[0] = 0xA0;
   cmd[1] = (uint8_t)abs_block;
   memcpy(&cmd[2], data, RETRO_NFC_MIFARE_BLOCK_LEN);
   return (af_xceive(cmd, (int)sizeof(cmd), rx, sizeof(rx), true) < 0)
      ? RETRO_NFC_ERR_PERM : RETRO_NFC_OK;
}

/* Nintendo driver: [0]=0x60(KeyA)/0x61(KeyB) [1]=block [2..7]=key6 -> 16 bytes. */
static int afnfc_mf_read_nintendo(unsigned abs_block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, uint8_t *out)
{
   uint8_t cmd[8], rx[32];
   int got;
   cmd[0] = (key_type == RETRO_NFC_MIFARE_KEY_B) ? 0x61 : 0x60;
   cmd[1] = (uint8_t)abs_block;
   memcpy(&cmd[2], key, RETRO_NFC_MIFARE_KEY_LEN);
   got = af_xceive(cmd, (int)sizeof(cmd), rx, (int)sizeof(rx), false);
   if (got < RETRO_NFC_MIFARE_BLOCK_LEN)
      return (got < 0) ? RETRO_NFC_ERR_IO : RETRO_NFC_ERR_AUTH;
   memcpy(out, rx, RETRO_NFC_MIFARE_BLOCK_LEN);
   return RETRO_NFC_OK;
}

/* Driver cmd -> 1-byte ACK (0x00 ok / 0xff refused). The key type rides as
 * the standard Mifare auth opcode, matching the read path and the wire:
 *   Key A: [0]=0xA0 [1]=block [2..17]=data16 [18..23]=key6          (len 24)
 *   Key B: [0]=0xA0 [1]=block [2..17]=data16 [18]=0x61 [19..24]=key6 (len 25) */
static int afnfc_mf_write_nintendo(unsigned abs_block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, const uint8_t *data)
{
   uint8_t cmd[25], ack[8];
   int got, len;
   cmd[0] = 0xA0;
   cmd[1] = (uint8_t)abs_block;
   memcpy(&cmd[2], data, RETRO_NFC_MIFARE_BLOCK_LEN);
   if (key_type == RETRO_NFC_MIFARE_KEY_B)
   {
      cmd[18] = 0x61;                        /* explicit auth opcode: Key B */
      memcpy(&cmd[19], key, RETRO_NFC_MIFARE_KEY_LEN);
      len = 25;
   }
   else
   {
      memcpy(&cmd[18], key, RETRO_NFC_MIFARE_KEY_LEN);   /* Key A (default auth) */
      len = 24;
   }
   got = af_xceive(cmd, len, ack, (int)sizeof(ack), false);
   if (got < 1)
      return RETRO_NFC_ERR_IO;
   return (ack[0] == 0x00) ? RETRO_NFC_OK : RETRO_NFC_ERR_PERM;
}

/* Run one Mifare block op in the adapter's dialect. On an adapter whose
 * dialect is not known yet, try the PN53x form and then the Nintendo form
 * on a fresh link, and keep whichever succeeds. A failed op leaves the tag
 * halted (and the raw socket shut), so always drop the link after one. */
static int afnfc_mf_run(unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type, const uint8_t *key,
      uint8_t *out, const uint8_t *data)
{
   unsigned abs_block = afnfc_mf_abs_block(sector, block);
   int r = afnfc_mf_prepare(sector, block, key, out ? (const void *)out
         : (const void *)data);
   if (r != RETRO_NFC_OK)
      return r;

   if (afnfc_dialect != AFNFC_DIALECT_NINTENDO)
   {
      r = out ? afnfc_mf_read_pn53x (abs_block, key_type, key, out)
              : afnfc_mf_write_pn53x(abs_block, key_type, key, data);
      if (r == RETRO_NFC_OK)
      {
         if (afnfc_dialect == AFNFC_DIALECT_UNKNOWN)
         {
            RARCH_LOG("[NFC] afnfc: Mifare uses PN53x-style AUTH.\n");
            afnfc_dialect = AFNFC_DIALECT_PN53X;
         }
         afnfc_last_xceive_ms = afnfc_now_ms();
         return r;
      }
      afnfc_teardown();
      if (afnfc_dialect == AFNFC_DIALECT_PN53X)
         return r;
      if (!afnfc_acquire(2000))
         return RETRO_NFC_ERR_NO_TAG;
   }

   r = out ? afnfc_mf_read_nintendo (abs_block, key_type, key, out)
           : afnfc_mf_write_nintendo(abs_block, key_type, key, data);
   if (r == RETRO_NFC_OK)
   {
      if (afnfc_dialect == AFNFC_DIALECT_UNKNOWN)
      {
         RARCH_LOG("[NFC] afnfc: Mifare uses inline-key commands.\n");
         afnfc_dialect = AFNFC_DIALECT_NINTENDO;
      }
      afnfc_last_xceive_ms = afnfc_now_ms();
   }
   else
      afnfc_teardown();
   return r;
}

static int afnfc_mifare_read_block(unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, uint8_t *out)
{
   if (!out)
      return RETRO_NFC_ERR_PARAM;
   return afnfc_mf_run(sector, block, key_type, key, out, NULL);
}

static int afnfc_mifare_write_block(unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, const uint8_t *data)
{
   if (!data)
      return RETRO_NFC_ERR_PARAM;
   return afnfc_mf_run(sector, block, key_type, key, NULL, data);
}

/* Raw ISO14443-3 transceive: core supplies the command payload; the kernel
 * frames/CRCs. Returns bytes received or a negative retro_nfc_result. */
static int afnfc_transceive(const uint8_t *tx, unsigned tx_len,
      uint8_t *rx, unsigned rx_cap)
{
   int got;
   if (!tx || tx_len == 0)
      return RETRO_NFC_ERR_PARAM;
   if (afnfc_sock >= 0 && (afnfc_now_ms() - afnfc_last_xceive_ms) > 1000)
      afnfc_teardown();                     /* drop a stale/wedged link */
   if (afnfc_sock < 0 && !afnfc_acquire(2000))
      return RETRO_NFC_ERR_NO_TAG;
   got = af_xceive(tx, (int)tx_len, rx, (int)rx_cap, true);
   if (got < 0)
   {
      afnfc_teardown();                     /* rawsock shuts down on error */
      return RETRO_NFC_ERR_IO;
   }
   afnfc_last_xceive_ms = afnfc_now_ms();
   return got;
}

const nfc_backend_t nfc_backend_afnfc = {
   "afnfc",
   afnfc_init,       afnfc_deinit,
   afnfc_start_scan, afnfc_stop_scan,
   afnfc_get_status, afnfc_get_tag_info,
   afnfc_read,       afnfc_write,
   afnfc_mifare_read_block, afnfc_mifare_write_block, afnfc_transceive
};
