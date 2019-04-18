/*-------------------------------------------------------------

network_wii.c -- Wii network subsystem

Copyright (C) 2008 bushing

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

#if defined(HW_RVL)

#define MAX_IP_RETRIES		100
#define MAX_INIT_RETRIES	32

//#define DEBUG_NET

#ifdef DEBUG_NET
#define debug_printf(fmt, args...) \
	do { \
		fprintf(stderr, "%s:%d:" fmt, __FUNCTION__, __LINE__, ##args); \
	} while (0)
#else
#define debug_printf(fmt, args...) do { } while (0)
#endif // DEBUG_NET

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define __LINUX_ERRNO_EXTENSIONS__
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>

#include "ipc.h"
#include "processor.h"
#include "network.h"
#include "ogcsys.h"
#include "lwp_heap.h"

#define NET_HEAP_SIZE				64*1024

#define IOS_O_NONBLOCK				0x04			//(O_NONBLOCK >> 16) - it's in octal representation, so this shift leads to 0 and hence nonblocking sockets didn't work. changed it to the right value.

#define IOCTL_NWC24_STARTUP			0x06

#define IOCTL_NCD_SETIFCONFIG3		0x03
#define IOCTL_NCD_SETIFCONFIG4		0x04
#define IOCTL_NCD_GETLINKSTATUS		0x07
#define IOCTLV_NCD_GETMACADDRESS    0x08

#define NET_UNKNOWN_ERROR_OFFSET	-10000

enum {
	IOCTL_SO_ACCEPT	= 1,
	IOCTL_SO_BIND,
	IOCTL_SO_CLOSE,
	IOCTL_SO_CONNECT,
	IOCTL_SO_FCNTL,
	IOCTL_SO_GETPEERNAME, // todo
	IOCTL_SO_GETSOCKNAME, // todo
	IOCTL_SO_GETSOCKOPT,  // todo    8
	IOCTL_SO_SETSOCKOPT,
	IOCTL_SO_LISTEN,
	IOCTL_SO_POLL,        // todo    b
	IOCTLV_SO_RECVFROM,
	IOCTLV_SO_SENDTO,
	IOCTL_SO_SHUTDOWN,    // todo    e
	IOCTL_SO_SOCKET,
	IOCTL_SO_GETHOSTID,
	IOCTL_SO_GETHOSTBYNAME,
	IOCTL_SO_GETHOSTBYADDR,// todo
	IOCTLV_SO_GETNAMEINFO, // todo   13
	IOCTL_SO_UNK14,        // todo
	IOCTL_SO_INETATON,     // todo
	IOCTL_SO_INETPTON,     // todo
	IOCTL_SO_INETNTOP,     // todo
	IOCTLV_SO_GETADDRINFO, // todo
	IOCTL_SO_SOCKATMARK,   // todo
	IOCTLV_SO_UNK1A,       // todo
	IOCTLV_SO_UNK1B,       // todo
	IOCTLV_SO_GETINTERFACEOPT, // todo
	IOCTLV_SO_SETINTERFACEOPT, // todo
	IOCTL_SO_SETINTERFACE,     // todo
	IOCTL_SO_STARTUP,           // 0x1f
	IOCTL_SO_ICMPSOCKET =	0x30, // todo
	IOCTLV_SO_ICMPPING,         // todo
	IOCTL_SO_ICMPCANCEL,        // todo
	IOCTL_SO_ICMPCLOSE          // todo
};

struct init_data {
	u32 state;
	s32 fd;
	s32 prevres;
	s32 result;
	syswd_t alarm;
	u32 retries;
	netcallback cb;
	void *usrdata;
	u8 *buf;
};

struct init_default_cb {
	lwpq_t queue;
	s32 result;
};

struct bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
};

struct connect_params {
	u32 socket;
	u32 has_addr;
	u8 addr[28];
};

struct sendto_params {
	u32 socket;
	u32 flags;
	u32 has_destaddr;
	u8 destaddr[28];
};

struct setsockopt_params {
	u32 socket;
	u32 level;
	u32 optname;
	u32 optlen;
	u8 optval[20];
};

// 0 means we don't know what this error code means
// I sense a pattern here...
static u8 _net_error_code_map[] = {
	0, // 0
 	E2BIG,
 	EACCES,
 	EADDRINUSE,
 	EADDRNOTAVAIL,
 	EAFNOSUPPORT, // 5
	EAGAIN,
	EALREADY,
	EBADFD,
 	EBADMSG,
 	EBUSY, // 10
 	ECANCELED,
 	ECHILD,
 	ECONNABORTED,
 	ECONNREFUSED,
 	ECONNRESET, // 15
 	EDEADLK,
 	EDESTADDRREQ,
 	EDOM,
 	EDQUOT,
 	EEXIST, // 20
 	EFAULT,
 	EFBIG,
 	EHOSTUNREACH,
 	EIDRM,
 	EILSEQ, // 25
	EINPROGRESS,
 	EINTR,
 	EINVAL,
 	EIO,
	EISCONN, // 30
 	EISDIR,
 	ELOOP,
 	EMFILE,
 	EMLINK,
 	EMSGSIZE, // 35
 	EMULTIHOP,
 	ENAMETOOLONG,
 	ENETDOWN,
 	ENETRESET,
 	ENETUNREACH, // 40
 	ENFILE,
 	ENOBUFS,
 	ENODATA,
 	ENODEV,
 	ENOENT, // 45
 	ENOEXEC,
 	ENOLCK,
 	ENOLINK,
 	ENOMEM,
 	ENOMSG, // 50
 	ENOPROTOOPT,
 	ENOSPC,
 	ENOSR,
 	ENOSTR,
 	ENOSYS, // 55
 	ENOTCONN,
 	ENOTDIR,
 	ENOTEMPTY,
 	ENOTSOCK,
 	ENOTSUP, // 60
 	ENOTTY,
 	ENXIO,
 	EOPNOTSUPP,
 	EOVERFLOW,
 	EPERM, // 65
 	EPIPE,
 	EPROTO,
 	EPROTONOSUPPORT,
 	EPROTOTYPE,
 	ERANGE, // 70
 	EROFS,
 	ESPIPE,
 	ESRCH,
 	ESTALE,
 	ETIME, // 75
 	ETIMEDOUT,
};

static volatile bool _init_busy = false;
static volatile bool _init_abort = false;
static vs32 _last_init_result = -ENETDOWN;
static s32 net_ip_top_fd = -1;
static u8 __net_heap_inited = 0;
static s32 __net_hid=-1;
static heap_cntrl __net_heap;

static char __manage_fs[] ATTRIBUTE_ALIGN(32) = "/dev/net/ncd/manage";
static char __iptop_fs[] ATTRIBUTE_ALIGN(32) = "/dev/net/ip/top";
static char __kd_fs[] ATTRIBUTE_ALIGN(32) = "/dev/net/kd/request";

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

static s32 NetCreateHeap()
{
	u32 level;
	void *net_heap_ptr;

	_CPU_ISR_Disable(level);

	if(__net_heap_inited)
	{
		_CPU_ISR_Restore(level);
		return IPC_OK;
	}

	net_heap_ptr = (void *)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - NET_HEAP_SIZE));
	if((u32)net_heap_ptr < (u32)SYS_GetArena2Lo())
	{
		_CPU_ISR_Restore(level);
		return IPC_ENOMEM;
	}
	SYS_SetArena2Hi(net_heap_ptr);
	__lwp_heap_init(&__net_heap, net_heap_ptr, NET_HEAP_SIZE, 32);
	__net_heap_inited=1;
	_CPU_ISR_Restore(level);
	return IPC_OK;
}

static void* net_malloc(u32 size)
{
	return __lwp_heap_allocate(&__net_heap, size);
}

static BOOL net_free(void *ptr)
{
	return __lwp_heap_free(&__net_heap, ptr);
}

static s32 _net_convert_error(s32 ios_retval)
{
//	return ios_retval;
	if (ios_retval >= 0) return ios_retval;
	if (ios_retval < -sizeof(_net_error_code_map)
		|| !_net_error_code_map[-ios_retval])
			return NET_UNKNOWN_ERROR_OFFSET + ios_retval;
	return -_net_error_code_map[-ios_retval];
}

u32 net_gethostip(void)
{
	u32 ip_addr=0;
	int retries;

	if (net_ip_top_fd < 0) return 0;
	for (retries=0, ip_addr=0; !ip_addr && retries < 5; retries++) {
		ip_addr = IOS_Ioctl(net_ip_top_fd, IOCTL_SO_GETHOSTID, 0, 0, 0, 0);
#ifdef DEBUG_NET
		debug_printf(".");
		fflush(stdout);
#endif
		if (!ip_addr)
			usleep(100000);
	}

	return ip_addr;
}

static s32 net_init_chain(s32 result, void *usrdata);

static void net_init_alarm(syswd_t alarm, void *cb_arg) {
	debug_printf("net_init alarm\n");
	net_init_chain(0, cb_arg);
}

static s32 net_init_chain(s32 result, void *usrdata) {
	struct init_data *data = (struct init_data *) usrdata;
	struct timespec tb;

	debug_printf("net_init chain entered: %u %d\n", data->state, result);

	if (_init_abort) {
		data->state = 0xff;
		goto error;
	}

	switch (data->state) {
	case 0: // open manage fd
		data->state = 1;
		data->result = IOS_OpenAsync(__manage_fs, 0, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			goto done;
		}
		return 0;

	case 1: // get link status
		if (result == IPC_ENOENT) {
			debug_printf("IPC_ENOENT, retrying...\n");
			data->state = 0;
			tb.tv_sec = 0;
			tb.tv_nsec = 100000000;
			data->result = SYS_SetAlarm(data->alarm, &tb, net_init_alarm, data);
			if (data->result) {
				debug_printf("error setting the alarm: %d\n", data->result);
				goto done;
			}
			return 0;
		}

		if (result < 0) {
			data->result = _net_convert_error(result);
			debug_printf("error opening the manage fd: %d\n", data->result);
			goto done;
		}

		data->fd = result;
		data->state = 2;
		data->result = IOS_IoctlvFormatAsync(__net_hid, data->fd, IOCTL_NCD_GETLINKSTATUS, net_init_chain, data, ":d", data->buf, 0x20);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			if (IOS_CloseAsync(data->fd, net_init_chain, data) < 0)
				goto done;
		}
		return 0;

	case 2: // close manage fd
		data->prevres = result;
		data->state = 3;
		data->result = IOS_CloseAsync(data->fd, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			goto done;
		}
		return 0;

	case 3: // open top fd
		if (data->prevres < 0) {
			data->result = _net_convert_error(data->prevres);
			debug_printf("invalid link status %d\n", data->result);
			goto done;
		}

		data->state = 4;
		data->result = IOS_OpenAsync(__iptop_fs, 0, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			goto done;
		}
		return 0;

	case 4: // open request fd
		if (result < 0) {
			data->result = _net_convert_error(result);
			debug_printf("error opening the top fd: %d\n", data->result);
			goto done;
		}

		net_ip_top_fd = result;
		data->state = 5;
		data->result = IOS_OpenAsync(__kd_fs, 0, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			goto error;
		}
		return 0;

	case 5: // NWC24 startup
		if (result < 0) {
			data->result = _net_convert_error(result);
			debug_printf("error opening the request fd: %d\n", data->result);
			data->state = 0xff;
			goto error;
		}

		data->fd = result;
		data->retries = MAX_INIT_RETRIES;
	case 6:
		data->state = 7;
		data->result = IOS_IoctlAsync(data->fd, IOCTL_NWC24_STARTUP, NULL, 0, data->buf, 0x20, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			if (IOS_CloseAsync(data->fd, net_init_chain, data) < 0)
				goto done;
		}
		return 0;
	case 7:
		if (result==0) {
			memcpy(&result, data->buf, sizeof(result));
			if(result==-29 && --data->retries) {
				data->state = 6;
				tb.tv_sec = 0;
				tb.tv_nsec = 100000000;
				data->result = SYS_SetAlarm(data->alarm, &tb, net_init_alarm, data);
				if (data->result) {
					data->state = 0xff;
					debug_printf("error setting the alarm: %d\n", data->result);
					if (IOS_CloseAsync(data->fd, net_init_chain, data) < 0)
						goto error;
				}
				return 0;
			} else if (result == -15) // this happens if it's already been started
				result = 0;
		}

		data->prevres = result;
		data->state = 8;
		data->result = IOS_CloseAsync(data->fd, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			goto error;
		}
		return 0;

	case 8: // socket startup
		if (data->prevres < 0) {
			data->result = _net_convert_error(data->prevres);
			debug_printf("NWC24 startup failed: %d\n", data->result);
			data->state = 0xff;
			goto error;
		}

		data->state = 9;
		data->retries = MAX_IP_RETRIES;
		data->result = IOS_IoctlAsync(net_ip_top_fd, IOCTL_SO_STARTUP, 0, 0, 0, 0, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			goto error;
		}
		return 0;

	case 9: // check ip
		if (result < 0) {
			data->result = _net_convert_error(result);
			debug_printf("socket startup failed: %d\n", data->result);
			data->state = 0xff;
			goto error;
		}

		data->state = 10;
		data->result = IOS_IoctlAsync(net_ip_top_fd, IOCTL_SO_GETHOSTID, 0, 0, 0, 0, net_init_chain, data);
		if (data->result < 0) {
			data->result = _net_convert_error(data->result);
			data->state = 0xff;
			goto error;
		}
		return 0;

	case 10: // done, check result
		if (result == 0) {
			if (!data->retries) {
				data->result = -ETIMEDOUT;
				debug_printf("unable to obtain ip\n");
				data->state = 0xff;
				goto error;
			}

			debug_printf("unable to obtain ip, retrying...\n");
			data->state = 9;
			data->retries--;
			tb.tv_sec = 0;
			tb.tv_nsec = 100000000;
			data->result = SYS_SetAlarm(data->alarm, &tb, net_init_alarm, data);
			if (data->result) {
				data->state = 0xff;
				debug_printf("error setting the alarm: %d\n", data->result);
				goto error;
			}
			return 0;
		}

		data->result = 0;
		goto done;

error:
	case 0xff: // error occured before, last async call finished
		if (net_ip_top_fd >= 0) {
			data->fd = net_ip_top_fd;
			net_ip_top_fd = -1;
			if (IOS_CloseAsync(data->fd, net_init_chain, data) < 0)
				goto done;
			return 0;
		}
		goto done;

	default:
		debug_printf("unknown state in chain %d\n", data->state);
		data->result = -1;

		break;
	}

done:
	SYS_RemoveAlarm(data->alarm);

	_last_init_result = data->result;

	if (data->cb)
		data->cb(data->result, data->usrdata);

	free(data->buf);
	free(data);

	_init_busy = false;

	return 0;
}

s32 net_init_async(netcallback cb, void *usrdata) {
	s32 ret;
	struct init_data *data;

	if (net_ip_top_fd >= 0)
		return 0;

	if (_init_busy)
		return -EBUSY;

	ret = NetCreateHeap();
	if (ret != IPC_OK)
		return ret;

	if (__net_hid == -1)
		__net_hid = iosCreateHeap(1024); //only needed for ios calls

	if (__net_hid < 0)
		return __net_hid;

	data = malloc(sizeof(struct init_data));
	if (!data)
		return -1;

	memset(data, 0, sizeof(struct init_data));

	if (SYS_CreateAlarm(&data->alarm)) {
		debug_printf("error creating alarm\n");
		free(data);
		return -1;
	}

	data->buf = memalign(32, 0x20);
	if (!data->buf) {
		free(data);
		return -1;
	}

	data->cb = cb;
	data->usrdata = usrdata;

	// kick off the callback chain
	_init_busy = true;
	_init_abort = false;
	_last_init_result = -EBUSY;
	net_init_chain(IPC_ENOENT, data);

	return 0;
}

static void net_init_callback(s32 result, void *usrdata) {
	struct init_default_cb *data = (struct init_default_cb *) usrdata;

	data->result = result;
	LWP_ThreadBroadcast(data->queue);

	return;
}

s32 net_init(void) {
	struct init_default_cb data;

	if (net_ip_top_fd >= 0)
		return 0;

	LWP_InitQueue(&data.queue);
	net_init_async((netcallback)net_init_callback, &data);
	LWP_ThreadSleep(data.queue);
	LWP_CloseQueue(data.queue);

	return data.result;
}

s32 net_get_status(void) {
	return _last_init_result;
}

void net_deinit() {
	if (_init_busy) {
		debug_printf("aborting net_init_async\n");
		_init_abort = true;
		while (_init_busy)
			usleep(50);
		debug_printf("net_init_async done\n");
	}

	if (net_ip_top_fd >= 0) IOS_Close(net_ip_top_fd);
	net_ip_top_fd = -1;
	_last_init_result = -ENETDOWN;
}

void net_wc24cleanup() {
    s32 kd_fd;
    STACK_ALIGN(u8, kd_buf, 0x20, 32);

    kd_fd = IOS_Open(__kd_fs, 0);
    if (kd_fd >= 0) {
        IOS_Ioctl(kd_fd, 7, NULL, 0, kd_buf, 0x20);
        IOS_Close(kd_fd);
    }
}

s32 net_get_mac_address(void *mac_buf) {
	s32 fd;
	s32 result;
	void *_mac_buf;
	STACK_ALIGN(u32, manage_buf, 0x20, 32);

	if (mac_buf==NULL) return -EINVAL;

	result = NetCreateHeap();
	if (result!=IPC_OK) return result;

	_mac_buf = net_malloc(6);
	if (_mac_buf==NULL) return IPC_ENOMEM;

	fd = IOS_Open(__manage_fs, 0);
	if (fd<0) {
		net_free(_mac_buf);
		return fd;
	}

	result = IOS_IoctlvFormat(__net_hid, fd, IOCTLV_NCD_GETMACADDRESS, ":dd", manage_buf, 0x20, _mac_buf, 0x06);
	IOS_Close(fd);

	if (result>=0) {
		memcpy(mac_buf, _mac_buf, 6);
		if (manage_buf[0]) result = manage_buf[0];
	}

	net_free(_mac_buf);
	return result;
}

/* Returned value is a static buffer -- this function is not threadsafe! */
struct hostent * net_gethostbyname(const char *addrString)
{
	s32 ret, len, i;
	u8 *params;
	struct hostent *ipData;
	u32 addrOffset;
	static u8 ipBuffer[0x460] ATTRIBUTE_ALIGN(32);

