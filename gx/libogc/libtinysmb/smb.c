/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * Copyright softdev
 * Modified by Tantric to utilize NTLM authentication
 * PathInfo added by rodries
 * SMB devoptab by scip, rodries
 *
 * You will find WireShark (http://www.wireshark.org/)
 * invaluable for debugging SAMBA implementations.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	http://www.ubiqx.org/cifs/SMB.html
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/

#include <asm.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <wchar.h>
#include <gccore.h>
#include <network.h>
#include <processor.h>
#include <lwp_threads.h>
#include <lwp_objmgr.h>
#include <ogc/lwp_watchdog.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <smb.h>

#define IOS_O_NONBLOCK				0x04
#define RECV_TIMEOUT				3000  // in ms
#define CONN_TIMEOUT				6000

/**
 * Field offsets.
 */
#define SMB_OFFSET_PROTO			0
#define SMB_OFFSET_CMD				4
#define SMB_OFFSET_NTSTATUS			5
#define SMB_OFFSET_ECLASS			5
#define SMB_OFFSET_ECODE			7
#define SMB_OFFSET_FLAGS			9
#define SMB_OFFSET_FLAGS2			10
#define SMB_OFFSET_EXTRA			12
#define SMB_OFFSET_TID				24
#define SMB_OFFSET_PID				26
#define SMB_OFFSET_UID				28
#define SMB_OFFSET_MID				30
#define SMB_HEADER_SIZE				32		/*** SMB Headers are always 32 bytes long ***/

/**
 * Message / Commands
 */
#define NBT_SESSISON_MSG			0x00

#define SMB_NEG_PROTOCOL			0x72
#define SMB_SETUP_ANDX				0x73
#define SMB_TREEC_ANDX				0x75


#define NBT_KEEPALIVE_MSG			0x85
#define KEEPALIVE_SIZE				4

/**
 * SMBTrans2
 */
#define SMB_TRANS2					0x32

#define SMB_OPEN2					0
#define SMB_FIND_FIRST2				1
#define SMB_FIND_NEXT2				2
#define SMB_QUERY_FS_INFO			3
#define SMB_QUERY_PATH_INFO			5
#define SMB_SET_PATH_INFO			6
#define SMB_QUERY_FILE_INFO			7
#define SMB_SET_FILE_INFO			8
#define SMB_CREATE_DIR				13
#define SMB_FIND_CLOSE2				0x34
#define SMB_QUERY_FILE_ALL_INFO		0x107

/**
 * File I/O
 */
#define SMB_OPEN_ANDX				0x2d
#define SMB_WRITE_ANDX				0x2f
#define SMB_READ_ANDX				0x2e
#define SMB_CLOSE					0x04

/**
 * SMB_COM
 */
#define SMB_COM_CREATE_DIRECTORY    0x00
#define SMB_COM_DELETE_DIRECTORY    0x01
#define SMB_COM_DELETE              0x06
#define SMB_COM_RENAME              0x07
#define SMB_COM_QUERY_INFORMATION_DISK  0x80

/**
 * TRANS2 Offsets
 */
#define T2_WORD_CNT				    (SMB_HEADER_SIZE)
#define T2_PRM_CNT				    (T2_WORD_CNT+1)
#define T2_DATA_CNT				    (T2_PRM_CNT+2)
#define T2_MAXPRM_CNT			    (T2_DATA_CNT+2)
#define T2_MAXBUFFER			    (T2_MAXPRM_CNT+2)
#define T2_SETUP_CNT			    (T2_MAXBUFFER+2)
#define T2_SPRM_CNT				    (T2_SETUP_CNT+10)
#define T2_SPRM_OFS				    (T2_SPRM_CNT+2)
#define T2_SDATA_CNT			    (T2_SPRM_OFS+2)
#define T2_SDATA_OFS			    (T2_SDATA_CNT+2)
#define T2_SSETUP_CNT			    (T2_SDATA_OFS+2)
#define T2_SUB_CMD				    (T2_SSETUP_CNT+2)
#define T2_BYTE_CNT				    (T2_SUB_CMD+2)


#define SMB_PROTO					0x424d53ff
#define SMB_HANDLE_NULL				0xffffffff
#define SMB_MAX_NET_READ_SIZE		(16*1024) // see smb_recv
#define SMB_MAX_NET_WRITE_SIZE		4096 // see smb_sendv
#define SMB_MAX_TRANSMIT_SIZE		65472

#define CAP_LARGE_FILES				0x00000008  // 64-bit file sizes and offsets supported
#define CAP_UNICODE					0x00000004  // Unicode supported
#define	CIFS_FLAGS1					0x08 // Paths are caseless
#define CIFS_FLAGS2_UNICODE			0x8001 // Server may return long components in paths in the response - use 0x8001 for Unicode support
#define CIFS_FLAGS2					0x0001 // Server may return long components in paths in the response - use 0x0001 for ASCII support

#define SMB_CONNHANDLES_MAX			8
#define SMB_FILEHANDLES_MAX			(32*SMB_CONNHANDLES_MAX)

#define SMB_OBJTYPE_HANDLE			7
#define SMB_CHECK_HANDLE(hndl)		\
{									\
	if(((hndl)==SMB_HANDLE_NULL) || (LWP_OBJTYPE(hndl)!=SMB_OBJTYPE_HANDLE))	\
		return NULL;				\
}

/* NBT Session Service Packet Type Codes
 */

#define SESS_MSG          0x00
#define SESS_REQ          0x81
#define SESS_POS_RESP     0x82
#define SESS_NEG_RESP     0x83
#define SESS_RETARGET     0x84
#define SESS_KEEPALIVE    0x85

struct _smbfile
{
	lwp_node node;
	u16 sfid;
	SMBCONN conn;
};

/**
 * NBT/SMB Wrapper
 */
typedef struct _nbtsmb
{
  u8 msg;		 /*** NBT Message ***/
  u8 length_high;
  u16 length;	 /*** Length, excluding NBT ***/
  u8 smb[SMB_MAX_TRANSMIT_SIZE+128];
} NBTSMB;

/**
 * Session Information
 */
typedef struct _smbsession
{
  u16 TID;
  u16 PID;
  u16 UID;
  u16 MID;
  u32 sKey;
  u32 capabilities;
  u32 MaxBuffer;
  u16 MaxMpx;
  u16 MaxVCS;
  u8 challenge[10];
  u8 p_domain[64];
  s64 timeOffset;
  u16 count;
  u16 eos;
  bool challengeUsed;
  u8 securityLevel;
} SMBSESSION;

typedef struct _smbhandle
{
	lwp_obj object;
	char *user;
	char *pwd;
	char *share_name;
	char *server_name;
	s32 sck_server;
	struct sockaddr_in server_addr;
	bool conn_valid;
	SMBSESSION session;
	NBTSMB message;
	bool unicode;
} SMBHANDLE;

static u32 smb_dialectcnt = 1;
static bool smb_inited = false;
static lwp_objinfo smb_handle_objects;
static lwp_queue smb_filehandle_queue;
static struct _smbfile smb_filehandles[SMB_FILEHANDLES_MAX];
static const char *smb_dialects[] = {"NT LM 0.12",NULL};

extern void ntlm_smb_nt_encrypt(const char *passwd, const u8 * challenge, u8 * answer);

// UTF conversion functions
size_t utf16_to_utf8(char* dst, char* src, size_t len)
{
	mbstate_t ps;
	size_t count = 0;
	int bytes;
	char buff[MB_CUR_MAX];
	int i;
	unsigned short c;
	memset(&ps, 0, sizeof(mbstate_t));

	while (count < len && *src != '\0')
	{
		c = *(src + 1) << 8 | *src; // little endian
		if (c == 0)
			break;
		bytes = wcrtomb(buff, c, &ps);
		if (bytes < 0)
		{
			*dst = '\0';
			return -1;
		}
		if (bytes > 0)
		{
			for (i = 0; i < bytes; i++)
			{
				*dst++ = buff[i];
			}
			src += 2;
			count += 2;
		}
		else
		{
			break;
		}
	}
	*dst = '\0';
	return count;
}

size_t utf8_to_utf16(char* dst, char* src, size_t len)
{
	mbstate_t ps;
	wchar_t tempWChar;
	char *tempChar;
	int bytes;
	size_t count = 0;
	tempChar = (char*) &tempWChar;
	memset(&ps, 0, sizeof(mbstate_t));

	while (count < len - 1 && src != '\0')
	{
		bytes = mbrtowc(&tempWChar, src, MB_CUR_MAX, &ps);
		if (bytes > 0)
		{
			*dst = tempChar[3];
			dst++;
			*dst = tempChar[2];
			dst++;
			src += bytes;
			count += 2;
		}
		else if (bytes == 0)
		{
			break;
		}
		else
		{
			*dst = '\0';
			dst++;
			*dst = '\0';
			return -1;
		}
	}
	*dst = '\0';
	dst++;
	*dst = '\0';
	return count;
}

/**
 * SMB Endian aware supporting functions
 *
 * SMB always uses Intel Little-Endian values, so htons etc are
 * of little or no use :) ... Thanks M$
 */

/*** get unsigned char ***/
static __inline__ u8 getUChar(u8 *buffer,u32 offset)
{
	return (u8)buffer[offset];
}

/*** set unsigned char ***/
static __inline__ void setUChar(u8 *buffer,u32 offset,u8 value)
{
	buffer[offset] = value;
}

/*** get signed short ***/
static __inline__ s16 getShort(u8 *buffer,u32 offset)
{
	return (s16)((buffer[offset+1]<<8)|(buffer[offset]));
}

/*** get unsigned short ***/
static __inline__ u16 getUShort(u8 *buffer,u32 offset)
{
	return (u16)((buffer[offset+1]<<8)|(buffer[offset]));
}

/*** set unsigned short ***/
static __inline__ void setUShort(u8 *buffer,u32 offset,u16 value)
{
	buffer[offset] = (value&0xff);
	buffer[offset+1] = ((value&0xff00)>>8);
}

/*** get unsigned int ***/
static __inline__ u32 getUInt(u8 *buffer,u32 offset)
{
	return (u32)((buffer[offset+3]<<24)|(buffer[offset+2]<<16)|(buffer[offset+1]<<8)|buffer[offset]);
}

/*** set unsigned int ***/
static __inline__ void setUInt(u8 *buffer,u32 offset,u32 value)
{
	buffer[offset] = (value&0xff);
	buffer[offset+1] = ((value&0xff00)>>8);
	buffer[offset+2] = ((value&0xff0000)>>16);
	buffer[offset+3] = ((value&0xff000000)>>24);
}

/*** get unsigned long long ***/
static __inline__ u64 getULongLong(u8 *buffer,u32 offset)
{
	return (u64)(getUInt(buffer, offset) | (u64)getUInt(buffer, offset+4) << 32);
}

static __inline__ SMBHANDLE* __smb_handle_open(SMBCONN smbhndl)
{
	u32 level;
	SMBHANDLE *handle;

	SMB_CHECK_HANDLE(smbhndl);

	_CPU_ISR_Disable(level);
	handle = (SMBHANDLE*)__lwp_objmgr_getnoprotection(&smb_handle_objects,LWP_OBJMASKID(smbhndl));
	_CPU_ISR_Restore(level);
	return handle;
}


static __inline__ void __smb_handle_free(SMBHANDLE *handle)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_objmgr_close(&smb_handle_objects,&handle->object);
	__lwp_objmgr_free(&smb_handle_objects,&handle->object);
	_CPU_ISR_Restore(level);
}

