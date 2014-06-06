/*-------------------------------------------------------------

card.h -- Memory card subsystem

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


#ifndef __CARD_H__
#define __CARD_H__

/*! 
\file card.h 
\brief Memory card subsystem

*/ 


#include <gctypes.h>

/*! \addtogroup cardsolts Memory card slots
 * @{
 */

#define CARD_SLOTA					0			/*!< memory card slot A */
#define CARD_SLOTB					1			/*!< memory card slot B */

/*! @} */


#define CARD_WORKAREA				(5*8*1024)	/*!< minimum size of the workarea passed to Mount[Async]() */
#define CARD_READSIZE				512			/*!< minimum size of block to read from memory card */
#define CARD_FILENAMELEN			32			/*!< maximum filename length */	
#define CARD_MAXFILES				128			/*!< maximum number of files on the memory card */

/*! \addtogroup card_errors Memory card error codes
 * @{
 */

#define CARD_ERROR_UNLOCKED			1			/*!< card beeing unlocked or allready unlocked. */
#define CARD_ERROR_READY            0			/*!< card is ready. */
#define CARD_ERROR_BUSY            -1			/*!< card is busy. */
#define CARD_ERROR_WRONGDEVICE     -2			/*!< wrong device connected in slot */
#define CARD_ERROR_NOCARD          -3			/*!< no memory card in slot */
#define CARD_ERROR_NOFILE          -4			/*!< specified file not found */
#define CARD_ERROR_IOERROR         -5			/*!< internal EXI I/O error */
#define CARD_ERROR_BROKEN          -6			/*!< directory structure or file entry broken */
#define CARD_ERROR_EXIST           -7			/*!< file allready exists with the specified parameters */
#define CARD_ERROR_NOENT           -8			/*!< found no empty block to create the file */
#define CARD_ERROR_INSSPACE        -9			/*!< not enough space to write file to memory card */
#define CARD_ERROR_NOPERM          -10			/*!< not enough permissions to operate on the file */
#define CARD_ERROR_LIMIT           -11			/*!< card size limit reached */
#define CARD_ERROR_NAMETOOLONG     -12			/*!< filename too long */
#define CARD_ERROR_ENCODING        -13			/*!< font encoding PAL/SJIS mismatch*/
#define CARD_ERROR_CANCELED        -14			/*!< card operation canceled */
#define CARD_ERROR_FATAL_ERROR     -128			/*!< fatal error, non recoverable */

/*! @} */


/* File attribute defines */
#define CARD_ATTRIB_PUBLIC			0x04
#define CARD_ATTRIB_NOCOPY			0x08
#define CARD_ATTRIB_NOMOVE			0x10

/* Banner & Icon Attributes */
#define CARD_BANNER_W				96
#define CARD_BANNER_H				32

#define CARD_BANNER_NONE			0x00
#define CARD_BANNER_CI				0x01
#define CARD_BANNER_RGB				0x02
#define CARD_BANNER_MASK			0x03

#define CARD_MAXICONS				8
#define CARD_ICON_W					32
#define CARD_ICON_H					32

#define CARD_ICON_NONE				0x00
#define CARD_ICON_CI				0x01
#define CARD_ICON_RGB				0x02
#define CARD_ICON_MASK				0x03

#define CARD_ANIM_LOOP				0x00
#define CARD_ANIM_BOUNCE			0x04
#define CARD_ANIM_MASK				0x04

#define CARD_SPEED_END				0x00
#define CARD_SPEED_FAST				0x01
#define CARD_SPEED_MIDDLE			0x02
#define CARD_SPEED_SLOW				0x03
#define CARD_SPEED_MASK				0x03

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */


/*! \typedef struct _card_file card_file
\brief structure to hold the fileinformations upon open and for later use.
\param chn CARD slot.
\param filenum file index in the card directory structure.
\param offset offset into the file.
\param len length of file.
\param iblock block index on memory card.
*/
typedef struct _card_file {
	s32 chn;
	s32 filenum;
	s32 offset;
	s32 len;
	u16 iblock;
} card_file;