	memset(ipBuffer, 0, 0x460);

	if (net_ip_top_fd < 0) {
		errno = -ENXIO;
		return NULL;
	}

	len = strlen(addrString) + 1;
	params = net_malloc(len);
	if (params==NULL) {
		errno = IPC_ENOMEM;
		return NULL;
	}

	memcpy(params, addrString, len);

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_GETHOSTBYNAME, params, len, ipBuffer, 0x460));

	if(params!=NULL) net_free(params);

	if (ret < 0) {
		errno = ret;
		return NULL;
	}

	ipData = ((struct hostent*)ipBuffer);
	addrOffset = (u32)MEM_PHYSICAL_TO_K0(ipData->h_name) - ((u32)ipBuffer + 0x10);

	ipData->h_name = MEM_PHYSICAL_TO_K0(ipData->h_name) - addrOffset;
	ipData->h_aliases = MEM_PHYSICAL_TO_K0(ipData->h_aliases) - addrOffset;

	for (i=0; (i < 0x40) && (ipData->h_aliases[i] != 0); i++) {
		ipData->h_aliases[i] = MEM_PHYSICAL_TO_K0(ipData->h_aliases[i]) - addrOffset;
	}

	ipData->h_addr_list = MEM_PHYSICAL_TO_K0(ipData->h_addr_list) - addrOffset;

	for (i=0; (i < 0x40) && (ipData->h_addr_list[i] != 0); i++) {
		ipData->h_addr_list[i] = MEM_PHYSICAL_TO_K0(ipData->h_addr_list[i]) - addrOffset;
	}

	errno = 0;
	return ipData;
}

