/*-------------------------------------------------------------

video.h -- VIDEO subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __VIDEO_H__
#define __VIDEO_H__

/*!
 * \file video.h
 * \brief VIDEO subsystem
 *
 */

#include <gctypes.h>
#include "gx_struct.h"
#include "video_types.h"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
 * \typedef void (*VIRetraceCallback)(u32 retraceCnt)
 * \brief function pointer typedef for the user's retrace callback
 * \param[in] retraceCnt current retrace count
 */
typedef void (*VIRetraceCallback)(u32 retraceCnt);

typedef void (*VIPositionCallback)(u32 posX,u32 posY);

void* VIDEO_GetNextFramebuffer();
void* VIDEO_GetCurrentFramebuffer();

/*!
 * \fn void VIDEO_Init()
 * \brief Initializes the VIDEO subsystem. This call should be done in the early stages of your main()
 *
 * \return none
 */
void VIDEO_Init();

/*!
 * \fn void VIDEO_Flush()
 * \brief Flush the shadow registers to the drivers video registers.
 *
 * \return none
 */
void VIDEO_Flush();

/*!
 * \fn void VIDEO_SetBlack(bool black)
 * \brief Blackout the VIDEO interface.
 *
 * \param[in] black Boolean flag to determine whether to blackout the VI or not.
 *
 * \return none
 */
void VIDEO_SetBlack(bool black);

/*!
 * \fn u32 VIDEO_GetNextField()
 * \brief Get the next field in DS mode.
 *
 * \return \ref vi_fielddef "field"
 */
u32 VIDEO_GetNextField();

/*!
 * \fn u32 VIDEO_GetCurrentLine()
 * \brief Get current video line
 *
 * \return linenumber
 */
u32 VIDEO_GetCurrentLine();

/*!
 * \fn u32 VIDEO_GetCurrentTvMode()
 * \brief Get current configured TV mode
 *
 * \return \ref vi_standardtypedef "tvmode"
 */
u32 VIDEO_GetCurrentTvMode();

/*!
 * \fn void VIDEO_Configure(GXRModeObj *rmode)
 * \brief Configure the VI with the given render mode object
 *
 * \param[in] rmode pointer to the video/render mode \ref gxrmode_obj "configuration".
 *
 * \return none
 */
void VIDEO_Configure(GXRModeObj *rmode);

u32 VIDEO_GetFrameBufferSize(GXRModeObj *rmode);

/*!
 * \fn void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color)
 * \brief Clear the given framebuffer.
 *
 * \param[in] rmode pointer to a GXRModeObj, specifying the mode.
 * \param[in] fb pointer to the startaddress of the framebuffer to clear.
 * \param[in] color YUYUV value to use for clearing.
 *
 * \return none
 */
void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color);

/*!
 * \fn void VIDEO_WaitVSync(void)
 * \brief Wait on the next vertical retrace
 *
 * \return none
 */
void VIDEO_WaitVSync(void);

/*!
 * \fn void VIDEO_SetNextFramebuffer(void *fb)
 * \brief Set the framebuffer for the next VI register update.
 *
 * \return none
 */
void VIDEO_SetNextFramebuffer(void *fb);

/*!
 * \fn void VIDEO_SetNextRightFramebuffer(void *fb)
 * \brief Set the right framebuffer for the next VI register update. This is used for 3D Gloves for instance.
 *
 * \return none
 */
void VIDEO_SetNextRightFramebuffer(void *fb);

/*!
 * \fn VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback)
 * \brief Set the Pre-Retrace callback function. This function is called within the video interrupt handler before the VI registers will be updated.
 *
 * \param[in] callback pointer to the callback function which is called at pre-retrace.
 *
 * \return Old pre-retrace callback or NULL
 */
VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback);

/*!
 * \fn VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback)
 * \brief Set the Post-Retrace callback function. This function is called within the video interrupt handler after the VI registers are updated.
 *
 * \param[in] callback pointer to the callback function which is called at post-retrace.
 *
 * \return Old post-retrace callback or NULL
 */
VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback);

/*!
 * \fn u32 VIDEO_HaveComponentCable(void)
 * \brief Check for a component cable. This function returns 1 when a Component (YPbPr) cable is connected.
 *
 * \return 1 if a component cable is connected, 0 otherwise
 */
u32 VIDEO_HaveComponentCable(void);

GXRModeObj * VIDEO_GetPreferredMode(GXRModeObj *mode);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
