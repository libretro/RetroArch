#ifndef CD_H
#define CD_H

#include <stdint.h>
#include <fileXio_rpc.h>
#include <fileXio.h>

#define CDVD_INIT_INIT		0x00
#define CDVD_INIT_NOCHECK	0x01
#define CDVD_INIT_EXIT		0x05

typedef enum {
	CDVD_TYPE_NODISK =	0x00,		// No Disc inserted
	CDVD_TYPE_DETECT,			// Detecting disc type
	CDVD_TYPE_DETECT_CD,
	CDVD_TYPE_DETECT_DVDSINGLE,
	CDVD_TYPE_DETECT_DVDDUAL,
	CDVD_TYPE_UNKNOWN,			// Unknown disc type

	CDVD_TYPE_PS1CD	=	0x10,		// PS1 CD with no CDDA tracks
	CDVD_TYPE_PS1CDDA,			// PS1 CD with CDDA tracks
	CDVD_TYPE_PS2CD,			// PS2 CD with no CDDA tracks
	CDVD_TYPE_PS2CDDA,			// PS2 CD with CDDA tracks
	CDVD_TYPE_PS2DVD,			// PS2 DVD

	CDVD_TYPE_CDDA =	0xFD,		// CDDA
	CDVD_TYPE_DVDVIDEO,			// DVD Video
	CDVD_TYPE_ILLEGAL,			// Illegal disk type
} CdvdDiscType_t;

s32 cdInit(s32);
CdvdDiscType_t cdGetDiscType(void);




int newfileXioDopen(const char *name);
int newfileXioDread(int fd, iox_dirent_t *dirent);

#endif
