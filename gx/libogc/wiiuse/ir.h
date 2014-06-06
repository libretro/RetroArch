#ifndef __IR_H__
#define __IR_H__

#include "wiiuse_internal.h"

#define WII_VRES_X		560
#define WII_VRES_Y		340

#ifdef __cplusplus
extern "C" {
#endif

void calculate_basic_ir(struct wiimote_t* wm, ubyte* data);
void calculate_extended_ir(struct wiimote_t* wm, ubyte* data);
float calc_yaw(struct ir_t* ir);
void interpret_ir_data(struct ir_t* ir, struct orient_t *orient);

#ifdef __cplusplus
}
#endif

#endif
