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

#ifndef __DRM_VBLANK_H
#define __DRM_VBLANK_H

/* A CRTC's vblanks as a clock. Its sequence is the count and the
 * sequence's stamp the time, on the monotonic clock; the next one is a
 * sequence event, queued on a file description of the clock's own and
 * waited for with a bound. None of it needs the DRM master, so a
 * client of a compositor can read the vblanks of the output its window
 * is on. Needs the sequence ioctls of Linux 4.15. One calling thread.
 *
 * Before including this: DRM_VBLANK_CONNECTOR for
 * drm_vblank_open_connector(), DRM_VBLANK_IOCTL for an ioctl() other
 * than the system's. */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <boolean.h>
#include <libretro.h>

/* libdrm's copy of these, where it was included first, is the same */
#ifndef DRM_IOCTL_VERSION
#include <drm/drm.h>
#include <drm/drm_mode.h>
#endif

#if defined(DRM_IOCTL_CRTC_GET_SEQUENCE) && defined(DRM_IOCTL_CRTC_QUEUE_SEQUENCE)
#define DRM_VBLANK_CLOCK

typedef struct
{
   /* The file description sequence events are queued on and read
    * from, so that they land among nobody else's events */
   int      fd;
   uint32_t crtc;
   bool     queued;
} drm_vblank_t;

