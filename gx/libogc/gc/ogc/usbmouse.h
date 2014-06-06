#ifndef __USBMOUSE_H__
#define __USBMOUSE_H__

#if defined(HW_RVL)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct {
	u8 button;
	int rx;
	int ry;
	int rz;
} mouse_event;

s32 MOUSE_Init(void);
s32 MOUSE_Deinit(void);

s32 MOUSE_GetEvent(mouse_event *event);
s32 MOUSE_FlushEvents(void);
bool MOUSE_IsConnected(void);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
