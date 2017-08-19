#ifndef __DEBUG_SUPP_H__
#define __DEBUG_SUPP_H__

#include <gctypes.h>

#define QM_MAXTHREADS			(20)

struct gdbstub_threadinfo {
	char display[256];
	char more_display[256];
	char name[256];
};

s32 gdbstub_getcurrentthread();
s32 hstr2nibble(const char *buf,s32 *nibble);
char* int2vhstr(char *buf,s32 val);
char* mem2hstr(char *buf,const char *mem,s32 count);
char* thread2vhstr(char *buf,s32 thread);
const char* vhstr2thread(const char *buf,s32 *thread);
lwp_cntrl* gdbstub_indextoid(s32 thread);
s32 gdbstub_getoffsets(char **textaddr,char **dataaddr,char **bssaddr);
s32 parsezbreak(const char *in,s32 *type,char **addr,u32 *len);
s32 gdbstub_getthreadinfo(s32 thread,struct gdbstub_threadinfo *info);
s32 parseqp(const char *in,s32 *mask,s32 *thread);
void packqq(char *out,s32 mask,s32 thread,struct gdbstub_threadinfo *info);
char* reserve_qmheader(char *out);
s32 parseql(const char *in,s32 *first,s32 *max_cnt,s32 *athread);
s32 gdbstub_getnextthread(s32 athread);
char* packqmthread(char *out,s32 thread);
void packqmheader(char *out,s32 count,s32 done,s32 athread);

#endif