static void __smb_init()
{
	smb_inited = true;
	__lwp_objmgr_initinfo(&smb_handle_objects,SMB_CONNHANDLES_MAX,sizeof(SMBHANDLE));
	__lwp_queue_initialize(&smb_filehandle_queue,smb_filehandles,SMB_FILEHANDLES_MAX,sizeof(struct _smbfile));
}

static SMBHANDLE* __smb_allocate_handle()
{
	u32 level;
	SMBHANDLE *handle;

	_CPU_ISR_Disable(level);
	handle = (SMBHANDLE*)__lwp_objmgr_allocate(&smb_handle_objects);
	if(handle) {
		handle->user = NULL;
		handle->pwd = NULL;
		handle->server_name = NULL;
		handle->share_name = NULL;
		handle->sck_server = INVALID_SOCKET;
		handle->conn_valid = false;
		__lwp_objmgr_open(&smb_handle_objects,&handle->object);
	}
	_CPU_ISR_Restore(level);
	return handle;
}

static void __smb_free_handle(SMBHANDLE *handle)
{
	if(handle->user) free(handle->user);
	if(handle->pwd) free(handle->pwd);
	if(handle->server_name) free(handle->server_name);
	if(handle->share_name) free(handle->share_name);

	handle->user = NULL;
	handle->pwd = NULL;
	handle->server_name = NULL;
	handle->share_name = NULL;
	handle->sck_server = INVALID_SOCKET;

	__smb_handle_free(handle);
}

static void MakeSMBHeader(u8 command,u8 flags,u16 flags2,SMBHANDLE *handle)
{
	u8 *ptr = handle->message.smb;
	NBTSMB *nbt = &handle->message;
	SMBSESSION *sess = &handle->session;

	memset(nbt,0,sizeof(NBTSMB));

	setUInt(ptr,SMB_OFFSET_PROTO,SMB_PROTO);
	setUChar(ptr,SMB_OFFSET_CMD,command);
	setUChar(ptr,SMB_OFFSET_FLAGS,flags);
	setUShort(ptr,SMB_OFFSET_FLAGS2,flags2);
	setUShort(ptr,SMB_OFFSET_TID,sess->TID);
	setUShort(ptr,SMB_OFFSET_PID,sess->PID);
	setUShort(ptr,SMB_OFFSET_UID,sess->UID);
	setUShort(ptr,SMB_OFFSET_MID,sess->MID);

	ptr[SMB_HEADER_SIZE] = 0;
}

/**
 * MakeTRANS2Hdr
 */
static void MakeTRANS2Header(u8 subcommand,SMBHANDLE *handle)
{
	u8 *ptr = handle->message.smb;

	setUChar(ptr, T2_WORD_CNT, 15);
	setUShort(ptr, T2_MAXPRM_CNT, 10);
	setUShort(ptr, T2_MAXBUFFER, 16384);
	setUChar(ptr, T2_SSETUP_CNT, 1);
	setUShort(ptr, T2_SUB_CMD, subcommand);
}

/**
 * smb_send
 *
 * blocking call with timeout
 * will return when ALL data has been sent. Number of bytes sent is returned.
 * OR timeout. Timeout will return -1
 * OR network error. -ve value will be returned
 */
static inline s32 smb_send(s32 s,const void *data,s32 size)
{
	u64 t1,t2;
	s32 ret, len = size, nextsend;

	t1=ticks_to_millisecs(gettime());
	while(len>0)
	{
		nextsend=len;

		if(nextsend>SMB_MAX_NET_WRITE_SIZE)
			nextsend=SMB_MAX_NET_WRITE_SIZE; //optimized value

		ret=net_send(s,data,nextsend,0);
		if(ret==-EAGAIN)
		{
			t2=ticks_to_millisecs(gettime());
			if( (t2 - t1) > RECV_TIMEOUT)
			{
				return -1; // timeout
			}
			usleep(100); // allow system to perform work. Stabilizes system
			continue;
		}
		else if(ret<0)
		{
			return ret;	// an error occurred
		}
		else
		{
			data+=ret;
			len-=ret;
			if(len==0) return size;
			t1=ticks_to_millisecs(gettime());
		}
		usleep(100);
	}
	return size;
}

/**
 * smb_recv
 *
 * blocking call with timeout
 * will return when ANY data has been read from socket. Number of bytes read is returned.
 * OR timeout. Timeout will return -1
 * OR network error. -ve value will be returned
 */
static s32 smb_recv(s32 s,void *mem,s32 len)
{
	s32 ret,read,readtotal=0;
	u64 t1,t2;

	t1=ticks_to_millisecs(gettime());
	while(len > 0)
	{
		read=len;
		if(read>SMB_MAX_NET_READ_SIZE)
			read=SMB_MAX_NET_READ_SIZE; // optimized value

		ret=net_recv(s,mem+readtotal,read,0);
		if(ret>0)
		{
			readtotal+=ret;
			len-=ret;
			if(len==0) return readtotal;
		}
		else
		{
			if(ret!=-EAGAIN) return ret;
			t2=ticks_to_millisecs(gettime());
			if( (t2 - t1) > RECV_TIMEOUT) return -1;
		}
		usleep(1000);
	}
	return readtotal;
}

