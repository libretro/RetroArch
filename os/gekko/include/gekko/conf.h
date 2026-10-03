/* Wii: the console settings in SYSCONF and setting.txt.  Each query
 * returns -1 where the setting is not available. */

#ifndef GEKKO_CONF_H
#define GEKKO_CONF_H

#include <gekko/gekko.h>

enum gk_conf_video
{
   GK_CONF_VIDEO_NTSC = 0,
   GK_CONF_VIDEO_PAL,
   GK_CONF_VIDEO_MPAL
};

int gk_conf_wide(void);          /* 1 for 16:9, 0 for 4:3 */
int gk_conf_progressive(void);   /* 480p chosen in the settings */
int gk_conf_eurgb60(void);       /* PAL60 allowed */
int gk_conf_video(void);         /* enum gk_conf_video */
int gk_conf_display_offset_h(void);   /* -32..32; 0 if unset */

#endif