s32 net_socket(u32 domain, u32 type, u32 protocol)
{
	s32 ret;
	STACK_ALIGN(u32, params, 3, 32);

	if (net_ip_top_fd < 0) return -ENXIO;

	params[0] = domain;
	params[1] = type;
	params[2] = protocol;

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_SOCKET, params, 12, NULL, 0));
	if(ret>=0) // set tcp window size to 32kb
	{
		int window_size = 32768;
		net_setsockopt(ret, SOL_SOCKET, SO_RCVBUF, (char *) &window_size, sizeof(window_size));
	}
	debug_printf("net_socket(%d, %d, %d)=%d\n", domain, type, protocol, ret);
	return ret;
}

s32 net_shutdown(s32 s, u32 how)
{
	s32 ret;
	STACK_ALIGN(u32, params, 2, 32);

	if (net_ip_top_fd < 0) return -ENXIO;

	params[0] = s;
	params[1] = how;
	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_SHUTDOWN, params, 8, NULL, 0));

	debug_printf("net_shutdown(%d, %d)=%d\n", s, how, ret);
	return ret;
}

s32 net_bind(s32 s, struct sockaddr *name, socklen_t namelen)
{
	s32 ret;
	STACK_ALIGN(struct bind_params,params,1,32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (name->sa_family != AF_INET) return -EAFNOSUPPORT;

	name->sa_len = 8;

	memset(params, 0, sizeof(struct bind_params));
	params->socket = s;
	params->has_name = 1;
	memcpy(params->name, name, 8);

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_BIND, params, sizeof (struct bind_params), NULL, 0));
	debug_printf("net_bind(%d, %p)=%d\n", s, name, ret);

	return ret;
}

