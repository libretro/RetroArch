/*-------------------------------------------------------------

di.c -- Drive Interface library

Team Twiizers
Copyright (C) 2008

Erant
marcan

rodries
emukidid

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

#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <di/di.h>
#include <ogc/cache.h>
#include <ogc/es.h>
#include <ogc/ipc.h>
#include <ogc/ios.h>
#include <ogc/mutex.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#define MOUNT_TIMEOUT		15000 // 15 seconds

int di_fd = -1;
static bool load_dvdx = false;
static bool use_dvd_cache = true;
static int have_ahbprot = 0;
static int state = DVD_INIT | DVD_NO_DISC;

static int _cover_callback(int ret, void* usrdata);

static unsigned int bufferMutex = 0;
static uint32_t outbuf[8] __attribute__((aligned(32)));
static uint32_t dic[8] __attribute__((aligned(32)));
static const char di_path[] ATTRIBUTE_ALIGN(32) = "/dev/di";

static read_func DI_ReadDVDptr = NULL;
static read_func_async DI_ReadDVDAsyncptr = NULL;
static di_callback di_cb = NULL;

static vu32* const _dvdReg = (u32*)0xCD806000;

static int _DI_ReadDVD_A8_Async(void* buf, uint32_t len, uint32_t lba, ipccallback ipc_cb){
	int ret;
	
	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F) // This only works with 32 byte aligned addresses!
		return -EFAULT;
	
	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = len << 11; // 1 LB is 2048 bytes
	dic[2] = lba << 9; // Nintendo's read function uses byteOffset >> 2, so we only shift 9 left, not 11.

	ret = IOS_IoctlAsync(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, len << 11,ipc_cb, buf);

	if(ret == 2)
		ret = EIO;

	return (ret == 1)? 0 : -ret;
}

static int _DI_ReadDVD_D0_Async(void* buf, uint32_t len, uint32_t lba, ipccallback ipc_cb){
	int ret;

	if(!buf)
		return -EINVAL;
	
	if((uint32_t)buf & 0x1F)
		return -EFAULT;

	dic[0] = DVD_READ << 24;
	dic[1] = 0;		// Unknown what this does as of now. (Sets some value to 0x10 in the drive if set).
	dic[2] = 0;		// USE_DEFAULT_CONFIG flag. Drive will use default config if this bit is set.
	dic[3] = len;
	dic[4] = lba;
	
	ret = IOS_IoctlAsync(di_fd, DVD_READ, dic, 0x20, buf, len << 11,ipc_cb, buf);

	if(ret == 2)
		ret = EIO;

	return (ret == 1)? 0 : -ret;
}

static int _DI_ReadDVD(void* buf, uint32_t len, uint32_t lba, uint32_t read_cmd){
	if ((((int) buf) & 0xC0000000) == 0x80000000) // cached?
		_dvdReg[0] = 0x2E;
	_dvdReg[1] = 0;
	_dvdReg[2] = read_cmd;
	_dvdReg[3] = read_cmd == 0xD0000000 ? lba : lba << 9;
	_dvdReg[4] = read_cmd == 0xD0000000 ? len : len << 11;
	_dvdReg[5] = (unsigned long) buf;
	_dvdReg[6] = len << 11;
	_dvdReg[7] = 3; // enable reading!
	DCInvalidateRange(buf, len << 11);
	while (_dvdReg[7] & 1);

	if (_dvdReg[0] & 0x4)
		return 1;
	return 0;
}

static int _DI_ReadDVD_A8(void* buf, uint32_t len, uint32_t lba){
	int ret, retry_count = LIBDI_MAX_RETRIES;
	
	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F) // This only works with 32 byte aligned addresses!
		return -EFAULT;
	
	if(have_ahbprot)
		return _DI_ReadDVD(buf, len, lba, 0xA8000000);
	
	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = len << 11; // 1 LB is 2048 bytes
	dic[2] = lba << 9; // Nintendo's read function uses byteOffset >> 2, so we only shift 9 left, not 11.

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, len << 11);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2)
		ret = EIO;

	return (ret == 1)? 0 : -ret;
}

static int _DI_ReadDVD_D0(void* buf, uint32_t len, uint32_t lba){
	int ret, retry_count = LIBDI_MAX_RETRIES;
	
	if(!buf)
		return -EINVAL;
	
	if((uint32_t)buf & 0x1F)
		return -EFAULT;
	
	if(have_ahbprot)
		return _DI_ReadDVD(buf, len, lba, 0xD0000000);

	dic[0] = DVD_READ << 24;
	dic[1] = 0; // Unknown what this does as of now. (Sets some value to 0x10 in the drive if set).
	dic[2] = 0; // USE_DEFAULT_CONFIG flag. Drive will use default config if this bit is set.
	dic[3] = len;
	dic[4] = lba;

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ, dic, 0x20, buf, len << 11);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2)
		ret = EIO;

	return (ret == 1)? 0 : -ret;
}

///// Cache
#define CACHE_FREE 0xFFFFFFFF
#define BLOCK_SIZE 0x800
#define CACHEBLOCKS 26
typedef struct
{
	uint32_t block;
	void *ptr;
} cache_page;
static cache_page *cache_read = NULL;

static void CreateDVDCache()
{
	if (cache_read != NULL)
		return;
	cache_read = (cache_page *) malloc(sizeof(cache_page));
	if (cache_read == NULL)
		return;

	cache_read->block = CACHE_FREE;
	cache_read->ptr = memalign(32, BLOCK_SIZE * CACHEBLOCKS);
	if (cache_read->ptr == NULL)
	{
		free(cache_read);
		cache_read = NULL;
		return;
	}
	memset(cache_read->ptr, 0, BLOCK_SIZE);
}

static int ReadBlockFromCache(void *buf, uint32_t len, uint32_t block)
{
	int retval;

	if (cache_read == NULL)
		return DI_ReadDVDptr(buf, len, block);

	if ((block >= cache_read->block) && (block + len < (cache_read->block + CACHEBLOCKS)))
	{
		memcpy(buf, cache_read->ptr	+ ((block - cache_read->block) * BLOCK_SIZE), BLOCK_SIZE * len);
		return 0;
	}

	if (len > CACHEBLOCKS)
		return DI_ReadDVDptr(buf, len, block);

	retval = DI_ReadDVDptr(cache_read->ptr, CACHEBLOCKS, block);
	if (retval)
	{
		cache_read->block = CACHE_FREE;
		return retval;
	}

	cache_read->block = block;
	memcpy(buf, cache_read->ptr, len * BLOCK_SIZE);

	return 0;
}

/*
Initialize the DI interface, should always be called first!
*/