/* libdrm's drmIoctl() where the includer has it, as DRM_VBLANK_IOCTL */
#ifndef DRM_VBLANK_IOCTL
#define DRM_VBLANK_IOCTL drm_vblank_ioctl
static int drm_vblank_ioctl(int fd, unsigned long request, void *arg)
{
   int ret;
   do
   {
      ret = ioctl(fd, request, arg);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
   return ret;
}
#endif

/* Takes the events that have arrived; true with the latest sequence
 * event's count and time */
static bool drm_vblank_read(drm_vblank_t *vb,
      uint64_t *sequence, int64_t *time_ns)
{
   char buf[256];
   bool ret = false;
   for (;;)
   {
      ssize_t i   = 0;
      ssize_t len = read(vb->fd, buf, sizeof(buf));
      if (len < (ssize_t)sizeof(struct drm_event))
         break;
      while (i + (ssize_t)sizeof(struct drm_event) <= len)
      {
         struct drm_event e;
         memcpy(&e, buf + i, sizeof(e));
         if (e.length < sizeof(e) || i + (ssize_t)e.length > len)
            break;
         if (     e.type == DRM_EVENT_CRTC_SEQUENCE
               && e.length >= sizeof(struct drm_event_crtc_sequence))
         {
            struct drm_event_crtc_sequence seq;
            memcpy(&seq, buf + i, sizeof(seq));
            *sequence  = seq.sequence;
            *time_ns   = seq.time_ns;
            vb->queued = false;
            ret        = true;
         }
         i += e.length;
      }
   }
   return ret;
}

/* The count of the latest vblank of the clock's CRTC, asked of @fd,
 * and its time in @at; one after @seen is waited for up to
 * @timeout_us. 0 where there is none: the CRTC scans nothing out, or
 * has no sequence to give. */
static unsigned drm_vblank_wait(drm_vblank_t *vb, int fd, unsigned seen,
      unsigned timeout_us, retro_time_t *at)
{
   struct drm_crtc_get_sequence get_seq;
   uint64_t sequence;
   int64_t time_ns;
   unsigned n;

   /* one a wait gave up on, since arrived */
   drm_vblank_read(vb, &sequence, &time_ns);

   get_seq.crtc_id     = vb->crtc;
   get_seq.active      = 0;
   get_seq.sequence    = 0;
   get_seq.sequence_ns = 0;
   /* An inactive CRTC scans nothing out, and a stamp of 0 is none */
   if (     DRM_VBLANK_IOCTL(fd, DRM_IOCTL_CRTC_GET_SEQUENCE, &get_seq) != 0
         || !get_seq.active
         || get_seq.sequence_ns <= 0)
      return 0;
   sequence = get_seq.sequence;
   time_ns  = get_seq.sequence_ns;
   n        = (unsigned)sequence ? (unsigned)sequence : 1;

   if (n == seen && timeout_us)
   {
      struct pollfd pfd;
      if (!vb->queued)
      {
         struct drm_crtc_queue_sequence queue;
         queue.crtc_id   = vb->crtc;
         queue.flags     = DRM_CRTC_SEQUENCE_RELATIVE;
         queue.sequence  = 1;
         queue.user_data = 0;
         if (DRM_VBLANK_IOCTL(vb->fd, DRM_IOCTL_CRTC_QUEUE_SEQUENCE,
                  &queue) != 0)
            return 0;
         vb->queued = true;
      }
      pfd.fd      = vb->fd;
      pfd.events  = POLLIN;
      pfd.revents = 0;
      if (     poll(&pfd, 1, (int)((timeout_us + 999) / 1000)) > 0
            && drm_vblank_read(vb, &sequence, &time_ns))
         n = (unsigned)sequence ? (unsigned)sequence : 1;
   }
   *at = (retro_time_t)(time_ns / 1000);
   return n;
}

#endif /* the sequence ioctls */

#endif

/* On its own guard: one file may include this for the clock alone and
 * then again, further down, with DRM_VBLANK_CONNECTOR */
#if defined(DRM_VBLANK_CONNECTOR) && defined(DRM_VBLANK_CLOCK) \
   && !defined(__DRM_VBLANK_CONNECTOR_H)
#define __DRM_VBLANK_CONNECTOR_H
/* A connector by its kind and number, the two halves of its name
 * ("DP-2": DRM_MODE_CONNECTOR_DisplayPort, 2). */
#define DRM_VBLANK_CONNECTOR_CODE(type, id) \
   ((((unsigned)(type) & 0xff) << 16) | ((unsigned)(id) & 0xffff))
/* A compositor that says "HDMI-1" for either kind of HDMI */
#define DRM_VBLANK_CONNECTOR_HDMI 0xff
#define DRM_VBLANK_CARDS          8
#define DRM_VBLANK_OBJECTS        64

/* A variable refresh rate has no vblanks to count on */
static bool drm_vblank_crtc_vrr(int fd, uint32_t crtc)
{
   uint32_t i;
   uint32_t ids[DRM_VBLANK_OBJECTS];
   uint64_t values[DRM_VBLANK_OBJECTS];
   struct drm_mode_obj_get_properties props;
   memset(&props, 0, sizeof(props));
   props.props_ptr       = (uint64_t)(uintptr_t)ids;
   props.prop_values_ptr = (uint64_t)(uintptr_t)values;
   props.count_props     = DRM_VBLANK_OBJECTS;
   props.obj_id          = crtc;
   props.obj_type        = DRM_MODE_OBJECT_CRTC;
   if (     DRM_VBLANK_IOCTL(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &props) != 0
         || props.count_props > DRM_VBLANK_OBJECTS)
      return false;
   for (i = 0; i < props.count_props; i++)
   {
      struct drm_mode_get_property prop;
      memset(&prop, 0, sizeof(prop));
      prop.prop_id = ids[i];
      if (     DRM_VBLANK_IOCTL(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop) == 0
            && !strcmp(prop.name, "VRR_ENABLED"))
         return values[i] != 0;
   }
   return false;
}

/* The CRTC that drives connector @code of the card open as @fd; 0 if
 * it has no such connector, or nothing drives it. */
static uint32_t drm_vblank_connector_crtc(int fd, unsigned code)
{
   uint32_t i;
   uint32_t ids[DRM_VBLANK_OBJECTS];
   struct drm_mode_card_res res;
   memset(&res, 0, sizeof(res));
   res.connector_id_ptr = (uint64_t)(uintptr_t)ids;
   res.count_connectors = DRM_VBLANK_OBJECTS;
   if (     DRM_VBLANK_IOCTL(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0
         || res.count_connectors > DRM_VBLANK_OBJECTS)
      return 0;
   for (i = 0; i < res.count_connectors; i++)
   {
      struct drm_mode_modeinfo mode;
      struct drm_mode_get_connector conn;
      struct drm_mode_get_encoder enc;
      unsigned type = (code >> 16) & 0xff;
      memset(&conn, 0, sizeof(conn));
      conn.connector_id = ids[i];
      /* room for a mode, or the kernel probes the connector: seconds,
       * and a flicker on some displays */
      conn.count_modes  = 1;
      conn.modes_ptr    = (uint64_t)(uintptr_t)&mode;
      if (     DRM_VBLANK_IOCTL(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0
            || conn.connector_type_id != (code & 0xffff))
         continue;
      if (type == DRM_VBLANK_CONNECTOR_HDMI)
      {
         if (     conn.connector_type != DRM_MODE_CONNECTOR_HDMIA
               && conn.connector_type != DRM_MODE_CONNECTOR_HDMIB)
            continue;
      }
      else if (conn.connector_type != type)
         continue;
      if (!conn.encoder_id)
         return 0;
      memset(&enc, 0, sizeof(enc));
      enc.encoder_id = conn.encoder_id;
      if (DRM_VBLANK_IOCTL(fd, DRM_IOCTL_MODE_GETENCODER, &enc) != 0)
         return 0;
      return enc.crtc_id;
   }
   return 0;
}

/* Opens the card whose connector @code is being driven, and its CRTC
 * as the clock's. False where no card has it, none may be opened, or
 * its refresh rate varies. */
static bool drm_vblank_open_connector(drm_vblank_t *vb, unsigned code)
{
   unsigned card;
   for (card = 0; card < DRM_VBLANK_CARDS; card++)
   {
      char path[] = "/dev/dri/card0";
      uint32_t crtc;
      int fd;
      path[sizeof(path) - 2] = (char)('0' + card);
      fd = open(path, O_RDWR | O_CLOEXEC | O_NONBLOCK);
      if (fd < 0)
         continue;
      crtc = drm_vblank_connector_crtc(fd, code);
      if (crtc && !drm_vblank_crtc_vrr(fd, crtc))
      {
         vb->fd     = fd;
         vb->crtc   = crtc;
         vb->queued = false;
         return true;
      }
      close(fd);
      if (crtc)
         break;
   }
   return false;
}
#endif /* DRM_VBLANK_CONNECTOR */
