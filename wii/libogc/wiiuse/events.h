/* This source as presented is a modified version of original wiiuse for use
 * with RetroArch, and must not be confused with the original software. */

#ifndef __EVENTS_H__
#define __EVENTS_H__

#ifdef __cplusplus
extern "C" {
#endif

void wiiuse_pressed_buttons(struct wiimote_t* wm, ubyte* msg);

#ifdef __cplusplus
}
#endif

#endif
