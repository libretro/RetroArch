/*-------------------------------------------------------------

system.h -- OS functions and initialization

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

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

/*! \file system.h
\brief OS functions and initialization

*/

#include <gctypes.h>
#include <gcutil.h>
#include <time.h>
#include <ogc/lwp_queue.h>
#include "gx_struct.h"

#define SYS_BASE_CACHED					(0x80000000)
#define SYS_BASE_UNCACHED				(0xC0000000)

#define SYS_WD_NULL						0xffffffff

/*!
 * \addtogroup sys_resettypes OS reset types
 * @{
 */

#define SYS_RESTART						0			/*!< Reboot the gamecube, force, if necessary, to boot the IPL menu. Cold reset is issued */
#define SYS_HOTRESET					1			/*!< Restart the application. Kind of softreset */
#define SYS_SHUTDOWN					2			/*!< Shutdown the thread system, card management system etc. Leave current thread running and return to caller */

#define SYS_RETURNTOMENU				3			/*!< Directly load the Wii Channels menu, without actually cold-resetting the system */
#define SYS_POWEROFF					4			/*!< Powers off the Wii, automatically choosing Standby or Idle mode depending on the user's configuration */
#define SYS_POWEROFF_STANDBY			5			/*!< Powers off the Wii to standby (red LED, WC24 off) mode. */
#define SYS_POWEROFF_IDLE				6			/*!< Powers off the Wii to idle (yellow LED, WC24 on) mode. */

/*!
 *@}
 */

/*!
 * \addtogroup sys_mprotchans OS memory protection channels
 * @{
 */

#define SYS_PROTECTCHAN0				0			/*!< OS memory protection channel 0 */
#define SYS_PROTECTCHAN1				1			/*!< OS memory protection channel 1 */
#define SYS_PROTECTCHAN2				2			/*!< OS memory protection channel 2 */
#define SYS_PROTECTCHAN3				3			/*!< OS memory protection channel 2 */
#define SYS_PROTECTCHANMAX				4			/*!< _Termination */

/*!
 *@}
 */

/*!
 * \addtogroup sys_mprotmodes OS memory protection modes
 * @{
 */

#define SYS_PROTECTNONE					0x00000000		/*!< Read and write operations on protected region is granted */
#define SYS_PROTECTREAD					0x00000001		/*!< Read from protected region is permitted */
#define SYS_PROTECTWRITE				0x00000002		/*!< Write to protected region is permitted */
#define SYS_PROTECTRDWR					(SYS_PROTECTREAD|SYS_PROTECTWRITE)	/*!< Read and write operations on protected region is permitted */

/*!
 *@}
 */

#define SYS_FONTSIZE_ANSI				(288 + 131072)
#define SYS_FONTSIZE_SJIS				(3840 + 1179648)

/*!
 * \addtogroup sys_mcastmacros OS memory casting macros
 * @{
 */

#define MEM_VIRTUAL_TO_PHYSICAL(x)		(((u32)(x)) & ~SYS_BASE_UNCACHED)									/*!< Cast virtual address to physical address, e.g. 0x8xxxxxxx -> 0x0xxxxxxx */
#define MEM_PHYSICAL_TO_K0(x)			(void*)((u32)(x) + SYS_BASE_CACHED)									/*!< Cast physical address to cached virtual address, e.g. 0x0xxxxxxx -> 0x8xxxxxxx */
#define MEM_PHYSICAL_TO_K1(x)			(void*)((u32)(x) + SYS_BASE_UNCACHED)								/*!< Cast physical address to uncached virtual address, e.g. 0x0xxxxxxx -> 0xCxxxxxxx */
#define MEM_K0_TO_PHYSICAL(x)			(void*)((u32)(x) - SYS_BASE_CACHED)									/*!< Cast physical address to cached virtual address, e.g. 0x0xxxxxxx -> 0x8xxxxxxx */
#define MEM_K1_TO_PHYSICAL(x)			(void*)((u32)(x) - SYS_BASE_UNCACHED)								/*!< Cast physical address to uncached virtual address, e.g. 0x0xxxxxxx -> 0xCxxxxxxx */
#define MEM_K0_TO_K1(x)					(void*)((u32)(x) + (SYS_BASE_UNCACHED - SYS_BASE_CACHED))			/*!< Cast cached virtual address to uncached virtual address, e.g. 0x8xxxxxxx -> 0xCxxxxxxx */
#define MEM_K1_TO_K0(x)					(void*)((u32)(x) - (SYS_BASE_UNCACHED - SYS_BASE_CACHED))			/*!< Cast uncached virtual address to cached virtual address, e.g. 0xCxxxxxxx -> 0x8xxxxxxx */

