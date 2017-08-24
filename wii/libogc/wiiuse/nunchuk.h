/* This source as presented is a modified version of original wiiuse for use 
 * with RetroArch, and must not be confused with the original software. */

#ifndef __NUNCHUK_H__
#define __NUNCHUK_H__

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

int nunchuk_handshake(struct wiimote_t* wm, struct nunchuk_t* nc, ubyte* data, uword len);
void nunchuk_disconnected(struct nunchuk_t* nc);
void nunchuk_event(struct nunchuk_t* nc, ubyte* msg);

#ifdef __cplusplus
}
#endif

#endif