s32 net_listen(s32 s, u32 backlog)
{
	s32 ret;
	STACK_ALIGN(u32, params, 2, 32);

	if (net_ip_top_fd < 0) return -ENXIO;

	params[0] = s;
	params[1] = backlog;

	debug_printf("calling ios_ioctl(%d, %d, %p, %d)\n", net_ip_top_fd, IOCTL_SO_SOCKET, params, 8);

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_LISTEN, params, 8, NULL, 0));
  	debug_printf("net_listen(%d, %d)=%d\n", s, backlog, ret);
	return ret;
}

s32 net_accept(s32 s, struct sockaddr *addr, socklen_t *addrlen)
{
	s32 ret;
	STACK_ALIGN(u32, _socket, 1, 32);

	debug_printf("net_accept()\n");

	if (net_ip_top_fd < 0) return -ENXIO;

	if (!addr) return -EINVAL;
	addr->sa_len = 8;
	addr->sa_family = AF_INET;

	if (!addrlen) return -EINVAL;

	if (*addrlen < 8) return -ENOMEM;

	*addrlen = 8;

	*_socket = s;
	debug_printf("calling ios_ioctl(%d, %d, %p, %d)\n", net_ip_top_fd, IOCTL_SO_ACCEPT, _socket, 4);
	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_ACCEPT, _socket, 4, addr, *addrlen));

	debug_printf("net_accept(%d, %p)=%d\n", s, addr, ret);
	return ret;
}