static void clear_network(s32 s,u8 *ptr)
{
	u64 t1,t2;

	t1=ticks_to_millisecs(gettime());
	while(true)
	{
		net_recv(s,ptr,SMB_MAX_NET_READ_SIZE,0);

		t2=ticks_to_millisecs(gettime());
		if( (t2 - t1) > 600) return;
		usleep(100);
	}
}

/**
 * SMBCheck
 *
 * Do very basic checking on the return SMB
 * Read <readlen> bytes
 * if <readlen>==0 then read a single SMB packet
 * discard any non NBT_SESSISON_MSG packets along the way.
 */
static s32 SMBCheck(u8 command,SMBHANDLE *handle)
{
	s32 ret;
	u8 *ptr = handle->message.smb;
	NBTSMB *nbt = &handle->message;
	u32 readlen;
	u64 t1,t2;

	if(handle->sck_server == INVALID_SOCKET) return SMB_ERROR;

	memset(nbt,0xFF,sizeof(NBTSMB)); //NBT_SESSISON_MSG is 0x00 so fill mem with 0xFF

	t1=ticks_to_millisecs(gettime());

	/*keep going till we get a NBT session message*/
	do{
		ret=smb_recv(handle->sck_server, (u8*)nbt, 4);
		if(ret!=4) goto failed;

		if(nbt->msg!=NBT_SESSISON_MSG)
		{
			readlen=(u32)((nbt->length_high<<16)|nbt->length);
			if(readlen>0)
			{
				t1=ticks_to_millisecs(gettime());
				smb_recv(handle->sck_server, ptr, readlen); //clear unexpected NBT message
			}
		}
		t2=ticks_to_millisecs(gettime());
		if( (t2 - t1) > RECV_TIMEOUT * 2) goto failed;

	} while(nbt->msg!=NBT_SESSISON_MSG);

	/* obtain required length from NBT header if readlen==0*/
	readlen=(u32)((nbt->length_high<<16)|nbt->length);

	// Get server message block
	ret=smb_recv(handle->sck_server, ptr, readlen);
	if(readlen!=ret) goto failed;

	/*** Do basic SMB Header checks ***/
	ret = getUInt(ptr,SMB_OFFSET_PROTO);
	if(ret!=SMB_PROTO) goto failed;

	ret = getUChar(ptr, SMB_OFFSET_CMD);
	if(ret!=command) goto failed;

	ret = getUInt(ptr,SMB_OFFSET_NTSTATUS);
	if(ret) goto failed;

	return SMB_SUCCESS;
failed:
    clear_network(handle->sck_server,ptr);
    return SMB_ERROR;
}

/**
 * SMB_SetupAndX
 *
 * Setup the SMB session, including authentication with the
 * magic 'NTLM Response'
 */
static s32 SMB_SetupAndX(SMBHANDLE *handle)
{
	s32 pos;
	s32 bcpos;
	s32 i, ret;
	u8 *ptr = handle->message.smb;
	SMBSESSION *sess = &handle->session;
	char pwd[30], ntRespData[24];

	if(handle->sck_server == INVALID_SOCKET) return SMB_ERROR;

	MakeSMBHeader(SMB_SETUP_ANDX,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);
	pos = SMB_HEADER_SIZE;

	setUChar(ptr,pos,13);
	pos++;				    /*** Word Count ***/
	setUChar(ptr,pos,0xff);
	pos++;				      /*** Next AndX ***/
	setUChar(ptr,pos,0);
	pos++;   /*** Reserved ***/
	pos += 2; /*** Next AndX Offset ***/
	setUShort(ptr,pos,sess->MaxBuffer);
	pos += 2;
	setUShort(ptr,pos,sess->MaxMpx);
	pos += 2;
	setUShort(ptr,pos,sess->MaxVCS);
	pos += 2;
	setUInt(ptr,pos,sess->sKey);
	pos += 4;
	setUShort(ptr,pos,24);	/*** Password length (case-insensitive) ***/
	pos += 2;
	setUShort(ptr,pos,24);	/*** Password length (case-sensitive) ***/
	pos += 2;
	setUInt(ptr,pos,0);
	pos += 4; /*** Reserved ***/
	setUInt(ptr,pos,sess->capabilities);
	pos += 4; /*** Capabilities ***/
	bcpos = pos;
	pos += 2; /*** Byte count ***/

	/*** The magic 'NTLM Response' ***/
	strcpy(pwd, handle->pwd);
	if (sess->challengeUsed)
		ntlm_smb_nt_encrypt((const char *) pwd, (const u8 *) sess->challenge, (u8*) ntRespData);

	/*** Build information ***/
	memset(&ptr[pos],0,24);
	pos += 24;
	memcpy(&ptr[pos],ntRespData,24);
	pos += 24;
	pos++;
	/*** Account ***/
	strcpy(pwd, handle->user);
	for(i=0;i<strlen(pwd);i++)
        pwd[i] = toupper((int)pwd[i]);
	if(handle->unicode)
	{
        pos += utf8_to_utf16((char*)&ptr[pos],pwd,SMB_MAXPATH-2);
        pos += 2;
	}
	else
	{
        memcpy(&ptr[pos],pwd,strlen(pwd));
        pos += strlen(pwd)+1;
	}

	/*** Primary Domain ***/
	if(handle->user[0]=='\0') sess->p_domain[0] = '\0';
	if(handle->unicode)
	{
        pos += utf8_to_utf16((char*)&ptr[pos],(char*)sess->p_domain,SMB_MAXPATH-2);
        pos += 2;
	}
	else
	{
        memcpy(&ptr[pos],sess->p_domain,strlen((const char*)sess->p_domain));
        pos += strlen((const char*)sess->p_domain)+1;
	}

	/*** Native OS ***/
    strcpy(pwd,"Unix (libOGC)");
	if(handle->unicode)
	{
        pos += utf8_to_utf16((char*)&ptr[pos],pwd,SMB_MAXPATH-2);
        pos += 2;
	}
	else
	{
        memcpy(&ptr[pos],pwd,strlen(pwd));
        pos += strlen(pwd)+1;
	}

	/*** Native LAN Manager ***/
	strcpy(pwd,"Nintendo Wii");
	if(handle->unicode)
	{
        pos += utf8_to_utf16((char*)&ptr[pos],pwd,SMB_MAXPATH-2);
        pos += 2;
	}
	else
	{
        memcpy(&ptr[pos],pwd,strlen(pwd));
        pos += strlen (pwd)+1;
	}

	/*** Update byte count ***/
	setUShort(ptr,bcpos,((pos-bcpos)-2));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons (pos);
	pos += 4;

	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<=0) return SMB_ERROR;

	if((ret=SMBCheck(SMB_SETUP_ANDX,handle))==SMB_SUCCESS) {
		/*** Collect UID ***/
		sess->UID = getUShort(handle->message.smb,SMB_OFFSET_UID);
		return SMB_SUCCESS;
	}
	return ret;
}

/**
 * SMB_TreeAndX
 *
 * Finally, net_connect to the remote share
 */