u32 __di_check_ahbprot(void) {
	return ((*(vu32*)0xcd800064 == 0xFFFFFFFF) ? 1 : 0);
}

int DI_Init() {
	if(di_fd >= 0)
		return 1;

	state = DVD_INIT | DVD_NO_DISC;
	have_ahbprot = __di_check_ahbprot();

	if(have_ahbprot == 0)
		return 0;

	if (di_fd < 0)
		di_fd = IOS_Open(di_path, 2);

	if (di_fd < 0)
		return di_fd;

	if (!bufferMutex)
		LWP_MutexInit(&bufferMutex, false);

	if(use_dvd_cache)
		CreateDVDCache();

	return 0;
}

void DI_LoadDVDX(bool load) {
	load_dvdx = load;
}

void DI_UseCache(bool use) {
	use_dvd_cache = use;
}

void DI_Mount() {
	if(di_fd < 0)
		return;

	uint32_t status;

	if (DI_GetCoverRegister(&status) != 0) {
		state = DVD_NO_DISC;
		return;
	}

	if ((status & DVD_COVER_DISC_INSERTED) == 0) {
		state = DVD_NO_DISC;
		return;
	}

	state = DVD_INIT | DVD_NO_DISC;
	_cover_callback(1, NULL);	// Initialize the callback chain.

	if (cache_read != NULL)
		cache_read->block = CACHE_FREE; // reset cache
}

