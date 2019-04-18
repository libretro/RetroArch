#ifndef __PAD_H__
#define __PAD_H__

#include <gctypes.h>

#define PAD_CHAN0					0
#define PAD_CHAN1					1
#define PAD_CHAN2					2
#define PAD_CHAN3					3
#define PAD_CHANMAX					4

#define PAD_MOTOR_STOP				0
#define PAD_MOTOR_RUMBLE			1
#define PAD_MOTOR_STOP_HARD			2

#define PAD_ERR_NONE				0
#define PAD_ERR_NO_CONTROLLER		-1
#define PAD_ERR_NOT_READY			-2
#define PAD_ERR_TRANSFER			-3

#define PAD_BUTTON_LEFT				0x0001
#define PAD_BUTTON_RIGHT			0x0002
#define PAD_BUTTON_DOWN				0x0004
#define PAD_BUTTON_UP				0x0008
#define PAD_TRIGGER_Z				0x0010
#define PAD_TRIGGER_R				0x0020
#define PAD_TRIGGER_L				0x0040
#define PAD_BUTTON_A				0x0100
#define PAD_BUTTON_B				0x0200
#define PAD_BUTTON_X				0x0400
#define PAD_BUTTON_Y				0x0800
#define PAD_BUTTON_MENU				0x1000
#define PAD_BUTTON_START			0x1000

#define PAD_CHAN0_BIT				0x80000000
#define PAD_CHAN1_BIT				0x40000000
#define PAD_CHAN2_BIT				0x20000000
#define PAD_CHAN3_BIT				0x10000000
/*+----------------------------------------------------------------------------------------------+*/
/*+----------------------------------------------------------------------------------------------+*/
/*+----------------------------------------------------------------------------------------------+*/
#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */
/*+----------------------------------------------------------------------------------------------+*/
typedef struct _padstatus {
	u16 button;
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u8 triggerL;
	u8 triggerR;
	u8 analogA;
	u8 analogB;
	s8 err;
} PADStatus;

typedef void (*sampling_callback)(void);
/*+----------------------------------------------------------------------------------------------+*/
/*+----------------------------------------------------------------------------------------------+*/
/*+----------------------------------------------------------------------------------------------+*/

u32 PAD_Init();
u32 PAD_Sync();
u32 PAD_Read(PADStatus *status);
u32 PAD_Reset(u32 mask);
u32 PAD_Recalibrate(u32 mask);
void PAD_Clamp(PADStatus *status);
void PAD_ControlMotor(s32 chan,u32 cmd);
void PAD_SetSpec(u32 spec);

u32 PAD_ScanPads();

u16 PAD_ButtonsUp(int pad);
u16 PAD_ButtonsDown(int pad);
u16 PAD_ButtonsHeld(int pad);

s8 PAD_SubStickX(int pad);
s8 PAD_SubStickY(int pad);

s8 PAD_StickX(int pad);
s8 PAD_StickY(int pad);

u8 PAD_TriggerL(int pad);
u8 PAD_TriggerR(int pad);

sampling_callback PAD_SetSamplingCallback(sampling_callback cb);

/*+----------------------------------------------------------------------------------------------+*/

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