static s32 SMB_TreeAndX(SMBHANDLE *handle)
{
	s32 pos, bcpos, ret;
	char path[512];
	u8 *ptr = handle->message.smb;
	SMBSESSION *sess = &handle->session;

	if(handle->sck_server == INVALID_SOCKET) return SMB_ERROR;

	MakeSMBHeader(SMB_TREEC_ANDX,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);
	pos = SMB_HEADER_SIZE;

	setUChar(ptr,pos,4);
	pos++;				    /*** Word Count ***/
	setUChar(ptr,pos,0xff);
	pos++;				    /*** Next AndX ***/
	pos++;   /*** Reserved ***/
	pos += 2; /*** Next AndX Offset ***/
	pos += 2; /*** Flags ***/
	setUShort(ptr,pos,1);
	pos += 2;				    /*** Password Length ***/
	bcpos = pos;
	pos += 2;
	pos++;    /*** NULL Password ***/

	/*** Build server share path ***/
	strcpy ((char*)path, "\\\\");
	strcat ((char*)path, handle->server_name);
	strcat ((char*)path, "\\");
	strcat ((char*)path, handle->share_name);

	for(ret=0;ret<strlen((const char*)path);ret++)
        path[ret] = (char)toupper((int)path[ret]);

	if(handle->unicode)
	{
        pos += utf8_to_utf16((char*)&ptr[pos],path,SMB_MAXPATH-2);
        pos += 2;
	}
	else
	{
        memcpy(&ptr[pos],path,strlen((const char*)path));
        pos += strlen((const char*)path)+1;
	}

	/*** Service ***/
	strcpy((char*)path,"?????");
	memcpy(&ptr[pos],path,strlen((const char*)path));
	pos += strlen((const char*)path)+1;


	/*** Update byte count ***/
	setUShort(ptr,bcpos,(pos-bcpos)-2);

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons (pos);
	pos += 4;

	ret = smb_send(handle->sck_server,(char *)&handle->message,pos);
	if(ret<=0) return SMB_ERROR;

	if((ret=SMBCheck(SMB_TREEC_ANDX,handle))==SMB_SUCCESS) {
		/*** Collect Tree ID ***/
		sess->TID = getUShort(handle->message.smb,SMB_OFFSET_TID);
		return SMB_SUCCESS;
	}
	return ret;
}

/**
 * SMB_NegotiateProtocol
 *
 * The only protocol we admit to is 'NT LM 0.12'
 */
static s32 SMB_NegotiateProtocol(const char *dialects[],int dialectc,SMBHANDLE *handle)
{
	u8 *ptr;
	s32 pos;
	s32 bcnt,i,j;
	s32 ret,len;
	u32 serverMaxBuffer;
	SMBSESSION *sess;

	if(!handle || !dialects || dialectc<=0)
		return SMB_ERROR;

	if(handle->sck_server == INVALID_SOCKET) return SMB_ERROR;

	/*** Clear session variables ***/
	sess = &handle->session;
	memset(sess,0,sizeof(SMBSESSION));
	sess->PID = 0xdead;
	sess->MID = 1;
	sess->capabilities = 0;

	MakeSMBHeader(SMB_NEG_PROTOCOL,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE+3;
	ptr = handle->message.smb;
	for(i=0,bcnt=0;i<dialectc;i++) {
		len = strlen(dialects[i])+1;
		ptr[pos++] = '\x02';
		memcpy(&ptr[pos],dialects[i],len);
		pos += len;
		bcnt += len+1;
	}
	/*** Update byte count ***/
	setUShort(ptr,(SMB_HEADER_SIZE+1),bcnt);

	/*** Set NBT information ***/
	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);
	pos += 4;

	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<=0) return SMB_ERROR;

	/*** Check response ***/
	if((ret=SMBCheck(SMB_NEG_PROTOCOL,handle))==SMB_SUCCESS)
	{
		pos = SMB_HEADER_SIZE;
		ptr = handle->message.smb;

		/*** Collect information ***/
		if(getUChar(ptr,pos)!=17) return SMB_PROTO_FAIL;	// UCHAR WordCount; Count of parameter words = 17

		pos++;
		if(getUShort(ptr,pos)!=0) return SMB_PROTO_FAIL;	// USHORT DialectIndex; Index of selected dialect - should always be 0 since we only supplied 1!

		pos += 2;
		if(getUChar(ptr,pos) & 1)
		{
			// user level security
			sess->securityLevel = 1;
		}
		else
		{
			// share level security - can we skip SetupAndX? If so, we would need to specify the password in TreeAndX
			sess->securityLevel = 0;
		}

		pos++;
		sess->MaxMpx = getUShort(ptr, pos);	//USHORT MaxMpxCount; Max pending outstanding requests

		pos += 2;
		sess->MaxVCS = getUShort(ptr, pos);	//USHORT MaxNumberVcs; Max VCs between client and server

		pos += 2;
		serverMaxBuffer = getUInt(ptr, pos);	//ULONG MaxBufferSize; Max transmit buffer size


		if(serverMaxBuffer>SMB_MAX_TRANSMIT_SIZE)
			sess->MaxBuffer = SMB_MAX_TRANSMIT_SIZE;
		else
			sess->MaxBuffer = serverMaxBuffer;
		pos += 4;
		pos += 4;	//ULONG MaxRawSize; Maximum raw buffer size
		sess->sKey = getUInt(ptr,pos); pos += 4;
		u32 servcap = getUInt(ptr,pos); pos += 4; //ULONG Capabilities; Server capabilities
		pos += 4;	//ULONG SystemTimeLow; System (UTC) time of the server (low).
		pos += 4;	//ULONG SystemTimeHigh; System (UTC) time of the server (high).
		sess->timeOffset = getShort(ptr,pos) * 600000000LL; pos += 2; //SHORT ServerTimeZone; Time zone of server (minutes from UTC)

		//UCHAR EncryptionKeyLength - 0 or 8
		if(getUChar(ptr,pos)!=8)
		{
			if (getUChar(ptr,pos)!=0)
			{
				return SMB_BAD_KEYLEN;
			}
			else
			{
				// Challenge key not used
				sess->challengeUsed = false;
			}
		}
		else
		{
			sess->challengeUsed = true;
		}

		pos++;
		getUShort(ptr,pos); // byte count

		if (sess->challengeUsed)
		{
			/*** Copy challenge key ***/
			pos += 2;
			memcpy(&sess->challenge,&ptr[pos],8);
		}

		/*** Primary domain ***/
		pos += 8;
		i = j = 0;
		while(ptr[pos+j]!=0) {
			sess->p_domain[i] = ptr[pos+j];
			j += 2;
			i++;
		}
		sess->p_domain[i] = '\0';

		// setup capabilities
		//if(servcap & CAP_LARGE_FILES)
		//	sess->capabilities |= CAP_LARGE_FILES;

		if(servcap & CAP_UNICODE)
		{
			sess->capabilities |= CAP_UNICODE;
			handle->unicode = true;
		}

		return SMB_SUCCESS;
	}
	return ret;
}

static s32 do_netconnect(SMBHANDLE *handle)
{
	u32 set = 1;
	s32 ret;
	s32 sock;
	u64 t1,t2;

	handle->sck_server = INVALID_SOCKET;
	/*** Create the global net_socket ***/
	sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(sock==INVALID_SOCKET) return -1;

	// Switch off Nagle with TCP_NODELAY
	net_setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&set,sizeof(set));

	// create non blocking socket
	ret = net_ioctl(sock, FIONBIO, &set);
	if (ret < 0)
	{
		net_close(sock);
		return ret;
	}

	t1=ticks_to_millisecs(gettime());
	while(1)
	{
		ret = net_connect(sock,(struct sockaddr*)&handle->server_addr,sizeof(handle->server_addr));
		if(ret==-EISCONN) break;
		t2=ticks_to_millisecs(gettime());
		usleep(1000);
		if((t2-t1) > CONN_TIMEOUT) break; // usually not more than 90ms
	}

	if(ret!=-EISCONN)
	{
		net_close(sock);
		return -1;
	}

	handle->sck_server = sock;
	return 0;
}

static s32 do_smbconnect(SMBHANDLE *handle)
{
	s32 ret;

	if(handle->sck_server == INVALID_SOCKET) return -1;

	ret = SMB_NegotiateProtocol(smb_dialects,smb_dialectcnt,handle);
	if(ret!=SMB_SUCCESS)
	{
		net_close(handle->sck_server);
		handle->sck_server = INVALID_SOCKET;
		return -1;
	}

	ret = SMB_SetupAndX(handle);
	if(ret!=SMB_SUCCESS)
	{
		net_close(handle->sck_server);
		handle->sck_server = INVALID_SOCKET;
		return -1;
	}

	ret = SMB_TreeAndX(handle);
	if(ret!=SMB_SUCCESS)
	{
		net_close(handle->sck_server);
		handle->sck_server = INVALID_SOCKET;
		return -1;
	}

	handle->conn_valid = true;
	return 0;
}

/****************************************************************************
 * Create an NBT SESSION REQUEST message.
 ****************************************************************************/