void DI_Close(){
	if(di_fd < 0)
		return;

	if (di_fd > 0)
		IOS_Close(di_fd);

	di_fd = -1;

	DI_ReadDVDptr = NULL;
	DI_ReadDVDAsyncptr = NULL;
	state = DVD_INIT | DVD_NO_DISC;

	if (bufferMutex) {
		LWP_MutexDestroy(bufferMutex);
		bufferMutex = 0;
	}
}

static int _DI_ReadDVD_Check(void* buf, uint32_t len, uint32_t lba)
{
	int ret;

	ret = _DI_ReadDVD_D0(buf, len, lba);
	if (ret == 0)
	{
		state = state | DVD_D0;
		DI_ReadDVDptr = _DI_ReadDVD_D0;
		DI_ReadDVDAsyncptr = _DI_ReadDVD_D0_Async;
		return ret;
	}
	ret = _DI_ReadDVD_A8(buf, len, lba);
	if (ret == 0)
	{
		state = state | DVD_A8;
		DI_ReadDVDptr = _DI_ReadDVD_A8;
		DI_ReadDVDAsyncptr = _DI_ReadDVD_A8_Async;
		return ret;
	}
	return ret;
}

static int _DI_ReadDVD_Check_Async(void* buf, uint32_t len, uint32_t lba,	ipccallback ipc_cb)
{
	int ret;

	ret = _DI_ReadDVD_D0_Async(buf, len, lba, ipc_cb);
	if (ret == 0)
	{
		state = state | DVD_D0;
		DI_ReadDVDptr = _DI_ReadDVD_D0;
		DI_ReadDVDAsyncptr = _DI_ReadDVD_D0_Async;
		return ret;
	}
	ret = _DI_ReadDVD_A8_Async(buf, len, lba, ipc_cb);
	if (ret == 0)
	{
		state = state | DVD_A8;
		DI_ReadDVDptr = _DI_ReadDVD_A8;
		DI_ReadDVDAsyncptr = _DI_ReadDVD_A8_Async;
		return ret;
	}
	return ret;
}
	
static void _DI_SetCallback(int ioctl_nr, ipccallback ipc_cb){
	if ((di_fd < 0) || !ipc_cb)
		return;

	LWP_MutexLock(bufferMutex);

	memset(dic, 0x00, sizeof(dic));

	dic[0] = ioctl_nr << 24;
	dic[1] = (ioctl_nr == DVD_RESET)? 1 : 0;	// For reset callback. Dirty, I know...

	IOS_IoctlAsync(di_fd,ioctl_nr, dic, 0x20, outbuf, 0x20, ipc_cb, outbuf);
	LWP_MutexUnlock(bufferMutex);
}

#define COVER_CLOSED (*((uint32_t*)usrdata) & DVD_COVER_DISC_INSERTED)

static int _cover_callback(int ret, void* usrdata){
	static int cur_state = 0;
	static int retry_count = LIBDI_MAX_RETRIES;
	const int callback_table[] = {
		DVD_GETCOVER,
		DVD_WAITFORCOVERCLOSE,
		DVD_RESET,
		DVD_IDENTIFY,		// This one will complete when the drive is ready.
		DVD_READ_DISCID,
		0};
	const int return_table[] = {1,1,4,1,1};

	if(cur_state > 1)
		state &= ~DVD_NO_DISC;

	if(callback_table[cur_state]){
		if(ret == return_table[cur_state]){
			if(cur_state == 1 && COVER_CLOSED)	// Disc inside, skipping wait for cover.
				cur_state += 2;	
			else
				cur_state++; // If the previous callback succeeded, moving on to the next

			retry_count = LIBDI_MAX_RETRIES;
		}
		else
		{
			retry_count--;
			if(retry_count < 0){		// Drive init failed for unknown reasons.
				retry_count = LIBDI_MAX_RETRIES;
				cur_state = 0;
				state = DVD_UNKNOWN;
				return 0;
			}
		}
		_DI_SetCallback(callback_table[cur_state - 1], _cover_callback);

	}
	else		// Callback chain has completed OK. The drive is ready.
	{
		state = DVD_READY;
		DI_ReadDVDptr = _DI_ReadDVD_Check;
		DI_ReadDVDAsyncptr = _DI_ReadDVD_Check_Async;


		if(di_cb)
			di_cb(state,0);

		retry_count = LIBDI_MAX_RETRIES;
		cur_state = 0;
	}
	return 0;
}