/*! \typedef struct card_dir
\brief structure to hold the information of a directory entry
\param chn CARD slot.
\param fileno file index in the card directory structure.
\param filelen length of file.
\param filename[CARD_FILENAMELEN] name of the file on card.
\param gamecode[4] string identifier <=4.
\param company[2] string identifier <=2.
\param showall boolean flag whether to showall entries or ony those identified by card_gamecode and card_company, previously set within the call to CARD_Init()
*/
typedef struct _card_dir { 
      s32 chn; 
      u32 fileno; 
	  u32 filelen;
	  u8 permissions;
      u8 filename[CARD_FILENAMELEN]; 
      u8 gamecode[4]; 
      u8 company[2];
      bool showall;
} card_dir; 


/*! \typedef struct card_stat
\brief structure to hold the additional statistical informations.
\param filename[CARD_FILENAMELEN] name of the file on card.
\param len length of file.
\param gamecode[4] string identifier <=4.
\param company[2] string identifier <=2.
\param banner_fmt format of banner. 
\param icon_addr icon image address in file.
\param icon_speed speed of an animated icon.
\param comment_addr address in file of the comment block.
\param offset_banner offset in file to the banner's image data.
\param offset_banner_tlut offset in file to the banner's texture lookup table.
\param offset_icon[CARD_MAXICONS] array of offsets in file to the icon's image data <CARD_MAXICONS.
\param offset_icon_tlut offset in file to the icons's texture lookup table.
\param offset_data offset to additional data.
*/
typedef struct _card_stat {
	u8 filename[CARD_FILENAMELEN];
	u32 len;
	u32 time;		//time since 1970 in seconds
	u8 gamecode[4];
	u8 company[2];
	u8 banner_fmt;
	u32 icon_addr;
	u16 icon_fmt;
	u16 iconfmt[CARD_MAXICONS];
	u16 icon_speed;
	u16 iconspeed[CARD_MAXICONS];
	u32 comment_addr;
	u32 offset_banner;
	u32 offset_banner_tlut;
	u32 offset_icon[CARD_MAXICONS];
	u32 offset_icon_tlut[CARD_MAXICONS];
	u32 offset_data;
} card_stat;

#define CARD_GetBannerFmt(stat)         (((stat)->banner_fmt)&CARD_BANNER_MASK)
#define CARD_SetBannerFmt(stat,fmt)		((stat)->banner_fmt = (u8)(((stat)->banner_fmt&~CARD_BANNER_MASK)|(fmt)))
#define CARD_GetIconFmt(stat,n)			(((stat)->icon_fmt>>(2*(n)))&CARD_ICON_MASK)
#define CARD_SetIconFmt(stat,n,fmt)		((stat)->icon_fmt = (u16)(((stat)->icon_fmt&~(CARD_ICON_MASK<<(2*(n))))|((fmt)<<(2*(n)))))
#define CARD_GetIconSpeed(stat,n)		(((stat)->icon_speed>>(2*(n)))&~CARD_SPEED_MASK);
#define CARD_SetIconSpeed(stat,n,speed)	((stat)->icon_speed = (u16)(((stat)->icon_fmt&~(CARD_SPEED_MASK<<(2*(n))))|((speed)<<(2*(n)))))
#define CARD_SetIconAddr(stat,addr)		((stat)->icon_addr = (u32)(addr))
#define CARD_SetCommentAddr(stat,addr)	((stat)->comment_addr = (u32)(addr))

/*! \typedef void (*cardcallback)(s32 chan,s32 result)
\brief function pointer typedef for the user's operation callback
\param chan CARD slot
\param result result of operation upon call of callback.
*/
typedef void (*cardcallback)(s32 chan,s32 result);


/*! \fn s32 CARD_Init(const char *gamecode,const char *company)
\brief Performs the initialization of the memory card subsystem
\param[in] gamecode pointer to a 4byte long string to specify the vendors game code. May be NULL
\param[in] company pointer to a 2byte long string to specify the vendors company code. May be NULL

\return \ref card_errors "card error codes"
*/
s32 CARD_Init(const char *gamecode,const char *company);


/*! \fn s32 CARD_Probe(s32 chn)
\brief Performs a check against the desired EXI channel if a device is inserted
\param[in] chn CARD slot

\return \ref card_errors "card error codes"
*/
s32 CARD_Probe(s32 chn);