static int MakeSessReq(unsigned char *bufr, unsigned char *Called,	unsigned char *Calling)
{
	// Write the header.
	bufr[0] = SESS_REQ;
	bufr[1] = 0;
	bufr[2] = 0;
	bufr[3] = 68; // 2x34 bytes in length.

	// Copy the Called and Calling names into the buffer.
	(void) memcpy(&bufr[4], Called, 34);
	(void) memcpy(&bufr[38], Calling, 34);

	// Return the total message length.
	return 72;
}

static unsigned char *L1_Encode(unsigned char *dst, const unsigned char *name,
		const unsigned char pad, const unsigned char sfx)
{
	int i = 0;
	int j = 0;
	int k = 0;

	while (('\0' != name[i]) && (i < 15))
	{
		k = toupper(name[i++]);
		dst[j++] = 'A' + ((k & 0xF0) >> 4);
		dst[j++] = 'A' + (k & 0x0F);
	}

	i = 'A' + ((pad & 0xF0) >> 4);
	k = 'A' + (pad & 0x0F);
	while (j < 30)
	{
		dst[j++] = i;
		dst[j++] = k;
	}

	dst[30] = 'A' + ((sfx & 0xF0) >> 4);
	dst[31] = 'A' + (sfx & 0x0F);
	dst[32] = '\0';

	return (dst);
}

static int L2_Encode(unsigned char *dst, const unsigned char *name,
		const unsigned char pad, const unsigned char sfx,
		const unsigned char *scope)
{
	int lenpos;
	int i;
	int j;

	if (NULL == L1_Encode(&dst[1], name, pad, sfx))
		return (-1);

	dst[0] = 0x20;
	lenpos = 33;

	if ('\0' != *scope)
	{
		do
		{
			for (i = 0, j = (lenpos + 1); ('.' != scope[i]) && ('\0'
					!= scope[i]); i++, j++)
				dst[j] = toupper(scope[i]);

			dst[lenpos] = (unsigned char) i;
			lenpos += i + 1;
			scope += i;
		} while ('.' == *(scope++));

		dst[lenpos] = '\0';
	}

	return (lenpos + 1);
}

/****************************************************************************
 * Send an NBT SESSION REQUEST over the TCP connection, then wait for a reply.
 ****************************************************************************/
static s32 SMB_RequestNBTSession(SMBHANDLE *handle)
{
	unsigned char Called[34];
	unsigned char Calling[34];
	unsigned char bufr[128];
	int result;

	if(handle->sck_server == INVALID_SOCKET) return -1;

	L2_Encode(Called, (const unsigned char*) "*SMBSERVER", 0x20, 0x20,
			(const unsigned char*) "");
	L2_Encode(Calling, (const unsigned char*) "SMBCLIENT", 0x20, 0x00,
			(const unsigned char*) "");

	// Create the NBT Session Request message.
	result = MakeSessReq(bufr, Called, Calling);

	//Send the NBT Session Request message.
	result = smb_send(handle->sck_server, bufr, result);
	if (result < 0)
	{
		// Error sending Session Request message
		return -1;
	}

	// Now wait for and handle the reply (2 seconds).
	result = smb_recv(handle->sck_server, bufr, 128);
	if (result <= 0)
	{
		// Timeout waiting for NBT Session Response
		return -1;
	}

	switch (*bufr)
	{
		case SESS_POS_RESP:
			// Positive Session Response
			return 0;

		case SESS_NEG_RESP:
			// Negative Session Response
			return -1;

		case SESS_RETARGET:
			// Retarget Session Response
			return -1;

		default:
			// Unexpected Session Response
			return -1;
	}
}

/****************************************************************************
 * Primary setup, logon and connection all in one :)
 ****************************************************************************/
s32 SMB_Connect(SMBCONN *smbhndl, const char *user, const char *password, const char *share, const char *server)
{
	s32 ret = 0;
	SMBHANDLE *handle;
	struct in_addr val;

	*smbhndl = SMB_HANDLE_NULL;

	if(!user || !password || !share || !server ||
		strlen(user) > 20 || strlen(password) > 14 ||
		strlen(share) > 80 || strlen(server) > 80)
	{
		return SMB_BAD_LOGINDATA;
	}

	if(!smb_inited)
	{
		u32 level;
		_CPU_ISR_Disable(level);
		__smb_init();
		_CPU_ISR_Restore(level);
	}

	handle = __smb_allocate_handle();
	if(!handle) return SMB_ERROR;

	handle->user = strdup(user);
	handle->pwd = strdup(password);
	handle->server_name = strdup(server);
	handle->share_name = strdup(share);
	handle->server_addr.sin_family = AF_INET;
	handle->server_addr.sin_port = htons(445);
	handle->unicode = false;

	if(strlen(server) < 16 && inet_aton(server, &val))
	{
		handle->server_addr.sin_addr.s_addr = val.s_addr;
	}
	else // might be a hostname
	{
#ifdef HW_RVL
		struct hostent *hp = net_gethostbyname(server);
		if (!hp || !(hp->h_addrtype == PF_INET))
			ret = SMB_BAD_LOGINDATA;
		else
			memcpy((char *)&handle->server_addr.sin_addr.s_addr, hp->h_addr_list[0], hp->h_length);
#else
		__smb_free_handle(handle);
		return SMB_ERROR;
#endif
	}

	*smbhndl =(SMBCONN)(LWP_OBJMASKTYPE(SMB_OBJTYPE_HANDLE)|LWP_OBJMASKID(handle->object.id));

	if(ret==0)
	{
		ret = do_netconnect(handle);
		if(ret==0) ret = do_smbconnect(handle);

		if(ret!=0)
		{
			// try port 139
			handle->server_addr.sin_port = htons(139);
			ret = do_netconnect(handle);
			if(ret==0) ret = SMB_RequestNBTSession(handle);
			if(ret==0) ret = do_smbconnect(handle);
		}
	}
	if(ret!=0)
	{
		__smb_free_handle(handle);
		return SMB_ERROR;
	}

	return SMB_SUCCESS;
}

/****************************************************************************
 * SMB_Destroy
 ****************************************************************************/
void SMB_Close(SMBCONN smbhndl)
{
	SMBHANDLE *handle = __smb_handle_open(smbhndl);
	if(!handle) return;

	if(handle->sck_server!=INVALID_SOCKET)
		net_close(handle->sck_server);

	__smb_free_handle(handle);
}

s32 SMB_Reconnect(SMBCONN *_smbhndl, bool test_conn)
{
	s32 ret = SMB_SUCCESS;
	SMBCONN smbhndl = *_smbhndl;
	SMBHANDLE *handle = __smb_handle_open(smbhndl);
	if(!handle)
		return SMB_ERROR; // we have no handle, so we can't reconnect

	if(handle->conn_valid && test_conn)
	{
		SMBDIRENTRY dentry;
		if(SMB_PathInfo("\\", &dentry, smbhndl)==SMB_SUCCESS) return SMB_SUCCESS; // no need to reconnect
		handle->conn_valid = false; // else connection is invalid
	}
	if(!handle->conn_valid)
	{
		// shut down connection
		if(handle->sck_server!=INVALID_SOCKET)
		{
			net_close(handle->sck_server);
			handle->sck_server = INVALID_SOCKET;
		}

		// reconnect
		if(handle->server_addr.sin_port > 0)
		{
			ret = do_netconnect(handle);
			if(ret==0 && handle->server_addr.sin_port == htons(139))
				ret = SMB_RequestNBTSession(handle);
			if(ret==0)
				ret = do_smbconnect(handle);
		}
		else // initial connection
		{
			handle->server_addr.sin_port = htons(445);
			ret = do_netconnect(handle);
			if(ret==0) ret = do_smbconnect(handle);

			if(ret != 0)
			{
				// try port 139
				handle->server_addr.sin_port = htons(139);
				ret = do_netconnect(handle);
				if(ret==0) ret = SMB_RequestNBTSession(handle);
				if(ret==0) ret = do_smbconnect(handle);
			}

			if(ret != 0)
				handle->server_addr.sin_port = 0;
		}
	}
	return ret;
}

