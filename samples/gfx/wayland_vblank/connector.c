/* gfx/common/drm_vblank.h's drm_vblank_open_connector() against cards
 * made up here: which card and CRTC a connector's kind and number
 * lead to, what is left open, and that no connector is probed. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>

struct fake_connector { uint32_t id, type, type_id, encoder; };
struct fake_card
{
   int denied;                       /* cannot be opened */
   int vrr;                          /* its CRTCs' VRR_ENABLED */
   unsigned connectors;
   struct fake_connector connector[4];
};
/* encoder N drives CRTC N + 100 */
static struct fake_card cards[8] = {
   { 1, 0, 1, { { 1, DRM_MODE_CONNECTOR_DisplayPort, 1, 11 } } },
   { 0, 0, 3, { { 1, DRM_MODE_CONNECTOR_DisplayPort, 1, 0 },
                { 2, DRM_MODE_CONNECTOR_HDMIA, 1, 21 },
                { 3, DRM_MODE_CONNECTOR_eDP, 1, 22 } } },
   { 0, 0, 2, { { 1, DRM_MODE_CONNECTOR_DisplayPort, 1, 31 },
                { 2, DRM_MODE_CONNECTOR_HDMIB, 2, 32 } } },
   { 0, 1, 1, { { 1, DRM_MODE_CONNECTOR_DisplayPort, 3, 41 } } }
};
static int      open_fds[8];          /* card + 1 open as fd 50 + n */
static unsigned opens, closes, probes, prop_gets;

static int fake_open(const char *path, int flags)
{
   unsigned i;
   int card = path[strlen(path) - 1] - '0';
   assert(!strncmp(path, "/dev/dri/card", 13) && (flags & O_CLOEXEC));
   if (card >= 4 || cards[card].denied)
   {
      errno = card >= 4 ? ENOENT : EACCES;
      return -1;
   }
   opens++;
   for (i = 0; i < 8; i++)
      if (!open_fds[i])
      {
         open_fds[i] = card + 1;
         return 50 + (int)i;
      }
   assert(0);
   return -1;
}
static int fake_close(int fd)
{
   assert(fd >= 50 && fd < 58 && open_fds[fd - 50]);
   open_fds[fd - 50] = 0;
   closes++;
   return 0;
}
static int fake_ioctl(int fd, unsigned long request, void *arg)
{
   unsigned i;
   struct fake_card *card;
   assert(fd >= 50 && fd < 58 && open_fds[fd - 50]);
   card = &cards[open_fds[fd - 50] - 1];
   if (request == DRM_IOCTL_MODE_GETRESOURCES)
   {
      struct drm_mode_card_res *res = (struct drm_mode_card_res*)arg;
      uint32_t *ids = (uint32_t*)(uintptr_t)res->connector_id_ptr;
      assert(res->count_connectors >= card->connectors);
      for (i = 0; i < card->connectors; i++)
         ids[i] = card->connector[i].id;
      res->count_connectors = card->connectors;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_GETCONNECTOR)
   {
      struct drm_mode_get_connector *conn =
         (struct drm_mode_get_connector*)arg;
      /* asked with no room for a mode, the kernel probes */
      if (!conn->count_modes)
         probes++;
      for (i = 0; i < card->connectors; i++)
         if (card->connector[i].id == conn->connector_id)
         {
            conn->connector_type    = card->connector[i].type;
            conn->connector_type_id = card->connector[i].type_id;
            conn->encoder_id        = card->connector[i].encoder;
            return 0;
         }
      errno = ENOENT;
      return -1;
   }
   if (request == DRM_IOCTL_MODE_GETENCODER)
   {
      struct drm_mode_get_encoder *enc = (struct drm_mode_get_encoder*)arg;
      enc->crtc_id = enc->encoder_id + 100;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_OBJ_GETPROPERTIES)
   {
      struct drm_mode_obj_get_properties *props =
         (struct drm_mode_obj_get_properties*)arg;
      uint32_t *ids    = (uint32_t*)(uintptr_t)props->props_ptr;
      uint64_t *values = (uint64_t*)(uintptr_t)props->prop_values_ptr;
      assert(props->obj_type == DRM_MODE_OBJECT_CRTC && props->count_props >= 2);
      ids[0] = 7; values[0] = 1;      /* ACTIVE */
      ids[1] = 8; values[1] = (uint64_t)card->vrr;
      props->count_props = 2;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_GETPROPERTY)
   {
      struct drm_mode_get_property *prop =
         (struct drm_mode_get_property*)arg;
      prop_gets++;
      strcpy(prop->name, prop->prop_id == 8 ? "VRR_ENABLED" : "ACTIVE");
      return 0;
   }
   errno = EINVAL;
   return -1;
}

#define open  fake_open
#define close fake_close
#define DRM_VBLANK_IOCTL fake_ioctl
#define DRM_VBLANK_CONNECTOR
#include <gfx/common/drm_vblank.h>

#define CODE DRM_VBLANK_CONNECTOR_CODE

int main(void)
{
   drm_vblank_t vb = { -1, 0, false };
   (void)drm_vblank_wait;

   /* DP-1: the first card may not be opened, on the second nothing
    * drives it, the third has it */
   assert(drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_DisplayPort, 1)));
   assert(vb.crtc == 131 && vb.fd >= 50 && open_fds[vb.fd - 50] == 3);
   assert(opens == 2 && closes == 1 && !vb.queued);
   close(vb.fd);

   /* by kind and number both */
   vb.fd = -1;
   assert(drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_eDP, 1)));
   assert(vb.crtc == 122 && open_fds[vb.fd - 50] == 2);
   close(vb.fd);
   vb.fd = -1;
   assert(drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_HDMIA, 1)));
   assert(vb.crtc == 121);
   close(vb.fd);

   /* "HDMI-n" is either kind of HDMI */
   vb.fd = -1;
   assert(drm_vblank_open_connector(&vb, CODE(DRM_VBLANK_CONNECTOR_HDMI, 2)));
   assert(vb.crtc == 132 && open_fds[vb.fd - 50] == 3);
   close(vb.fd);
   vb.fd = -1;
   assert(drm_vblank_open_connector(&vb, CODE(DRM_VBLANK_CONNECTOR_HDMI, 1)));
   assert(vb.crtc == 121);
   close(vb.fd);

   /* a refresh rate that varies: no clock, and no other card tried */
   vb.fd  = -1;
   opens  = closes = 0;
   assert(!drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_DisplayPort, 3)));
   assert(vb.fd == -1 && opens == 3 && closes == 3 && prop_gets);

   /* not there, not that kind, not that number */
   opens = closes = 0;
   assert(!drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_VGA, 1)));
   assert(!drm_vblank_open_connector(&vb, CODE(DRM_MODE_CONNECTOR_eDP, 2)));
   assert(!drm_vblank_open_connector(&vb, CODE(DRM_VBLANK_CONNECTOR_HDMI, 3)));
   assert(vb.fd == -1 && opens == closes && opens == 9);

   {
      unsigned i;
      for (i = 0; i < 8; i++)
         assert(!open_fds[i]);
   }
   assert(!probes);
   puts("PASS DRM connector to CRTC: cards, kinds, numbers, VRR and no probing");
   return 0;
}
