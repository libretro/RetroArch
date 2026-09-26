/* Host stand-in for the Wii SYSCONF queries dispserv_gx.c makes. */
#ifndef DISPSERV_GX_TEST_OGC_CONF_H
#define DISPSERV_GX_TEST_OGC_CONF_H

#define CONF_VIDEO_NTSC 0
#define CONF_VIDEO_PAL  1
#define CONF_VIDEO_MPAL 2

int CONF_GetVideo(void);
int CONF_GetEuRGB60(void);
int CONF_GetProgressiveScan(void);

#endif
