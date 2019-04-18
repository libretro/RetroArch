#ifndef _CDVD_RPC_H
#define _CDVD_RPC_H

// include the common definitions
#include "../common/cdvd.h"

#ifdef __cplusplus
extern "C" {
#endif


int CDVD_Init();
int CDVD_DiskReady(int mode);
int CDVD_FindFile(const char *fname, struct TocEntry *tocEntry);
void CDVD_Stop();
int CDVD_TrayReq(int mode);
int CDVD_DiskReady(int mode);
int CDVD_GetDir(const char *pathname, const char *extensions, enum CDVD_getMode getMode, struct TocEntry tocEntry[], unsigned int req_entries, char *new_pathname);
void CDVD_FlushCache();
unsigned int CDVD_GetSize();


#ifdef __cplusplus
}
#endif


#endif  // _CDVD_H
