/*-------------------------------------------------------------

dvd.h -- DVD subsystem

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


#ifndef __DVD_H__
#define __DVD_H__

/*! 
 * \file dvd.h 
 * \brief DVD subsystem
 *
 */ 

#include <gctypes.h>
#include <ogc/lwp_queue.h>
#include <ogc/disc_io.h>

/*! 
 * \addtogroup dvd_statecodes DVD state codes
 * @{
 */

#define  DVD_STATE_FATAL_ERROR			-1 
#define  DVD_STATE_END					0 
#define  DVD_STATE_BUSY					1 
#define  DVD_STATE_WAITING				2 
#define  DVD_STATE_COVER_CLOSED			3 
#define  DVD_STATE_NO_DISK				4 
#define  DVD_STATE_COVER_OPEN			5 
#define  DVD_STATE_WRONG_DISK			6 
#define  DVD_STATE_MOTOR_STOPPED		7 
#define  DVD_STATE_IGNORED				8 
#define  DVD_STATE_CANCELED				10 
#define  DVD_STATE_RETRY				11 

#define  DVD_ERROR_OK					0 
#define  DVD_ERROR_FATAL				-1 
#define  DVD_ERROR_IGNORED				-2 
#define  DVD_ERROR_CANCELED				-3 
#define  DVD_ERROR_COVER_CLOSED			-4 

/*!
 * @}
 */


/*! 
 * \addtogroup dvd_resetmode DVD reset modes
 * @{
 */

#define DVD_RESETHARD					0			/*!< Performs a hard reset. Complete new boot of FW. */
#define DVD_RESETSOFT					1			/*!< Performs a soft reset. FW restart and drive spinup */
#define DVD_RESETNONE					2			/*!< Only initiate DI registers */

/*!
 * @}
 */


/*! 
 * \addtogroup dvd_motorctrlmode DVD motor control modes
 * @{
 */

#define DVD_SPINMOTOR_DOWN				0x00000000	/*!< Stop DVD drive */
#define DVD_SPINMOTOR_UP				0x00000100  /*!< Start DVD drive */
#define DVD_SPINMOTOR_ACCEPT			0x00004000	/*!< Force DVD to accept the disk */
#define DVD_SPINMOTOR_CHECKDISK			0x00008000	/*!< Force DVD to perform a disc check */

/*!
 * @}
 */


#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*!
 * \typedef struct _dvddiskid dvddiskid
 * \brief forward typedef for struct _dvddiskid
 */
typedef struct _dvddiskid dvddiskid;

/*!
 * \typedef struct _dvddiskid dvddiskid
 *
 *        This structure holds the game vendors copyright informations.<br>
 *        Additionally it holds certain parameters for audiocontrol and<br>
 *        multidisc support.
 *
 * \param gamename[4] vendors game key
 * \param company[2] vendors company key
 * \param disknum number of disc when multidisc support is used.
 * \param gamever version of game
 * \param streaming flag to control audio streaming
 * \param streambufsize size of buffer used for audio streaming
 * \param pad[22] padding 
 */
struct _dvddiskid {
	s8 gamename[4];
	s8 company[2];
	u8 disknum;
	u8 gamever;
	u8 streaming;
	u8 streambufsize;
	u8 pad[22];
};

/*!
 * \typedef struct _dvdcmdblk dvdcmdblk
 * \brief forward typedef for struct _dvdcmdblk
 */
typedef struct _dvdcmdblk dvdcmdblk;


/*!
 * \typedef void (*dvdcbcallback)(s32 result,dvdcmdblk *block)
 * \brief function pointer typedef for the user's operations callback
 */
typedef void (*dvdcbcallback)(s32 result,dvdcmdblk *block);


/*!
 * \typedef struct _dvdcmdblk dvdcmdblk
 *
 *        This structure is used internally to control the requested operation.
 */
struct _dvdcmdblk {
	lwp_node node;
	u32 cmd;
	s32 state;
	s64 offset;
	u32 len;
	void *buf;
	u32 currtxsize;
	u32 txdsize;
	dvddiskid *id;
	dvdcbcallback cb;
	void *usrdata;
};


/*!
 * \typedef struct _dvddrvinfo dvddrvinfo
 * \brief forward typedef for struct _dvddrvinfo
 */
typedef struct _dvddrvinfo dvddrvinfo;


/*!
 * \typedef struct _dvddrvinfo dvddrvinfo
 *
 *        This structure structure holds the drive version infromation.<br>
 *		  Use DVD_Inquiry() to retrieve this information.
 *
 * \param rev_leve revision level
 * \param dev_code device code
 * \param rel_date release date
 * \param pad[24] padding
 */
struct _dvddrvinfo {
	u16 rev_level;
	u16 dev_code;
	u32 rel_date;
	u8  pad[24];
};


/*!
 * \typedef struct _dvdfileinfo dvdfileinfo
 * \brief forward typedef for struct _dvdfileinfo
 */
typedef struct _dvdfileinfo dvdfileinfo;


/*!
 * \typedef void (*dvdcallback)(s32 result,dvdfileinfo *info)
 * \brief function pointer typedef for the user's DVD operation callback
 *
 * \param[in] result error code of last operation
 * \param[in] info pointer to user's file info strucutre
 */
typedef void (*dvdcallback)(s32 result,dvdfileinfo *info);


/*!
 * \typedef struct _dvdfileinfo dvdfileinfo
 *
 *        This structure is used internally to control the requested file operation.
 */