/*! \fn s32 CARD_ProbeEx(s32 chn,s32 *mem_size,s32 *sect_size)
\brief Performs a check against the desired EXI channel if a memory card is inserted or mounted
\param[in] chn CARD slot
\param[out] mem_size pointer to a integer variable, ready to take the resulting value (this param is optional and can be NULL)
\param[out] sect_size pointer to a integer variable, ready to take the resulting value (this param is optional and can be NULL)

\return \ref card_errors "card error codes"
*/
s32 CARD_ProbeEx(s32 chn,s32 *mem_size,s32 *sect_size);


/*! \fn s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb)
\brief Mounts the memory card in the slot CHN. Synchronous version.
\param[in] chn CARD slot
\param[in] workarea pointer to memory area to hold the cards system area. The startaddress of the workdarea should be aligned on a 32byte boundery
\param[in] detach_cb pointer to a callback function. This callback function will be called when the card is removed from the slot.

\return \ref card_errors "card error codes"
*/
s32 CARD_Mount(s32 chn,void *workarea,cardcallback detach_cb);


/*! \fn s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb)
\brief Mounts the memory card in the slot CHN. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot
\param[in] workarea pointer to memory area to hold the cards system area. The startaddress of the workdarea should be aligned on a 32byte boundery
\param[in] detach_cb pointer to a callback function. This callback function will be called when the card is removed from the slot.
\param[in] attach_cb pointer to a callback function. This callback function will be called when the mount process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_MountAsync(s32 chn,void *workarea,cardcallback detach_cb,cardcallback attach_cb);


/*! \fn s32 CARD_Unmount(s32 chn)
\brief Unmounts the memory card in the slot CHN and releases the EXI bus.
\param[in] chn CARD slot

\return \ref card_errors "card error codes"
*/
s32 CARD_Unmount(s32 chn);


/*! \fn s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset)
\brief Reads the data from the file into the buffer from the given offset with the given length. Synchronous version
\param[in] file pointer to the card_file structure. It holds the fileinformations to read from.
\param[out] buffer pointer to memory area read-in the data. The startaddress of the buffer should be aligned to a 32byte boundery.
\param[in] len length of data to read.
\param[in] offset offset into the file to read from.

\return \ref card_errors "card error codes"
*/
s32 CARD_Read(card_file *file,void *buffer,u32 len,u32 offset);


/*! \fn s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
\brief Reads the data from the file into the buffer from the given offset with the given length. This function returns immediately. Asynchronous version
\param[in] file pointer to the card_file structure. It holds the fileinformations to read from.
\param[out] buffer pointer to memory area read-in the data. The startaddress of the buffer should be aligned to a 32byte boundery.
\param[in] len length of data to read.
\param[in] offset offset into the file to read from.
\param[in] callback pointer to a callback function. This callback will be called when the read process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_ReadAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);


/*! \fn s32 CARD_Open(s32 chn,const char *filename,card_file *file)
\brief Opens the file with the given filename and fills in the fileinformations.
\param[in] chn CARD slot
\param[in] filename name of the file to open.
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.

\return \ref card_errors "card error codes"
*/
s32 CARD_Open(s32 chn,const char *filename,card_file *file);


/*! \fn s32 CARD_OpenEntry(s32 chn,card_dir *entry,card_file *file)
\brief Opens the file with the given filename and fills in the fileinformations.
\param[in] chn CARD slot
\param[in] entry pointer to the directory entry to open.
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.

\return \ref card_errors "card error codes"
*/
s32 CARD_OpenEntry(s32 chn,card_dir *entry,card_file *file);


/*! \fn s32 CARD_Close(card_file *file)
\brief Closes the file with the given card_file structure and releases the handle.
\param[in] file pointer to the card_file structure to close.

\return \ref card_errors "card error codes"
*/
s32 CARD_Close(card_file *file);


/*! \fn s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file)
\brief Creates a new file with the given filename and fills in the fileinformations. Synchronous version.
\param[in] chn CARD slot
\param[in] filename name of the file to create.
\param[in] size size of the newly created file.
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.

\return \ref card_errors "card error codes"
*/
s32 CARD_Create(s32 chn,const char *filename,u32 size,card_file *file);