SMBFILE SMB_OpenFile(const char *filename, u16 access, u16 creation,SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	struct _smbfile *fid = NULL;
	SMBHANDLE *handle;
	char realfile[512];

	if(filename == NULL)
		return NULL;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return NULL;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return NULL;

	MakeSMBHeader(SMB_OPEN_ANDX,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 15);
	pos++;			       /*** Word Count ***/
	setUChar(ptr, pos, 0xff);
	pos++;				 /*** AndXCommand 0xFF = None ***/
	setUChar(ptr, pos, 0);
	pos++;				 /*** AndX Reserved must be 0 ***/
	pos += 2;  /*** Next AndX Offset to next Command ***/
	pos += 2;  /*** Flags ***/
	setUShort(ptr, pos, access);
	pos += 2;					 /*** Access mode ***/
	setUShort(ptr, pos, 0x6);
	pos += 2;  /*** Type of file ***/
	pos += 2;  /*** File Attributes ***/
	pos += 4;  /*** File time - don't care - let server decide ***/
	setUShort(ptr, pos, creation);
	pos += 2;  /*** Creation flags ***/
	pos += 4;  /*** Allocation size ***/
	setUInt(ptr, pos, 0);
	pos += 4;  /*** Reserved[0] must be 0 ***/
	setUInt(ptr, pos, 0);
	pos += 4;  /*** Reserved[1] must be 0 ***/
	pos += 2;  /*** Byte Count ***/
	bpos = pos;
	setUChar(ptr, pos, 0x04); /** Bufferformat **/
	pos++;

	realfile[0]='\0';
	if (filename[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,filename);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],realfile,strlen(realfile));
		pos += strlen(realfile)+1;
	}

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) goto failed;

	if(SMBCheck(SMB_OPEN_ANDX,handle)==SMB_SUCCESS) {
		/*** Check file handle ***/
		fid = (struct _smbfile*)__lwp_queue_get(&smb_filehandle_queue);
		if(fid) {
			fid->conn = smbhndl;
			fid->sfid = getUShort(handle->message.smb,(SMB_HEADER_SIZE+5));
		}
	}
	return (SMBFILE)fid;

failed:
	handle->conn_valid = false;
	return NULL;
}

/**
 * SMB_CloseFile
 */
void SMB_CloseFile(SMBFILE sfid)
{
	u8 *ptr;
	s32 pos, ret;
	SMBHANDLE *handle;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return;

	MakeSMBHeader(SMB_CLOSE,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 3);
	pos++;			      /** Word Count **/
	setUShort(ptr, pos, fid->sfid);
	pos += 2;
	setUInt(ptr, pos, 0xffffffff);
	pos += 4;					/*** Last Write ***/
	pos += 2;  /*** Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) handle->conn_valid = false;
	else SMBCheck(SMB_CLOSE,handle);
	__lwp_queue_append(&smb_filehandle_queue,&fid->node);
}

/**
 * SMB_CreateDirectory
 */
s32 SMB_CreateDirectory(const char *dirname, SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	SMBHANDLE *handle;
	char realfile[512];

	if(dirname == NULL)
		return -1;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return -1;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return -1;

	MakeSMBHeader(SMB_COM_CREATE_DIRECTORY,CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 0);
	pos++;			       /*** Word Count ***/
	pos += 2;              /*** Byte Count ***/
	bpos = pos;
    setUChar(ptr, pos, 0x04); /*** Buffer format ***/
	pos++;

	realfile[0]='\0';
	if (dirname[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,dirname);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],realfile,strlen(realfile));
		pos += strlen(realfile)+1;
	}

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret < 0) goto failed;

    ret = SMBCheck(SMB_COM_CREATE_DIRECTORY,handle);

    return ret;

failed:
	return ret;
}

/**
 * SMB_DeleteDirectory
 */
s32 SMB_DeleteDirectory(const char *dirname, SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	SMBHANDLE *handle;
	char realfile[512];

	if(dirname == NULL)
		return -1;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return -1;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return -1;

	MakeSMBHeader(SMB_COM_DELETE_DIRECTORY,CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 0);
	pos++;			       /*** Word Count ***/
	pos += 2;              /*** Byte Count ***/
	bpos = pos;
	setUChar(ptr, pos, 0x04); /** Bufferformat **/
	pos++;

	realfile[0]='\0';
	if (dirname[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,dirname);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],realfile,strlen(realfile));
		pos += strlen(realfile)+1;
	}

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret < 0) goto failed;

    ret = SMBCheck(SMB_COM_DELETE_DIRECTORY,handle);

    return ret;

failed:
	return ret;
}

/**
 * SMB_DeleteFile
 */
s32 SMB_DeleteFile(const char *filename, SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	SMBHANDLE *handle;
	char realfile[512];

	if(filename == NULL)
		return -1;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return -1;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return -1;

	MakeSMBHeader(SMB_COM_DELETE,CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 1);
	pos++;			       /*** Word Count ***/
	setUShort(ptr, pos, SMB_SRCH_HIDDEN);
	pos += 2;			   /*** SearchAttributes ***/
	pos += 2;              /*** Byte Count ***/
	bpos = pos;
	setUChar(ptr, pos, 0x04); /** Bufferformat **/
	pos++;

	realfile[0]='\0';
	if (filename[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,filename);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],realfile,strlen(realfile));
		pos += strlen(realfile)+1;
	}

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret < 0) goto failed;

    ret = SMBCheck(SMB_COM_DELETE,handle);

    return ret;

failed:
	return ret;
}

/**
 * SMB_Rename
 */
s32 SMB_Rename(const char *filename, const char * newfilename, SMBCONN smbhndl)
{
	s32 pos;
	s32 bpos,ret;
	u8 *ptr;
	SMBHANDLE *handle;
	char realfile[512];
	char newrealfile[512];

	if(filename == NULL || newfilename == NULL)
        return -1;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS)
        return -1;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return -1;

	MakeSMBHeader(SMB_COM_RENAME,CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 1);
	pos++;			       /*** Word Count ***/
	setUShort(ptr, pos, SMB_SRCH_HIDDEN);
	pos += 2;			   /*** SearchAttributes ***/
	pos += 2;              /*** Byte Count ***/
	bpos = pos;
	setUChar(ptr, pos, 0x04); /** Bufferformat **/
	pos++;

	realfile[0]='\0';
	if (filename[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,filename);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],realfile,strlen(realfile));
		pos += strlen(realfile)+1;
	}

	pos++;
	setUChar(ptr, pos, 0x04); /** Bufferformat **/
	pos++;

	newrealfile[0]='\0';
	if (newfilename[0] != '\\')
		strcpy(newrealfile,"\\");
	strcat(newrealfile,newfilename);

	if(handle->unicode)
	{
		pos += utf8_to_utf16((char*)&ptr[pos],newrealfile,SMB_MAXPATH-2);
		pos += 2;
	}
	else
	{
		memcpy(&ptr[pos],newrealfile,strlen(newrealfile));
		pos += strlen(newrealfile)+1;
	}

	setUShort(ptr,(bpos-2),(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret < 0) goto failed;

    ret = SMBCheck(SMB_COM_RENAME,handle);

    return ret;

failed:
	return ret;
}

/**
 * SMB_DiskInformation
 */