s32 net_connect(s32 s, struct sockaddr *addr, socklen_t addrlen)
{
	s32 ret;
	STACK_ALIGN(struct connect_params,params,1,32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (addr->sa_family != AF_INET) return -EAFNOSUPPORT;
	if (addrlen < 8) return -EINVAL;

	addr->sa_len = 8;

	memset(params, 0, sizeof(struct connect_params));
	params->socket = s;
	params->has_addr = 1;
	memcpy(&params->addr, addr, addrlen);

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_CONNECT, params, sizeof(struct connect_params), NULL, 0));
	if (ret < 0)
    	debug_printf("SOConnect(%d, %p)=%d\n", s, addr, ret);

  	return ret;
}

s32 net_write(s32 s, const void *data, s32 size)
{
    return net_send(s, data, size, 0);
}

s32 net_send(s32 s, const void *data, s32 size, u32 flags)
{
	return net_sendto(s, data, size, flags, NULL, 0);
}

s32 net_sendto(s32 s, const void *data, s32 len, u32 flags, struct sockaddr *to, socklen_t tolen)
{
	s32 ret;
	u8 * message_buf = NULL;
	STACK_ALIGN(struct sendto_params,params,1,32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (tolen > 28) return -EOVERFLOW;

	message_buf = net_malloc(len);
	if (message_buf == NULL) {
		debug_printf("net_send: failed to alloc %d bytes\n", len);
		return IPC_ENOMEM;
	}

	debug_printf("net_sendto(%d, %p, %d, %d, %p, %d)\n", s, data, len, flags, to, tolen);

	if (to && to->sa_len != tolen) {
		debug_printf("warning: to->sa_len was %d, setting to %d\n",	to->sa_len, tolen);
		to->sa_len = tolen;
	}

	memset(params, 0, sizeof(struct sendto_params));
	memcpy(message_buf, data, len);   // ensure message buf is aligned

	params->socket = s;
	params->flags = flags;
	if (to) {
		params->has_destaddr = 1;
		memcpy(params->destaddr, to, to->sa_len);
	} else {
		params->has_destaddr = 0;
	}

	ret = _net_convert_error(IOS_IoctlvFormat(__net_hid, net_ip_top_fd, IOCTLV_SO_SENDTO, "dd:", message_buf, len, params, sizeof(struct sendto_params)));
	debug_printf("net_send retuned %d\n", ret);

	if(message_buf!=NULL) net_free(message_buf);
	return ret;
}

s32 net_recv(s32 s, void *mem, s32 len, u32 flags)
{
    return net_recvfrom(s, mem, len, flags, NULL, NULL);
}

s32 net_recvfrom(s32 s, void *mem, s32 len, u32 flags, struct sockaddr *from, socklen_t *fromlen)
{
	s32 ret;
	u8* message_buf = NULL;
	STACK_ALIGN(u32, params, 2, 32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (len<=0) return -EINVAL;

	if (fromlen && from->sa_len != *fromlen) {
		debug_printf("warning: from->sa_len was %d, setting to %d\n",from->sa_len, *fromlen);
		from->sa_len = *fromlen;
	}

	message_buf = net_malloc(len);
	if (message_buf == NULL) {
    	debug_printf("SORecv: failed to alloc %d bytes\n", len);
		return IPC_ENOMEM;
	}

	debug_printf("net_recvfrom(%d, '%s', %d, %d, %p, %d)\n", s, (char *)mem, len, flags, from, fromlen?*fromlen:0);

	memset(message_buf, 0, len);
	params[0] = s;
	params[1] = flags;

	ret = _net_convert_error(IOS_IoctlvFormat(__net_hid, net_ip_top_fd, IOCTLV_SO_RECVFROM,	"d:dd", params, 8, message_buf, len, from, (fromlen?*fromlen:0)));
	debug_printf("net_recvfrom returned %d\n", ret);

	if (ret > 0) {
		if (ret > len) {
			ret = -EOVERFLOW;
			goto done;
		}

		memcpy(mem, message_buf, ret);
	}

	if (fromlen && from) *fromlen = from->sa_len;

done:
	if(message_buf!=NULL) net_free(message_buf);
	return ret;
}

s32 net_read(s32 s, void *mem, s32 len)
{
	return net_recvfrom(s, mem, len, 0, NULL, NULL);
}

s32 net_close(s32 s)
{
	s32 ret;
	STACK_ALIGN(u32, _socket, 1, 32);

	if (net_ip_top_fd < 0) return -ENXIO;

	*_socket = s;
	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_CLOSE, _socket, 4, NULL, 0));

	if (ret < 0)
		debug_printf("net_close(%d)=%d\n", s, ret);

	return ret;
}

