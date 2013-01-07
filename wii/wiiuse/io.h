/* This source as presented is a modified version of original wiiuse for use 
 * with RetroArch, and must not be confused with the original software. */

#ifndef __IO_H__
#define __IO_H__

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void wiiuse_handshake(struct wiimote_t* wm,ubyte *data,uword len);
void wiiuse_handshake_expansion_start(struct wiimote_t *wm);
void wiiuse_handshake_expansion(struct wiimote_t *wm,ubyte *data,uword len);
void wiiuse_disable_expansion(struct wiimote_t *wm);

int wiiuse_io_read(struct wiimote_t* wm);
int wiiuse_io_write(struct wiimote_t* wm, ubyte* buf, int len);

#ifdef __cplusplus
}
#endif

#endif

