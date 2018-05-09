#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "mini-hb-menu/common.h"


extern const loaderFuncs_s loader_Ninjhax1;
extern const loaderFuncs_s loader_Ninjhax2;
extern const loaderFuncs_s loader_Rosalina;

static argData_s newProgramArgs;;//the argv variable must remain in memory even when the application exits for the new program to load properly


int exec_3dsx(const char* path, const char* args){
	struct stat sBuff; 
	bool fileExists;
	bool inited;
	
	if(path == NULL || path[0] == '\0'){
		errno = EINVAL;
		return -1;
	}
	
	fileExists = stat(path, &sBuff) == 0;
	if(!fileExists){
		errno = ENOENT;
		return -1;
	}
	else if(S_ISDIR(sBuff.st_mode)){
		errno = EINVAL;;
		return -1;
	}
	
	if(args == NULL || args[0] == '\0')
		args = path;
	
	int argsSize = strlen(args);
	strncpy((char*)newProgramArgs.buf , args, ENTRY_ARGBUFSIZE);
	if(argsSize >= ENTRY_ARGBUFSIZE)
		((char*)&newProgramArgs.buf[0])[ENTRY_ARGBUFSIZE - 1] = '\0';
	newProgramArgs.dst = (char*)newProgramArgs.buf + (argsSize < ENTRY_ARGBUFSIZE ? argsSize : ENTRY_ARGBUFSIZE);
	
	inited = loader_Rosalina.init();
	if(inited){
		loader_Rosalina.launchFile(path, &newProgramArgs, NULL);
		exit(0);
	}
	
	inited = loader_Ninjhax2.init();
	if(inited){
		loader_Ninjhax2.launchFile(path, &newProgramArgs, NULL);
		exit(0);
	}
	
	inited = loader_Ninjhax1.init();
	if(inited){
		loader_Ninjhax1.launchFile(path, &newProgramArgs, NULL);
		exit(0);
	}
	
	//should never be reached
	errno = ENOTSUP;
	return -1;
}