struct _dvdfileinfo {
	dvdcmdblk block;
	u32 addr;
	u32 len;
	dvdcallback cb;
};


/*! 
 * \fn void DVD_Init()
 * \brief Initializes the DVD subsystem
 *
 *        You must call this function before calling any other DVD function
 *
 * \return none
 */
void DVD_Init();
void DVD_Pause();


/*! 
 * \fn void DVD_Reset(u32 reset_mode)
 * \brief Performs a reset of the drive and FW respectively.
 *
 * \param[in] reset_mode \ref dvd_resetmode "type" of reset
 *
 * \return none
 */
void DVD_Reset(u32 reset_mode);


/*! 
 * \fn s32 DVD_Mount()
 * \brief Mounts the DVD drive.
 *
 *        This is a synchronous version of DVD_MountAsync().
 *
 * \return none
 */
s32 DVD_Mount();
s32 DVD_GetDriveStatus();


/*! 
 * \fn s32 DVD_MountAsync(dvdcmdblk *block,dvdcbcallback cb)
 * \brief Mounts the DVD drive.
 *
 *        You <b>must</b> call this function in order to access the DVD.
 *
 *        Following tasks are performed:
 *      - Issue a hard reset to the drive.
 *      - Turn on drive's debug mode.
 *      - Patch drive's FW.
 *      - Enable extensions.
 *      - Read disc ID
 *
 *        The patch code and procedure was taken from the gc-linux DVD device driver.
 *
 * \param[in] block pointer to a dvdcmdblk structure used to process the operation
 * \param[in] cb callback to be invoked upon completion of operation
 *
 * \return none
 */
s32 DVD_MountAsync(dvdcmdblk *block,dvdcbcallback cb);


/*! 
 * \fn s32 DVD_ControlDrive(dvdcmdblk *block,u32 cmd)
 * \brief Controls the drive's motor and behavior.
 *
 *        This is a synchronous version of DVD_ControlDriveAsync().
 *
 * \param[in] block pointer to a dvdcmdblk structure used to process the operation
 * \param[in] cmd \ref dvd_motorctrlmode "command" to control the drive.
 *
 * \return none
 */
s32 DVD_ControlDrive(dvdcmdblk *block,u32 cmd);


/*! 
 * \fn s32 DVD_ControlDriveAsync(dvdcmdblk *block,u32 cmd,dvdcbcallback cb)
 * \brief Controls the drive's motor and behavior.
 *
 * \param[in] block pointer to a dvdcmdblk structure used to process the operation
 * \param[in] cmd \ref dvd_motorctrlmode "command" to control the drive.
 * \param[in] cb callback to be invoked upon completion of operation.
 *
 * \return none
 */
s32 DVD_ControlDriveAsync(dvdcmdblk *block,u32 cmd,dvdcbcallback cb);


/*! 
 * \fn s32 DVD_SetGCMOffset(dvdcmdblk *block,u32 offset)
 * \brief Sets the offset to the GCM. Used for multigame discs.
 *
 *        This is a synchronous version of DVD_SetGCMOffsetAsync().
 *
 * \param[in] block pointer to a dvdcmdblk structure used to process the operation
 * \param[in] offset offset to the GCM on disc.
 *
 * \return \ref dvd_errorcodes "dvd error code"
 */
s32 DVD_SetGCMOffset(dvdcmdblk *block,s64 offset);


/*! 
 * \fn s32 DVD_SetGCMOffsetAsync(dvdcmdblk *block,u32 offset,dvdcbcallback cb)
 * \brief Sets the offset to the GCM. Used for multigame discs.
 *
 *        This is a synchronous version of DVD_SetGCMOffsetAsync().
 *
 * \param[in] block pointer to a dvdcmdblk structure used to process the operation
 * \param[in] offset offset to the GCM on disc.
 * \param[in] cb callback to be invoked upon completion of operation.
 *
 * \return \ref dvd_errorcodes "dvd error code"
 */
s32 DVD_SetGCMOffsetAsync(dvdcmdblk *block,s64 offset,dvdcbcallback cb);

s32 DVD_GetCmdBlockStatus(dvdcmdblk *block);
s32 DVD_SpinUpDrive(dvdcmdblk *block);
s32 DVD_SpinUpDriveAsync(dvdcmdblk *block,dvdcbcallback cb);
s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info);
s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb);
s32 DVD_ReadPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,s32 prio);
s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb,s32 prio);
s32 DVD_ReadAbsAsyncForBS(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb);
s32 DVD_SeekPrio(dvdcmdblk *block,s64 offset,s32 prio);
s32 DVD_SeekAbsAsyncPrio(dvdcmdblk *block,s64 offset,dvdcbcallback cb,s32 prio);
s32 DVD_CancelAllAsync(dvdcbcallback cb);
s32 DVD_StopStreamAtEndAsync(dvdcmdblk *block,dvdcbcallback cb);
s32 DVD_StopStreamAtEnd(dvdcmdblk *block);
s32 DVD_ReadDiskID(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb);
u32 DVD_SetAutoInvalidation(u32 auto_inv);
dvddiskid* DVD_GetCurrentDiskID();
dvddrvinfo* DVD_GetDriveInfo();

#define DVD_SetUserData(block, data) ((block)->usrdata = (data))
#define DVD_GetUserData(block)       ((block)->usrdata)

#define DEVICE_TYPE_GAMECUBE_DVD		(('G'<<24)|('D'<<16)|('V'<<8)|'D')
extern const DISC_INTERFACE __io_gcdvd;

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