/*!
 *@}
 */

#define SYS_GetArenaLo					SYS_GetArena1Lo
#define SYS_SetArenaLo					SYS_SetArena1Lo
#define SYS_GetArenaHi					SYS_GetArena1Hi
#define SYS_SetArenaHi					SYS_SetArena1Hi
#define SYS_GetArenaSize				SYS_GetArena1Size

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
 * \typedef u32 syswd_t
 * \brief handle typedef for the alarm context
 */
typedef u32 syswd_t;

 /*!
 * \typedef struct _syssram syssram
 * \brief holds the stored configuration value from the system SRAM area
 * \param checksum holds the block checksum.
 * \param checksum_in holds the inverse block checksum
 * \param ead0 unknown attribute
 * \param ead1 unknown attribute
 * \param counter_bias bias value for the realtime clock
 * \param display_offsetH pixel offset for the VI
 * \param ntd unknown attribute
 * \param lang language of system
 * \param flags device and operations flag
 */
typedef struct _syssram syssram;

struct _syssram {
	u16 checksum;
	u16 checksum_inv;
	u32 ead0;
	u32 ead1;
	u32 counter_bias;
	s8 display_offsetH;
	u8 ntd;
	u8 lang;
	u8 flags;
} ATTRIBUTE_PACKED;

/*!
 * \typedef struct _syssramex syssramex
 * \brief holds the stored configuration value from the extended SRAM area
 * \param flash_id[2][12] 96bit memorycard unlock flash ID
 * \param wirelessKbd_id Device ID of last connected wireless keyboard
 * \param wirelessPad_id[4] 16bit device ID of last connected pad.
 * \param dvderr_code last non-recoverable error from DVD interface
 * \param __padding0 padding
 * \param flashID_chksum[2] 16bit checksum of unlock flash ID
 * \param __padding1[4] padding
 */
typedef struct _syssramex syssramex;

struct _syssramex {
	u8 flash_id[2][12];
	u32 wirelessKbd_id;
	u16 wirelessPad_id[4];
	u8 dvderr_code;
	u8 __padding0;
	u16 flashID_chksum[2];
	u8 __padding1[4];
} ATTRIBUTE_PACKED;

typedef void (*alarmcallback)(syswd_t alarm,void *cb_arg);

typedef struct _sys_fontheader sys_fontheader;

struct _sys_fontheader {
	u16 font_type;
	u16 first_char;
	u16 last_char;
	u16 inval_char;
	u16 asc;
	u16 desc;
	u16 width;
	u16 leading;
    u16 cell_width;
    u16 cell_height;
    u32 sheet_size;
    u16 sheet_format;
    u16 sheet_column;
    u16 sheet_row;
    u16 sheet_width;
    u16 sheet_height;
    u16 width_table;
    u32 sheet_image;
    u32 sheet_fullsize;
    u8  c0;
    u8  c1;
    u8  c2;
    u8  c3;
} ATTRIBUTE_PACKED;

typedef void (*resetcallback)(void);
typedef void (*powercallback)(void);
typedef s32 (*resetfunction)(s32 final);
typedef struct _sys_resetinfo sys_resetinfo;

struct _sys_resetinfo {
	lwp_node node;
	resetfunction func;
	u32 prio;
};

/*! \fn void SYS_Init()
\deprecated Performs basic system initialization such as EXI init etc. This function is called from within the crt0 startup code.

\return none
*/
void SYS_Init();

/*!
 * \fn void* SYS_AllocateFramebuffer(GXRModeObj *rmode)
 * \brief Allocate cacheline aligned memory for the external framebuffer based on the rendermode object.
 * \param[in] rmode pointer to the video/render mode configuration
 *
 * \return pointer to the framebuffer's startaddress. <b><i>NOTE:</i></b> Address returned is aligned to a 32byte boundery!
 */
void* SYS_AllocateFramebuffer(GXRModeObj *rmode);