s32 SMB_DiskInformation(struct statvfs *buf, SMBCONN smbhndl)
{
	s32 pos;
	s32 ret;
	u16 TotalUnits, BlocksPerUnit, BlockSize, FreeUnits;
	u8 *ptr;
	SMBHANDLE *handle;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return -1;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return -1;

	MakeSMBHeader(SMB_COM_QUERY_INFORMATION_DISK,CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 0);
	pos++;			       /*** Word Count ***/
	setUShort(ptr, pos, 0);
	pos += 2;              /*** Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret < 0) goto failed;

	if((ret=SMBCheck(SMB_COM_QUERY_INFORMATION_DISK, handle))==SMB_SUCCESS)
	{
		ptr = handle->message.smb;
		/** Read the received data ***/
		/** WordCount **/
		s32 recv_pos = 1;
		/** TotalUnits **/
		TotalUnits = getUShort(ptr,(SMB_HEADER_SIZE+recv_pos));
		recv_pos += 2;
		/** BlocksPerUnit **/
		BlocksPerUnit = getUShort(ptr,(SMB_HEADER_SIZE+recv_pos));
		recv_pos += 2;
		/** BlockSize **/
		BlockSize = getUShort(ptr,(SMB_HEADER_SIZE+recv_pos));
		recv_pos += 2;
		/** FreeUnits **/
		FreeUnits = getUShort(ptr,(SMB_HEADER_SIZE+recv_pos));
		recv_pos += 2;

        buf->f_bsize = (unsigned long) BlockSize;		    // File system block size.
        buf->f_frsize = (unsigned long) BlockSize;	        // Fundamental file system block size.
        buf->f_blocks = (fsblkcnt_t) (TotalUnits*BlocksPerUnit);   // Total number of blocks on file system in units of f_frsize.
        buf->f_bfree = (fsblkcnt_t) (FreeUnits*BlocksPerUnit);	    // Total number of free blocks.
        buf->f_bavail	= 0;	// Number of free blocks available to non-privileged process.
        buf->f_files = 0;	// Total number of file serial numbers.
        buf->f_ffree = 0;	// Total number of free file serial numbers.
        buf->f_favail = 0;	// Number of file serial numbers available to non-privileged process.
        buf->f_fsid = 0;    // File system ID. 32bit ioType value
        buf->f_flag = 0;    // Bit mask of f_flag values.
        buf->f_namemax = SMB_MAXPATH;   // Maximum filename length.

        return SMB_SUCCESS;
	}

failed:
	handle->conn_valid = false;
	return ret;
}

/**
 * SMB_Read
 */
s32 SMB_ReadFile(char *buffer, size_t size, off_t offset, SMBFILE sfid)
{
	u8 *ptr;
	u32 pos, ret, ofs;
	u16 length = 0;
	SMBHANDLE *handle;
	size_t totalread=0,nextread;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return -1;

	// Check for invalid size
	if(size == 0) return -1;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return -1;

	while(totalread < size)
	{
		if((size-totalread) > SMB_MAX_TRANSMIT_SIZE)
			nextread=SMB_MAX_TRANSMIT_SIZE;
		else
			nextread=size-totalread;

		MakeSMBHeader(SMB_READ_ANDX,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

		pos = SMB_HEADER_SIZE;
		ptr = handle->message.smb;
		setUChar(ptr, pos, 12);
		pos++;				      /*** Word count ***/
		setUChar(ptr, pos, 0xff);
		pos++;
		setUChar(ptr, pos, 0);
		pos++;          /*** Reserved must be 0 ***/
		pos += 2;	    /*** Next AndX Offset ***/
		setUShort(ptr, pos, fid->sfid);
		pos += 2;					    /*** FID ***/
		setUInt(ptr, pos, (offset+totalread) & 0xffffffff);
		pos += 4;						 /*** Offset ***/

		setUShort(ptr, pos, nextread & 0xffff);
		pos += 2;
		setUShort(ptr, pos, nextread & 0xffff);
		pos += 2;
		setUInt(ptr, pos, 0);
		pos += 4;       /*** Reserved must be 0 ***/
		setUShort(ptr, pos, nextread & 0xffff);
		pos += 2;	    /*** Remaining ***/
		setUInt(ptr, pos, (offset+totalread) >> 32);  // offset high
		pos += 4;       /*** OffsetHIGH ***/
		pos += 2;	    /*** Byte count ***/

		handle->message.msg = NBT_SESSISON_MSG;
		handle->message.length = htons(pos);

		pos += 4;

		ret = smb_send(handle->sck_server,(char*)&handle->message, pos);
		if(ret<0) goto failed;

		/*** SMBCheck  ***/
		if(SMBCheck(SMB_READ_ANDX,handle)!=SMB_SUCCESS) goto failed;

		ptr = handle->message.smb;
		// Retrieve data length for this packet
		length = getUShort(ptr,(SMB_HEADER_SIZE+11));

		if(length==0)
			break;

		// Retrieve offset to data
		ofs = getUShort(ptr,(SMB_HEADER_SIZE+13));
		memcpy(&buffer[totalread],&ptr[ofs],length);
		totalread+=length;
	}
	return size;

failed:
	handle->conn_valid = false;
	return SMB_ERROR;
}

/**
 * SMB_Write
 */
s32 SMB_WriteFile(const char *buffer, size_t size, off_t offset, SMBFILE sfid)
{
	u8 *ptr,*src;
	s32 pos, ret;
	s32 blocks64;
	u32 copy_len;
	SMBHANDLE *handle;
	struct _smbfile *fid = (struct _smbfile*)sfid;

	if(!fid) return -1;

	handle = __smb_handle_open(fid->conn);
	if(!handle) return -1;

	MakeSMBHeader(SMB_WRITE_ANDX,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);


	pos = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 14);
	pos++;				  /*** Word Count ***/
	setUChar(ptr, pos, 0xff);
	pos += 2;				   /*** Next AndX ***/
	pos += 2; /*** Next AndX Offset ***/

	setUShort(ptr, pos, fid->sfid);
	pos += 2;
	setUInt(ptr, pos, offset & 0xffffffff);
	pos += 4;
	setUInt(ptr, pos, 0);  /*** Reserved, must be 0 ***/
	pos += 4;
	setUShort(ptr, pos, 0);  /*** Write Mode ***/
	pos += 2;
	pos += 2; /*** Remaining ***/

	blocks64 = size >> 16;

	setUShort(ptr, pos, blocks64);
	pos += 2;				       /*** Length High ***/
	setUShort(ptr, pos, size & 0xffff);
	pos += 2;					    /*** Length Low ***/
	setUShort(ptr, pos, 63);
	pos += 2;				 /*** Data Offset ***/
	setUInt(ptr, pos, offset >> 32);  /*** OffsetHigh ***/
	pos += 4;
	setUShort(ptr, pos, size & 0xffff);
	pos += 2;					    /*** Data Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos+size);

	src = (u8*)buffer;
	copy_len = size;

	if((copy_len+pos)>SMB_MAX_TRANSMIT_SIZE)
		copy_len = (SMB_MAX_TRANSMIT_SIZE-pos);

	memcpy(&ptr[pos],src,copy_len);
	size -= copy_len;
	src += copy_len;
	pos += copy_len;

	pos += 4;

	/*** Send Header Information ***/
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) goto failed;

	if(size>0) {
		/*** Send the data ***/
		ret = smb_send(handle->sck_server,src,size);
		if(ret<0) goto failed;
	}

	ret = 0;
	if(SMBCheck(SMB_WRITE_ANDX,handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		ret = getUShort(ptr,(SMB_HEADER_SIZE+5));
	}

	return ret;

failed:
	handle->conn_valid = false;
	return ret;
}

/**
* SMB_PathInfo
*/
s32 SMB_PathInfo(const char *filename, SMBDIRENTRY *sdir, SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	int len;
	SMBHANDLE *handle;
	char realfile[512];

	if(filename == NULL)
		return SMB_ERROR;

	handle = __smb_handle_open(smbhndl);
	if (!handle) return SMB_ERROR;

	MakeSMBHeader(SMB_TRANS2, CIFS_FLAGS1, handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2, handle);
	MakeTRANS2Header(SMB_QUERY_PATH_INFO, handle);

	bpos = pos = (T2_BYTE_CNT + 2);
	pos += 3; /*** Padding ***/
	ptr = handle->message.smb;
	setUShort(ptr, pos, SMB_QUERY_FILE_ALL_INFO);
	pos += 2; /*** Level of information requested ***/
	setUInt(ptr, pos, 0);
	pos += 4; /*** reserved ***/

    realfile[0] = '\0';
	if (filename[0] != '\\')
		strcpy(realfile,"\\");
	strcat(realfile,filename);

	if(handle->unicode)
	{
		len = utf8_to_utf16((char*)&ptr[pos],realfile,SMB_MAXPATH-2);
		pos += len+2;
		len+=1;
	}
	else
	{
		len = strlen(realfile);
		memcpy(&ptr[pos],realfile,len);
		pos += len+1;
	}

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, (7 + len));
	setUShort(ptr, T2_SPRM_CNT, (7 + len));
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_BYTE_CNT, (pos - bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server, (char*) &handle->message, pos);
	if(ret<0) goto failed;

	ret = SMB_ERROR;
	if (SMBCheck(SMB_TRANS2, handle) == SMB_SUCCESS) {

		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr, (SMB_HEADER_SIZE + 9));
		pos += 4; // padding
		sdir->ctime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - creation time
		sdir->atime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - access time
		sdir->mtime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - write time
		pos += 8; // ULONGLONG - change time
		sdir->attributes = getUInt(ptr,pos); pos += 4; // ULONG - file attributes
		pos += 4; // padding
		pos += 8; // ULONGLONG - allocated file size
		sdir->size = getULongLong(ptr, pos); pos += 8; // ULONGLONG - file size
		pos += 4; // ULONG   NumberOfLinks;
		pos += 2; // UCHAR   DeletePending;
		pos += 2; // UCHAR   Directory;
		pos += 2; // USHORT  Pad2; // for alignment only
		pos += 4; // ULONG   EaSize;
		pos += 4; // ULONG   FileNameLength;

		strcpy(sdir->name,realfile);

		ret = SMB_SUCCESS;
	}
	return ret;