/*! \fn s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback)
\brief Creates a new file with the given filename and fills in the fileinformations. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot
\param[in] filename name of the file to create.
\param[in] size size of the newly created file.
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.
\param[in] callback pointer to a callback function. This callback will be called when the create process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_CreateAsync(s32 chn,const char *filename,u32 size,card_file *file,cardcallback callback);


/*! \fn s32 CARD_CreateEntry(s32 chn,card_dir *entry,card_file *file)
\brief Creates a new file with the given filename and fills in the fileinformations. Synchronous version.
\param[in] chn CARD slot
\param[in] entry pointer to the directory entry to create.
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.

\return \ref card_errors "card error codes"
*/
s32 CARD_CreateEntry(s32 chn,card_dir *direntry,card_file *file);


/*! \fn s32 CARD_CreateEntryAsync(s32 chn,card_dir *entry,card_file *file,cardcallback callback)
\brief Creates a new file with the given filename and fills in the fileinformations. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot
\param[in] entry pointer to the directory entry to create
\param[out] file pointer to the card_file structure. It receives the fileinformations for later usage.
\param[in] callback pointer to a callback function. This callback will be called when the create process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_CreateEntryAsync(s32 chn,card_dir *direntry,card_file *file,cardcallback callback);


/*! \fn s32 CARD_Delete(s32 chn,const char *filename)
\brief Deletes a file with the given filename. Synchronous version.
\param[in] chn CARD slot
\param[in] filename name of the file to delete.

\return \ref card_errors "card error codes"
*/
s32 CARD_Delete(s32 chn,const char *filename);


/*! \fn s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback)
\brief Deletes a file with the given filename. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot
\param[in] filename name of the file to delete.
\param[in] callback pointer to a callback function. This callback will be called when the delete process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_DeleteAsync(s32 chn,const char *filename,cardcallback callback);


/*! \fn s32 CARD_DeleteEntry(s32 chn,card_dir *dir_entry)
\brief Deletes a file with the given directory entry informations.
\param[in] chn CARD slot
\param[in] dir_entry pointer to the card_dir structure which holds the informations for the delete operation.

\return \ref card_errors "card error codes"
*/
s32 CARD_DeleteEntry(s32 chn,card_dir *dir_entry);


/*! \fn s32 CARD_DeleteEntryAsync(s32 chn,card_dir *dir_entry,cardcallback callback)
\brief Deletes a file with the given directory entry informations. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot
\param[in] dir_entry pointer to the card_dir structure which holds the informations for the delete operation.
\param[in] callback pointer to a callback function. This callback will be called when the delete process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_DeleteEntryAsync(s32 chn,card_dir *dir_entry,cardcallback callback);


/*! \fn s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset)
\brief Writes the data to the file from the buffer to the given offset with the given length. Synchronous version
\param[in] file pointer to the card_file structure which holds the fileinformations.
\param[in] buffer pointer to the memory area to read from. The startaddress of the buffer should be aligned on a 32byte boundery.
\param[in] len length of data to write.
\param[in] offset starting point in the file to start writing.

\return \ref card_errors "card error codes"
*/
s32 CARD_Write(card_file *file,void *buffer,u32 len,u32 offset);


/*! \fn s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback)
\brief Writes the data to the file from the buffer to the given offset with the given length. This function returns immediately. Asynchronous version
\param[in] file pointer to the card_file structure which holds the fileinformations.
\param[in] buffer pointer to the memory area to read from. The startaddress of the buffer should be aligned on a 32byte boundery.
\param[in] len length of data to write.
\param[in] offset starting point in the file to start writing.
\param[in] callback pointer to a callback function. This callback will be called when the write process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_WriteAsync(card_file *file,void *buffer,u32 len,u32 offset,cardcallback callback);


/*! \fn s32 CARD_GetErrorCode(s32 chn)
\brief Returns the result code from the last operation.
\param[in] chn CARD slot

\return \ref card_errors "card error codes" of last operation
*/
s32 CARD_GetErrorCode(s32 chn);


/*! \fn s32 CARD_FindFirst(s32 chn, card_dir *dir, bool showall)
\brief Start to iterate thru the memory card's directory structure and returns the first directory entry.
\param[in] chn CARD slot
\param[out] dir pointer to card_dir structure to receive the result set.
\param[in] showall Whether to show all files of the memory card or only those which are identified by the company and gamecode string.

\return \ref card_errors "card error codes"
*/
s32 CARD_FindFirst(s32 chn, card_dir *dir, bool showall);
 