/* Get current status, will return the API status */
int DI_GetStatus(){
	return state;
}

void DI_SetInitCallback(di_callback cb){
	di_cb = cb;
}

/*
Request an identification from the drive, returned in a DI_DriveID struct
*/
int DI_Identify(DI_DriveID* id){
	if(di_fd < 0)
		return -ENXIO;

	if(!id)
		return -EINVAL;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_IDENTIFY << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_IDENTIFY, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	memcpy(id,outbuf,sizeof(DI_DriveID));

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_CheckDVDSupport() {
	DI_DriveID id;

	if(DI_Identify(&id) == 0 && id.rel_date <= 0x20080714)
		return 1;

	return 0;
}

/*
Returns the current error code on the drive.
yagcd has a pretty comprehensive list of possible error codes
*/
int DI_GetError(uint32_t* error){
	if(di_fd < 0)
		return -ENXIO;

	if(!error)
		return -EINVAL;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_GET_ERROR << 24;
	
	int ret = IOS_Ioctl(di_fd, DVD_GET_ERROR, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	*error = outbuf[0];		// Error code is returned as an int in the first four bytes of outbuf.

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;		
}

/*
Reset the drive.
*/
int DI_Reset(){
	if(di_fd < 0)
		return -ENXIO;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_RESET << 24;
	dic[1] = 1;
	
	int ret = IOS_Ioctl(di_fd, DVD_RESET, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Main read function, basically just a wrapper to the function pointer.
Nicer then just exposing the pointer itself
*/
int DI_ReadDVD(void* buf, uint32_t len, uint32_t lba){
	if(di_fd < 0)
		return -ENXIO;

	int ret;
	if(DI_ReadDVDptr){
		LWP_MutexLock(bufferMutex);
		ret = ReadBlockFromCache(buf,len,lba);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

int DI_ReadDVDAsync(void* buf, uint32_t len, uint32_t lba,ipccallback ipc_cb){
	if(di_fd < 0)
		return -ENXIO;

	int ret;
	if(DI_ReadDVDAsyncptr){
		LWP_MutexLock(bufferMutex);
		ret = DI_ReadDVDAsyncptr(buf,len,lba,ipc_cb);
		LWP_MutexUnlock(bufferMutex);
		return ret;
	}
	return -1;
}

/*
Unknown what this does as of now...
*/
int DI_ReadDVDConfig(uint32_t* val, uint32_t flag){
	if(di_fd < 0)
		return -ENXIO;

	if(!val)
		return -EINVAL;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_READ_CONFIG << 24;
	dic[1] = flag & 0x1;		// Update flag, val will be written if this is 1, val won't be written if it's 0.
	dic[2] = 0;					// Command will fail driveside if this is not zero.
	dic[3] = *val;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_CONFIG, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	*val = outbuf[0];
	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Read the copyright information on a DVDVideo
*/
int DI_ReadDVDCopyright(uint32_t* copyright){
	if(di_fd < 0)
		return -ENXIO;

	if(!copyright)
		return -EINVAL;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_READ_COPYRIGHT << 24;
	dic[1] = 0;
	
	int ret = IOS_Ioctl(di_fd, DVD_READ_COPYRIGHT, dic, 0x20, outbuf, 0x20);
	*copyright = *((uint32_t*)outbuf);		// Copyright information is returned as an int in the first four bytes of outbuf.

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_Read_BCA(void *outbuf)
{
	if(di_fd < 0)
		return -ENXIO;

	if(!outbuf)
		return -EINVAL;

	memset(dic, 0, sizeof(dic));
	dic[0] = DVD_READ_BCA << 24;

	int ret = IOS_Ioctl(di_fd, DVD_READ_BCA, dic, 0x20, outbuf, 64);
	
	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
Returns 0x800 bytes worth of Disc key
*/
int DI_ReadDVDDiscKey(void* buf){
	int ret;
	int retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0)
		return -ENXIO;

	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F)
		return -EFAULT;
	
	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_READ_DISCKEY << 24;
	dic[1] = 0;		// Unknown what this flag does.
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_DISCKEY, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/*
This function will read the initial sector on the DVD, which contains stuff like the booktype
*/
int DI_ReadDVDPhysical(void* buf){
	int ret;
	int retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0)
		return -ENXIO;

	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F)
		return -EFAULT;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_READ_PHYSICAL << 24;
	dic[1] = 0;		// Unknown what this flag does.
	
	do{
		ret = IOS_Ioctl(di_fd, DVD_READ_PHYSICAL, dic, 0x20, buf, 0x800);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_ReportKey(int keytype, uint32_t lba, void* buf){
	if(di_fd < 0)
		return -ENXIO;

	if(!buf)
		return -EINVAL;
	
	if((uint32_t)buf & 0x1F)
		return -EFAULT;

	LWP_MutexLock(bufferMutex);
	
	dic[0] = DVD_REPORTKEY << 24;
	dic[1] = keytype & 0xFF;
	dic[2] = lba;
	
	int ret = IOS_Ioctl(di_fd, DVD_REPORTKEY, dic, 0x20, buf, 0x20);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

int DI_GetCoverRegister(uint32_t* status){
	if(di_fd < 0)
		return -ENXIO;

	LWP_MutexLock(bufferMutex);
	memset(dic, 0x00, 0x20);

	int ret = IOS_Ioctl(di_fd, DVD_GETCOVER, dic, 0x20, outbuf, 0x20);
	if(ret == 2)
		ret = EIO;

	*status = outbuf[0];

	LWP_MutexUnlock(bufferMutex);
	return (ret == 1)? 0 : -ret;
}

/* Internal function for controlling motor operations */
static int _DI_SetMotor(int flag){
	if(di_fd < 0)
		return -ENXIO;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_SET_MOTOR << 24;
	dic[1] = flag & 0x1;			// Eject flag.
	dic[2] = (flag >> 1) & 0x1;		// Don't use this flag, it kills the drive until next reset.

	int ret = IOS_Ioctl(di_fd, DVD_SET_MOTOR, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

/* Stop the drives motor */
int DI_StopMotor(){
	return _DI_SetMotor(0);
}

/* Stop the motor, and eject the disc. Also needs a reset afterwards for normal operation */
int DI_Eject(){
	return _DI_SetMotor(1);
}

/* Warning, this will kill your drive untill the next reset. Will not respond to DI commands,
will not take in or eject the disc. Your drive will be d - e - d, dead.

I deem this function to be harmless, as normal operation will resume after a reset.
However, I am not liable for anyones drive exploding as a result from using this function.
*/
int DI_KillDrive(){
	return _DI_SetMotor(2);
}

int DI_ClosePartition() {
	if(di_fd < 0)
		return -ENXIO;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_CLOSE_PARTITION << 24;

	int ret = IOS_Ioctl(di_fd, DVD_CLOSE_PARTITION, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_OpenPartition(uint32_t offset)
{
	if(di_fd < 0)
		return -ENXIO;

	static ioctlv vectors[5] __attribute__((aligned(32)));
	static char certs[0x49e4] __attribute__((aligned(32)));
	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_OPEN_PARTITION << 24;
	dic[1] = offset;

	vectors[0].data = dic;
	vectors[0].len = 0x20;
	vectors[1].data = NULL;
	vectors[1].len = 0x2a4;
	vectors[2].data = NULL;
	vectors[2].len = 0;

	vectors[3].data = certs;
	vectors[3].len = 0x49e4;
	vectors[4].data = outbuf;
	vectors[4].len = 0x20;

	int ret = IOS_Ioctlv(di_fd, DVD_OPEN_PARTITION, 3, 2, vectors);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;

}

int DI_Read(void *buf, uint32_t size, uint32_t offset)
{
	if(di_fd < 0)
		return -ENXIO;

	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F)
		return -EFAULT;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_LOW_READ << 24;
	dic[1] = size;
	dic[2] = offset;

	int ret = IOS_Ioctl(di_fd, DVD_LOW_READ, dic, 0x20, buf, size);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

int DI_UnencryptedRead(void *buf, uint32_t size, uint32_t offset)
{
	int ret, retry_count = LIBDI_MAX_RETRIES;

	if(di_fd < 0)
		return -ENXIO;

	if(!buf)
		return -EINVAL;

	if((uint32_t)buf & 0x1F) // This only works with 32 byte aligned addresses!
		return -EFAULT;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_READ_UNENCRYPTED << 24;
	dic[1] = size;
	dic[2] = offset;

	do{	
		ret = IOS_Ioctl(di_fd, DVD_READ_UNENCRYPTED, dic, 0x20, buf, size);
		retry_count--;
	}while(ret != 1 && retry_count > 0);

	if(ret == 2)
		ret = EIO;

	LWP_MutexUnlock(bufferMutex);

	return (ret == 1)? 0 : -ret;
}

int DI_ReadDiscID(uint64_t *id)
{
	if(di_fd < 0)
		return -ENXIO;

	LWP_MutexLock(bufferMutex);

	dic[0] = DVD_READ_DISCID << 24;

	int ret = IOS_Ioctl(di_fd, DVD_READ_DISCID, dic, 0x20, outbuf, 0x20);

	if(ret == 2)
		ret = EIO;

	memcpy(id, outbuf, sizeof(*id));

	LWP_MutexUnlock(bufferMutex);
	return(ret == 1)? 0 : -ret;
}

static bool diio_Startup()
{
	u64 t1,t2;

	if(di_fd < 0)
		return false;

	DI_Mount();

	t1=ticks_to_millisecs(gettime());

	while(state & DVD_INIT)
	{
		usleep(500);
		t2=ticks_to_millisecs(gettime());
		if( (t2 - t1) > MOUNT_TIMEOUT)
			return false; // timeout
	}

	if(state & DVD_READY)
		return true;
	return false;
}

static bool diio_IsInserted()
{
	u32 val;

	if(di_fd < 0)
		return false;

	DI_GetCoverRegister(&val);
	if(val & 0x2)
		return true;

	return false;
}

static bool diio_ReadSectors(sec_t sector,sec_t numSectors,void *buffer)
{
	if(DI_ReadDVD(buffer, numSectors, sector) == 0)
		return true;
	return false;
}

static bool diio_WriteSectors(sec_t sector,sec_t numSectors,const void *buffer)
{
	return true;
}

static bool diio_ClearStatus()
{
	return true;
}

static bool diio_Shutdown()
{
	DI_StopMotor();
	return true;
}

const DISC_INTERFACE __io_wiidvd = {
	DEVICE_TYPE_WII_DVD,
	FEATURE_MEDIUM_CANREAD | FEATURE_WII_DVD,
	(FN_MEDIUM_STARTUP)&diio_Startup,
	(FN_MEDIUM_ISINSERTED)&diio_IsInserted,
	(FN_MEDIUM_READSECTORS)&diio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&diio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&diio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&diio_Shutdown
};