s32 net_select(s32 maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout)
{
	// not yet implemented
	return -EINVAL;
}

s32 net_setsockopt(s32 s, u32 level, u32 optname, const void *optval, socklen_t optlen)
{
	s32 ret;
	STACK_ALIGN(struct setsockopt_params,params,1,32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (optlen < 0 || optlen > 20) return -EINVAL;

	memset(params, 0, sizeof(struct setsockopt_params));
	params->socket = s;
	params->level = level;
	params->optname = optname;
	params->optlen = optlen;
	if (optval && optlen) memcpy (params->optval, optval, optlen);

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_SETSOCKOPT, params, sizeof(struct setsockopt_params), NULL, 0));

	debug_printf("net_setsockopt(%d, %u, %u, %p, %d)=%d\n",	s, level, optname, optval, optlen, ret);
	return ret;
}

s32 net_ioctl(s32 s, u32 cmd, void *argp)
{
	u32 flags;
	u32 *intp = (u32 *)argp;

	if (net_ip_top_fd < 0) return -ENXIO;
	if (!intp) return -EINVAL;

	switch (cmd) {
		case FIONBIO:
			flags = net_fcntl(s, F_GETFL, 0);
			flags &= ~IOS_O_NONBLOCK;
			if (*intp) flags |= IOS_O_NONBLOCK;
			return net_fcntl(s, F_SETFL, flags);
		default:
			return -EINVAL;
	}
}