/*! \fn s32 CARD_FindNext(card_dir *dir)
\brief Returns the next directory entry from the memory cards directory structure.
\param[out] dir pointer to card_dir structure to receive the result set.

\return \ref card_errors "card error codes"
*/
s32 CARD_FindNext(card_dir *dir); 


/*! \fn s32 CARD_GetDirectory(s32 chn, card_dir *dir_entries, s32 *count, bool showall)
\brief Returns the directory entries. size of entries is max. 128.
\param[in] chn CARD slot
\param[out] dir_entries pointer to card_dir structure to receive the result set.
\param[out] count pointer to an integer to receive the counted entries.
\param[in] showall Whether to show all files of the memory card or only those which are identified by the company and gamecode string.

\return \ref card_errors "card error codes"
*/
s32 CARD_GetDirectory(s32 chn, card_dir *dir_entries, s32 *count, bool showall);
 

/*! \fn s32 CARD_GetSectorSize(s32 chn,u32 *sector_size)
\brief Returns the next directory entry from the memory cards directory structure.
\param[in] chn CARD slot.
\param[out] sector_size pointer to receive the result.

\return \ref card_errors "card error codes"
*/
s32 CARD_GetSectorSize(s32 chn,u32 *sector_size);


/*! \fn s32 CARD_GetBlockCount(s32 chn,u32 *block_count)
\brief Returns the next directory entry from the memory cards directory structure.
\param[in] chn CARD slot.
\param[out] sector_size pointer to receive the result.

\return \ref card_errors "card error codes"
*/
s32 CARD_GetBlockCount(s32 chn,u32 *block_count);


/*! \fn s32 CARD_GetStatus(s32 chn,s32 fileno,card_stat *stats)
\brief Get additional file statistic informations.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] stats pointer to receive the result set.

\return \ref card_errors "card error codes"
*/
s32 CARD_GetStatus(s32 chn,s32 fileno,card_stat *stats);


/*! \fn s32 CARD_SetStatus(s32 chn,s32 fileno,card_stat *stats)
\brief Set additional file statistic informations. Synchronous version.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] stats pointer which holds the informations to set.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetStatus(s32 chn,s32 fileno,card_stat *stats);


/*! \fn s32 CARD_SetStatusAsync(s32 chn,s32 fileno,card_stat *stats,cardcallback callback)
\brief Set additional file statistic informations. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] stats pointer which holds the informations to set.
\param[in] callback pointer to a callback function. This callback will be called when the setstatus process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetStatusAsync(s32 chn,s32 fileno,card_stat *stats,cardcallback callback);


/*! \fn s32 CARD_GetAttributes(s32 chn,s32 fileno,u8 *attr)
\brief Get additional file attributes. Synchronous version.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] attr pointer to receive attribute value.

\return \ref card_errors "card error codes"
*/
s32 CARD_GetAttributes(s32 chn,s32 fileno,u8 *attr);


/*! \fn s32 CARD_SetAttributes(s32 chn,s32 fileno,u8 attr)
\brief Set additional file attributes. Synchronous version.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[in] attr attribute value to set.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetAttributes(s32 chn,s32 fileno,u8 attr);


/*! \fn s32 CARD_SetAttributesAsync(s32 chn,s32 fileno,u8 attr,cardcallback callback)
\brief Set additional file attributes. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[in] attr attribute value to set.
\param[in] callback pointer to a callback function. This callback will be called when the setattributes process has finished.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetAttributesAsync(s32 chn,s32 fileno,u8 attr,cardcallback callback);

/**
 * Not finished function
*/
s32 CARD_Format(s32 chn);
/**
 * Not finished function
*/
s32 CARD_FormatAsync(s32 chn,cardcallback callback);


/*! \fn s32 CARD_SetCompany(const char *company)
\brief Set additional file attributes. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetCompany(const char *company);


/*! \fn s32 CARD_SetGamecode(const char *gamecode)
\brief Set additional file attributes. This function returns immediately. Asynchronous version.
\param[in] chn CARD slot.

\return \ref card_errors "card error codes"
*/
s32 CARD_SetGamecode(const char *gamecode);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