failed:
	handle->conn_valid = false;
	return ret;
}

/**
 * SMB_FindFirst
 *
 * Uses TRANS2 to support long filenames
 */
s32 SMB_FindFirst(const char *filename, unsigned short flags, SMBDIRENTRY *sdir, SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	unsigned int len;
	SMBHANDLE *handle;
	SMBSESSION *sess;

	if(filename == NULL)
		return SMB_ERROR;

	if(SMB_Reconnect(&smbhndl,true)!=SMB_SUCCESS) return SMB_ERROR;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;

	sess = &handle->session;
	MakeSMBHeader(SMB_TRANS2,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);
	MakeTRANS2Header(SMB_FIND_FIRST2,handle);

	ptr = handle->message.smb;

	bpos = pos = (T2_BYTE_CNT+2);
	pos += 3;	     /*** Padding ***/
	setUShort(ptr, pos, flags);
	pos += 2;					  /*** Flags ***/
	setUShort(ptr, pos, 1);
	pos += 2;					  /*** Count ***/
	setUShort(ptr, pos, 6);
	pos += 2;					  /*** Internal Flags ***/
	setUShort(ptr, pos, 260); // SMB_FIND_FILE_BOTH_DIRECTORY_INFO
	pos += 2;					  /*** Level of Interest ***/
	pos += 4;    /*** Storage Type == 0 ***/

	if(handle->unicode)
	{
		len = utf8_to_utf16((char*)&ptr[pos], (char*)filename,SMB_MAXPATH-2);
		pos += len+2;
		len++;
	}
	else
	{
		len = strlen(filename);
		memcpy(&ptr[pos],filename,len);
		pos += len+1;
	}

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, (13+len));
	setUShort(ptr, T2_SPRM_CNT, (13+len));
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_BYTE_CNT,(pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) goto failed;

	sess->eos = 1;
	sess->count = 0;
	ret = SMB_ERROR;
	if(SMBCheck(SMB_TRANS2,handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr,(SMB_HEADER_SIZE+9));
		sdir->sid = getUShort(ptr, pos); pos += 2;
		sess->count = getUShort(ptr, pos); pos += 2;
		sess->eos = getUShort(ptr, pos); pos += 2;
		pos += 2; // USHORT EaErrorOffset;
		pos += 2; // USHORT LastNameOffset;
		pos += 2; // padding?

		if (sess->count)
		{
			pos += 4; // ULONG NextEntryOffset;
			pos += 4; // ULONG FileIndex;
			sdir->ctime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - creation time
			sdir->atime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - access time
			sdir->mtime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - write time
			pos += 8; // ULONGLONG - change time low
			sdir->size = getULongLong(ptr, pos); pos += 8;
			pos += 8;
			sdir->attributes = getUInt(ptr, pos); pos += 4;
			len=getUInt(ptr, pos);
			pos += 34;
			if(handle->unicode)
                utf16_to_utf8(sdir->name,(char*)&ptr[pos],len);
			else
                strcpy(sdir->name,(const char*)&ptr[pos]);

			ret = SMB_SUCCESS;
		}
	}
	return ret;

failed:
	handle->conn_valid = false;
	return ret;
}

/**
 * SMB_FindNext
 */
s32 SMB_FindNext(SMBDIRENTRY *sdir,SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	s32 bpos;
	SMBHANDLE *handle;
	SMBSESSION *sess;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;

	sess = &handle->session;
	if(sess->eos || sdir->sid==0) return SMB_ERROR;

	MakeSMBHeader(SMB_TRANS2,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);
	MakeTRANS2Header(SMB_FIND_NEXT2,handle);

	bpos = pos = (T2_BYTE_CNT+2);
	pos += 3;	     /*** Padding ***/
	ptr = handle->message.smb;
	setUShort(ptr, pos, sdir->sid);
	pos += 2;						    /*** Search ID ***/
	setUShort(ptr, pos, 1);
	pos += 2;					  /*** Count ***/
	setUShort(ptr, pos, 260); // SMB_FIND_FILE_BOTH_DIRECTORY_INFO
	pos += 2;					  /*** Level of Interest ***/
	pos += 4;    /*** Storage Type == 0 ***/
	setUShort(ptr, pos, 12);
	pos+=2; /*** Search flags ***/
	pos++;

	int pad=0;
	if(handle->unicode)pad=1;
	pos+=pad;

	/*** Update counts ***/
	setUShort(ptr, T2_PRM_CNT, 13+pad);
	setUShort(ptr, T2_SPRM_CNT, 13+pad);
	setUShort(ptr, T2_SPRM_OFS, 68);
	setUShort(ptr, T2_BYTE_CNT, (pos-bpos));

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) goto failed;

	ret = SMB_ERROR;
	if (SMBCheck(SMB_TRANS2,handle)==SMB_SUCCESS) {
		ptr = handle->message.smb;
		/*** Get parameter offset ***/
		pos = getUShort(ptr,(SMB_HEADER_SIZE+9));
		sess->count = getUShort(ptr, pos); pos += 2;
		sess->eos = getUShort(ptr, pos); pos += 2;
		pos += 2; // USHORT EaErrorOffset;
		pos += 2; // USHORT LastNameOffset;

		if (sess->count)
		{
			int len;
			pos += 4; // ULONG NextEntryOffset;
			pos += 4; // ULONG FileIndex;
			sdir->ctime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - creation time
			sdir->atime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - access time
			sdir->mtime = getULongLong(ptr, pos) - handle->session.timeOffset; pos += 8; // ULONGLONG - write time
			pos += 8; // ULONGLONG - change time low
			sdir->size = getULongLong(ptr, pos); pos += 8;
			pos += 8;
			sdir->attributes = getUInt(ptr, pos); pos += 4;
			len=getUInt(ptr, pos);
			pos += 34;
			if(handle->unicode)
				utf16_to_utf8(sdir->name, (char*)&ptr[pos],len);
			else
				strcpy (sdir->name, (const char*)&ptr[pos]);

			ret = SMB_SUCCESS;
		}
	}
	return ret;

failed:
	handle->conn_valid = false;
	return ret;
}

/**
 * SMB_FindClose
 */
s32 SMB_FindClose(SMBDIRENTRY *sdir,SMBCONN smbhndl)
{
	u8 *ptr;
	s32 pos;
	s32 ret;
	SMBHANDLE *handle;

	handle = __smb_handle_open(smbhndl);
	if(!handle) return SMB_ERROR;

	if(sdir->sid==0) return SMB_ERROR;

	MakeSMBHeader(SMB_FIND_CLOSE2,CIFS_FLAGS1,handle->unicode?CIFS_FLAGS2_UNICODE:CIFS_FLAGS2,handle);

	pos  = SMB_HEADER_SIZE;
	ptr = handle->message.smb;
	setUChar(ptr, pos, 1);
	pos++;					   /*** Word Count ***/
	setUShort(ptr, pos, sdir->sid);
	pos += 2;
	pos += 2;  /*** Byte Count ***/

	handle->message.msg = NBT_SESSISON_MSG;
	handle->message.length = htons(pos);

	pos += 4;
	ret = smb_send(handle->sck_server,(char*)&handle->message,pos);
	if(ret<0) goto failed;

	ret = SMBCheck(SMB_FIND_CLOSE2,handle);
	return ret;

failed:
	handle->conn_valid = false;
	return ret;
}
