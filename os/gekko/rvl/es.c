/* Wii: the parts of the title manager (/dev/es) RetroArch uses. */

#include <gekko/ios.h>

#define ES_LAUNCH_TITLE  0x08
#define ES_GET_VIEWCNT   0x12
#define ES_GET_VIEWS     0x13

#define TICKET_VIEW_SIZE 0xd8

/* Each buffer IOS writes gets cache lines of its own. */
static struct
{
   uint64_t id;
   uint8_t  pad0[24];
   uint32_t count;
   uint8_t  pad1[28];
   uint8_t  view[(TICKET_VIEW_SIZE + 31) & ~31];
} es __attribute__((aligned(32)));

int gk_es_launch_title(uint64_t title)
{
   gk_ios_vec_t v[3];
   int32_t fd = gk_ios_open("/dev/es", 0);
   if (fd < 0)
      return fd;
   es.id     = title;
   v[0].data = &es.id;
   v[0].len  = sizeof(es.id);
   v[1].data = &es.count;
   v[1].len  = sizeof(es.count);
   if (gk_ios_ioctlv(fd, ES_GET_VIEWCNT, 1, 1, v) < 0 || !es.count)
   {
      gk_ios_close(fd);
      return -1;
   }
   /* The first view is enough to launch. */
   es.count  = 1;
   v[2].data = es.view;
   v[2].len  = TICKET_VIEW_SIZE;
   if (gk_ios_ioctlv(fd, ES_GET_VIEWS, 2, 1, v) < 0)
   {
      gk_ios_close(fd);
      return -1;
   }
   v[1].data = es.view;
   v[1].len  = TICKET_VIEW_SIZE;
   /* Does not return on success: the system reloads. */
   return gk_ios_ioctlv(fd, ES_LAUNCH_TITLE, 2, 0, v);
}

int gk_es_launch_system_menu(void)
{
   return gk_es_launch_title(0x0000000100000002ull);
}