s32 net_fcntl(s32 s, u32 cmd, u32 flags)
{
	s32 ret;
	STACK_ALIGN(u32, params, 3, 32);

	if (net_ip_top_fd < 0) return -ENXIO;
	if (cmd != F_GETFL && cmd != F_SETFL) return -EINVAL;

	params[0] = s;
	params[1] = cmd;
	params[2] = flags;

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_FCNTL, params, 12, NULL, 0));

	debug_printf("net_fcntl(%d, %d, %x)=%d\n", params[0], params[1], params[2], ret);

	return ret;
}

/*!
 * \fn s32 net_poll(struct pollsd *sds, u32 nsds, s64 timeout)
 * \brief Poll a set of sockets for a set of events.
 *
 * \param[in] sds a pointer to an array of pollsd structures
 * \param[in] nsds the number of elements in the sds array
 * \param[in] time in milliseconds before the function should timeout
 *
 * \return the number of structures in sds that now have non-zero revent fields
 */
s32 net_poll(struct pollsd *sds,s32 nsds,s32 timeout)
{
	union ullc {
		u64 ull;
		u32 ul[2];
	};

	s32 ret;
	union ullc outv;
	struct pollsd *psds;
	STACK_ALIGN(u64,params,1,32);

	if(net_ip_top_fd<0) return -ENXIO;
	if(sds==NULL || nsds==0) return -EINVAL;

	psds = net_malloc((nsds*sizeof(struct pollsd)));
	if(psds==NULL) {
		debug_printf("net_poll: failed to alloc %d bytes\n", nsds * sizeof(struct pollsd));
		return IPC_ENOMEM;
	}

	outv.ul[0] = 0;
	outv.ul[1] = timeout;
	params[0] = outv.ull;
	memcpy(psds,sds,(nsds*sizeof(struct pollsd)));

	ret = _net_convert_error(IOS_Ioctl(net_ip_top_fd, IOCTL_SO_POLL, params, 8, psds, (nsds * sizeof(struct pollsd))));

	memcpy(sds,psds,(nsds*sizeof(struct pollsd)));

	net_free(psds);

	debug_printf("net_poll(sds, %d, %lld)=%d\n", nsds, params[0], ret);

	return ret;
}

