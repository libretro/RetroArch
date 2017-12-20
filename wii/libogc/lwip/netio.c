#if 0
#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#undef errno
extern int errno;

#include <sys/iosupport.h>
#include "lwip/ip_addr.h"
#include "network.h"
int netio_open(struct _reent *r, void *fileStruct, const char *path,int flags,int mode);
int netio_close(struct _reent *r,void *fd);
int netio_write(struct _reent *r,void *fd,const char *ptr,size_t len);
int netio_read(struct _reent *r,void *fd,char *ptr,size_t len);

//---------------------------------------------------------------------------------
const devoptab_t dotab_stdnet = {
//---------------------------------------------------------------------------------
	"stdnet",	// device name
	0,		// size of file structure
	netio_open,	// device open
	netio_close,	// device close
	netio_write,	// device write
	netio_read,	// device read
	NULL,		// device seek
	NULL,		// device fstat
	NULL,		// device stat
	NULL,		// device link
	NULL,		// device unlink
	NULL,		// device chdir
	NULL,		// device rename
	NULL,		// device mkdir
	0,		// dirStateSize
	NULL,		// device diropen_r
	NULL,		// device dirreset_r
	NULL,		// device dirnext_r
	NULL,		// device dirclose_r
	NULL		// device statvfs_r
};

int netio_open(struct _reent *r, void *fileStruct, const char *path,int flags,int mode)
{
	char *cport = NULL;
	int optval = 1,nport = -1,udp_sock = INVALID_SOCKET;
	struct sockaddr_in name;
	socklen_t namelen = sizeof(struct sockaddr);

	if(net_init()==SOCKET_ERROR) return -1;

	udp_sock = net_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(udp_sock==INVALID_SOCKET) return -1;

	cport = strchr(path,':');
	if(cport) {
		*cport++ = '\0';
		nport = atoi(cport);
	}
	if(nport==-1) nport = 7;	//try to connect to the well known port 7

	name.sin_addr.s_addr = inet_addr(path);
	name.sin_port = htons(nport);
	name.sin_family = AF_INET;
	if(net_connect(udp_sock,(struct sockaddr*)&name,namelen)==-1) {
		net_close(udp_sock);
		return -1;
	}
	net_setsockopt(udp_sock,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof(optval));

	return udp_sock;
}

int netio_close(struct _reent *r,void *fd)
{
	if(fd<0) return -1;

	net_close(fd);

	return 0;
}

int netio_write(struct _reent *r,void *fd,const char *ptr,size_t len)
{
	int ret;

	if(fd<0) return -1;

	ret = net_write(fd,(void*)ptr,len);

	return ret;
}

int netio_read(struct _reent *r,void *fd,char *ptr,size_t len)
{
	int ret;

	if(fd<0) return -1;

	ret = net_read(fd,ptr,len);

	return ret;
}
#endif