void SYS_ProtectRange(u32 chan,void *addr,u32 bytes,u32 cntrl);
void SYS_StartPMC(u32 mcr0val,u32 mcr1val);
void SYS_DumpPMC();
void SYS_StopPMC();

/*! \fn s32 SYS_CreateAlarm(syswd_t *thealarm)
\brief Create/initialize sysalarm structure
\param[in] thealarm pointer to the handle to store the created alarm context identifier

\return 0 on succuess, non-zero on error
*/
s32 SYS_CreateAlarm(syswd_t *thealarm);

/*! \fn s32 SYS_SetAlarm(syswd_t thealarm,const struct timespec *tp,alarmcallback cb)
\brief Set the alarm parameters for a one-shot alarm, add to the list of alarms and start.
\param[in] thealarm identifier to the alarm context to be initialize for a one-shot alarm
\param[in] tp pointer to timespec structure holding the time to fire the alarm
\param[in] cb pointer to callback which is called when the alarm fires.

\return 0 on succuess, non-zero on error
*/
s32 SYS_SetAlarm(syswd_t thealarm,const struct timespec *tp,alarmcallback cb,void *cbarg);

/*! \fn s32 SYS_SetPeriodicAlarm(syswd_t thealarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb)
\brief Set the alarm parameters for a periodioc alarm, add to the list of alarms and start. The alarm and interval persists as long as SYS_CancelAlarm() isn't called.
\param[in] thealarm identifier to the alarm context to be initialized for a periodic alarm
\param[in] tp_start pointer to timespec structure holding the time to fire first time the alarm
\param[in] tp_period pointer to timespec structure holding the interval for all following alarm triggers.
\param[in] cb pointer to callback which is called when the alarm fires.

\return 0 on succuess, non-zero on error
*/
s32 SYS_SetPeriodicAlarm(syswd_t thealarm,const struct timespec *tp_start,const struct timespec *tp_period,alarmcallback cb,void *cbarg);

/*! \fn s32 SYS_RemoveAlarm(syswd_t thealarm)
\brief Remove the given alarm context from the list of contexts and destroy it
\param[in] thealarm identifier to the alarm context to be removed and destroyed

\return 0 on succuess, non-zero on error
*/
s32 SYS_RemoveAlarm(syswd_t thealarm);

/*! \fn s32 SYS_CancelAlarm(syswd_t thealarm)
\brief Cancel the alarm, but do not remove from the list of contexts.
\param[in] thealarm identifier to the alram context to be canceled

\return 0 on succuess, non-zero on error
*/
s32 SYS_CancelAlarm(syswd_t thealarm);

void SYS_SetWirelessID(u32 chan,u32 id);
u32 SYS_GetWirelessID(u32 chan);
u32 SYS_GetFontEncoding();
u32 SYS_InitFont(sys_fontheader *font_data);
void SYS_GetFontTexture(s32 c,void **image,s32 *xpos,s32 *ypos,s32 *width);
void SYS_GetFontTexel(s32 c,void *image,s32 pos,s32 stride,s32 *width);
void SYS_ResetSystem(s32 reset,u32 reset_code,s32 force_menu);
void SYS_RegisterResetFunc(sys_resetinfo *info);
void SYS_UnregisterResetFunc(sys_resetinfo *info);
void SYS_SwitchFiber(u32 arg0,u32 arg1,u32 arg2,u32 arg3,u32 pc,u32 newsp);

void* SYS_GetArena1Lo();
void SYS_SetArena1Lo(void *newLo);
void* SYS_GetArena1Hi();
void SYS_SetArena1Hi(void *newHi);
u32 SYS_GetArena1Size();

resetcallback SYS_SetResetCallback(resetcallback cb);

u32 SYS_ResetButtonDown();

#if defined(HW_RVL)
u32 SYS_GetHollywoodRevision();
void* SYS_GetArena2Lo();
void SYS_SetArena2Lo(void *newLo);
void* SYS_GetArena2Hi();
void SYS_SetArena2Hi(void *newHi);
u32 SYS_GetArena2Size();
powercallback SYS_SetPowerCallback(powercallback cb);
#endif

/* \fn u64 SYS_Time()
\brief Returns the current time in ticks since 2000 (proper Dolphin/Wii time)
\return ticks since 2000
*/
u64 SYS_Time();

void kprintf(const char *str, ...);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