s32 if_config(char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries)
{
	s32 i,ret;
	struct in_addr hostip;

	if (!use_dhcp)
		return -EINVAL;

	for (i = 0; i < MAX_INIT_RETRIES; ++i) {
		ret = net_init();

		if ((ret != -EAGAIN) && (ret != -ETIMEDOUT))
			break;

		usleep(50 * 1000);
	}

	if (ret < 0)
		return ret;

	hostip.s_addr = net_gethostip();
	if (local_ip && hostip.s_addr) {
		strcpy(local_ip, inet_ntoa(hostip));
		return 0;
	}

	return -1;
}

s32 if_configex(struct in_addr *local_ip, struct in_addr *netmask, struct in_addr *gateway,bool use_dhcp, int max_retries)
{
	s32 i,ret;
	struct in_addr hostip;

	if (!use_dhcp)
		return -EINVAL;

	for (i = 0; i < MAX_INIT_RETRIES; ++i) {
		ret = net_init();

		if ((ret != -EAGAIN) && (ret != -ETIMEDOUT))
			break;

		usleep(50 * 1000);
	}

	if (ret < 0)
		return ret;

	hostip.s_addr = net_gethostip();
	if (local_ip && hostip.s_addr) {
		*local_ip = hostip;
		return 0;
	}

	return -1;
}

#endif /* defined(HW_RVL) */
