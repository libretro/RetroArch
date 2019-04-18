#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <3ds.h>

#define FILE_CHUNK_SIZE 4096

typedef struct{
	u32 argc;
	char args[0x300 - 0x4];
}ciaParam;

char argvHmac[0x20] = {0x1d, 0x78, 0xff, 0xb9, 0xc5, 0xbc, 0x78, 0xb7, 0xac, 0x29, 0x1d, 0x3e, 0x16, 0xd0, 0xcf, 0x53, 0xef, 0x12, 0x58, 0x83, 0xb6, 0x9e, 0x2f, 0x79, 0x47, 0xf9, 0x35, 0x61, 0xeb, 0x50, 0xd7, 0x67};

static void errorAndQuit(const char* errorStr){
	errorConf error;

	errorInit(&error, ERROR_TEXT, CFG_LANGUAGE_EN);
	errorText(&error, errorStr);
	errorDisp(&error);
	exit(0);
}

static int isCiaInstalled(u64 titleId, u16 version){
	u32 titlesToRetrieve;
	u32 titlesRetrieved;
	u64* titleIds;
	bool titleExists = false;
	AM_TitleEntry titleInfo;
	Result failed;

	failed = AM_GetTitleCount(MEDIATYPE_SD, &titlesToRetrieve);
	if(R_FAILED(failed))
		return -1;

	titleIds = malloc(titlesToRetrieve * sizeof(uint64_t));
	if(titleIds == NULL)
		return -1;

	failed = AM_GetTitleList(&titlesRetrieved, MEDIATYPE_SD, titlesToRetrieve, titleIds);
	if(R_FAILED(failed))
		return -1;

	for(u32 titlesToCheck = 0; titlesToCheck < titlesRetrieved; titlesToCheck++){
		if(titleIds[titlesToCheck] == titleId){
			titleExists = true;
			break;
		}
	}

	free(titleIds);

	if(titleExists){
		failed = AM_GetTitleInfo(MEDIATYPE_SD, 1 /*titleCount*/, &titleId, &titleInfo);
		if(R_FAILED(failed))
			return -1;

		if(titleInfo.version == version)
			return 1;
	}

	return 0;
}

static int installCia(Handle ciaFile){
	Result failed;
	Handle outputHandle;
	u64 fileSize;
	u64 fileOffset = 0;
	u32 bytesRead;
	u32 bytesWritten;
	u8 transferBuffer[FILE_CHUNK_SIZE];

	failed = AM_StartCiaInstall(MEDIATYPE_SD, &outputHandle);
	if(R_FAILED(failed))
		return -1;

	failed = FSFILE_GetSize(ciaFile, &fileSize);
	if(R_FAILED(failed))
		return -1;

	while(fileOffset < fileSize){
		u64 bytesRemaining = fileSize - fileOffset;
		failed = FSFILE_Read(ciaFile, &bytesRead, fileOffset, transferBuffer, bytesRemaining < FILE_CHUNK_SIZE ? bytesRemaining : FILE_CHUNK_SIZE);
		if(R_FAILED(failed)){
			AM_CancelCIAInstall(outputHandle);
			return -1;
		}

		failed = FSFILE_Write(outputHandle, &bytesWritten, fileOffset, transferBuffer, bytesRead, 0);
		if(R_FAILED(failed)){
			AM_CancelCIAInstall(outputHandle);
			if(R_DESCRIPTION(failed) == RD_ALREADY_EXISTS)
				return 1;
			return -1;
		}

		if(bytesWritten != bytesRead){
			AM_CancelCIAInstall(outputHandle);
			return -1;
		}

		fileOffset += bytesWritten;
	}

	failed = AM_FinishCiaInstall(outputHandle);
	if(R_FAILED(failed))
		return -1;

	return 1;
}

int exec_cia(const char* path, const char** args){
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

	inited = R_SUCCEEDED(amInit()) && R_SUCCEEDED(fsInit());
	if(inited){
		Result res;
		AM_TitleEntry ciaInfo;
		FS_Archive ciaArchive;
		Handle ciaFile;
		int ciaInstalled;
		ciaParam param;
		int argsLength;

		//open cia file
		res = FSUSER_OpenArchive(&ciaArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
		if(R_FAILED(res))
			errorAndQuit("Cant open SD FS archive.");

		res = FSUSER_OpenFile(&ciaFile, ciaArchive, fsMakePath(PATH_ASCII, path + 5/*skip "sdmc:"*/), FS_OPEN_READ, 0);
		if(R_FAILED(res))
			errorAndQuit("Cant open CIA file.");

		res = AM_GetCiaFileInfo(MEDIATYPE_SD, &ciaInfo, ciaFile);
		if(R_FAILED(res))
			errorAndQuit("Cant get CIA file info.");

		ciaInstalled = isCiaInstalled(ciaInfo.titleID, ciaInfo.version);
		if(ciaInstalled == -1){
			//error
			errorAndQuit("Could not read title id list.");
		}
		else if(ciaInstalled == 0){
			//not installed
			int error = installCia(ciaFile);
			if(error == -1)
				errorAndQuit("Cant install CIA.");
		}

		FSFILE_Close(ciaFile);
		FSUSER_CloseArchive(ciaArchive);

		param.argc = 0;
		argsLength = 0;
		char* argLocation = param.args;
		while(args[param.argc] != NULL){
			strcpy(argLocation, args[param.argc]);
			argLocation += strlen(args[param.argc]) + 1;
			argsLength += strlen(args[param.argc]) + 1;
			param.argc++;
		}

		res = APT_PrepareToDoApplicationJump(0, ciaInfo.titleID, 0x1);
		if(R_FAILED(res))
			errorAndQuit("CIA cant run, cant prepare.");

		res = APT_DoApplicationJump(&param, sizeof(param.argc) + argsLength, argvHmac);
		if(R_FAILED(res))
			errorAndQuit("CIA cant run, cant jump.");

		//wait for application jump, for some reason its not instant
		while(1);
	}

	//should never be reached
	amExit();
	fsExit();
	errno = ENOTSUP;
	return -1;
}
