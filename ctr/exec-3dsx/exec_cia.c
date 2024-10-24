#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <3ds.h>
#include "ctr/ctr_debug.h"

#define FILE_CHUNK_SIZE 4096

PrintConsole debugConsoleTop;

typedef struct
{
   u32 argc;
   char args[0x300 - 0x4];
}ciaParam;

char argvHmac[0x20] = {0x1d, 0x78, 0xff, 0xb9, 0xc5, 0xbc, 0x78, 0xb7, 0xac, 0x29, 0x1d, 0x3e, 0x16, 0xd0, 0xcf, 0x53, 0xef, 0x12, 0x58, 0x83, 0xb6, 0x9e, 0x2f, 0x79, 0x47, 0xf9, 0x35, 0x61, 0xeb, 0x50, 0xd7, 0x67};

static int isCiaInstalled(u64 titleId, u16 version)
{
   u32 titlesToRetrieve;
   u32 titlesRetrieved;
   u64* titleIds;
   u32 titlesToCheck;
   AM_TitleEntry titleInfo;
   bool titleExists = false;
   Result failed    = AM_GetTitleCount(MEDIATYPE_SD, &titlesToRetrieve);
   if (R_FAILED(failed))
      return -1;

   titleIds = malloc(titlesToRetrieve * sizeof(uint64_t));
   if (!titleIds)
      return -1;

   failed = AM_GetTitleList(&titlesRetrieved, MEDIATYPE_SD, titlesToRetrieve, titleIds);
   if (R_FAILED(failed))
      return -1;

   for(titlesToCheck = 0; titlesToCheck < titlesRetrieved; titlesToCheck++)
   {
      if (titleIds[titlesToCheck] == titleId)
      {
         titleExists = true;
         break;
      }
   }

   free(titleIds);

   if (titleExists)
   {
      failed = AM_GetTitleInfo(MEDIATYPE_SD,
            1 /*titleCount*/, &titleId, &titleInfo);
      if (R_FAILED(failed))
         return -1;

      if (titleInfo.version == version)
         return 1;
   }

   return 0;
}

static int deleteCia(u64 TitleId)
{
   u64 currTitleId = 0;

   /*  Do not delete if the titleid is currently running. */
   if (R_FAILED(APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &currTitleId, NULL, NULL, NULL, NULL)) || TitleId != currTitleId)
   {
      AM_DeleteTitle(MEDIATYPE_SD, TitleId);
      AM_DeleteTicket(TitleId);
   }
}

static int installCia(Handle ciaFile)
{
   Handle outputHandle;
   u64 fileSize;
   u32 bytesRead;
   u32 bytesWritten;
   u8 transferBuffer[FILE_CHUNK_SIZE];
   u64 fileOffset = 0;
   Result failed  = AM_StartCiaInstall(MEDIATYPE_SD, &outputHandle);
   if (R_FAILED(failed))
      return -1;

   failed = FSFILE_GetSize(ciaFile, &fileSize);
   if (R_FAILED(failed))
      return -1;

   while(fileOffset < fileSize)
   {
      u64 bytesRemaining = fileSize - fileOffset;
      failed             = FSFILE_Read(ciaFile, &bytesRead,
            fileOffset, transferBuffer,
            bytesRemaining < FILE_CHUNK_SIZE 
            ? bytesRemaining 
            : FILE_CHUNK_SIZE);
      if (R_FAILED(failed))
      {
         AM_CancelCIAInstall(outputHandle);
         return -1;
      }

      failed = FSFILE_Write(outputHandle, &bytesWritten,
            fileOffset, transferBuffer, bytesRead, FS_WRITE_FLUSH);
      if (R_FAILED(failed))
      {
         AM_CancelCIAInstall(outputHandle);
         if (R_DESCRIPTION(failed) == RD_ALREADY_EXISTS)
            return 1;

         return -1;
      }

      printf(PRINTFPOS(27,0)" %llu / %llu\n\n", fileOffset, fileSize);

      int progress = roundf(((float)fileOffset/(float)fileSize)*50);
      for (int i = 0; i < progress; i++)
      {
         printf("#");
      }

      if (bytesWritten != bytesRead)
      {
         AM_CancelCIAInstall(outputHandle);
         return -1;
      }

      fileOffset += bytesWritten;
   }

   failed = AM_FinishCiaInstall(outputHandle);
   if (R_FAILED(failed))
      return -1;

   return 1;
}

int exec_cia(const char* path, const char** args)
{
   struct stat sBuff;
   bool fileExists;
   bool inited;
#ifdef USE_CTRULIB_2
   if (!gspHasGpuRight())
#endif
       gfxInitDefault();

   consoleInit(GFX_TOP, &debugConsoleTop);

   if (!path || path[0] == '\0')
   {
      errno = EINVAL;
      return -1;
   }

   fileExists = stat(path, &sBuff) == 0;
   if (!fileExists)
   {
      errno = ENOENT;
      return -1;
   }
   else if (S_ISDIR(sBuff.st_mode))
   {
      errno = EINVAL;
      return -1;
   }

   inited = R_SUCCEEDED(amInit()) && R_SUCCEEDED(fsInit());

   if (inited)
   {
      AM_TitleEntry ciaInfo;
      FS_Archive ciaArchive;
      Handle ciaFile;
      int ciaInstalled;
      ciaParam param;
      int argsLength;
      /* open CIA file */
      Result res = FSUSER_OpenArchive(&ciaArchive,
            ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

      if (R_FAILED(res))
         error_and_quit("Cant open SD FS archive.");

      res = FSUSER_OpenFile(&ciaFile,
            ciaArchive, fsMakePath(PATH_ASCII, path + 5/*skip "sdmc:"*/),
            FS_OPEN_READ, 0);
      if (R_FAILED(res))
         error_and_quit("Cant open CIA file.");

      res = AM_GetCiaFileInfo(MEDIATYPE_SD, &ciaInfo, ciaFile);
      if (R_FAILED(res))
         error_and_quit("Cant get CIA file info.");

      ciaInstalled = isCiaInstalled(ciaInfo.titleID, ciaInfo.version);
      if (ciaInstalled == -1)
      {
         /* error */
         error_and_quit("Could not read title ID list.");
      }
      else if (ciaInstalled == 0)
      {
         /* titleid with version not installed. */
         consoleSelect(&debugConsoleTop);
         printf(PRINTFPOS(2,0)"Do not close RetroArch or turn off your console.");
         printf(PRINTFPOS(5,0)"Installing core:\n%s\n", path);

         /*  Delete existing content of the pending titleid. */
         deleteCia(ciaInfo.titleID);

         int error = installCia(ciaFile);
         if (error == -1)
         {
            printf(PRINTFPOS(25,0)"CIA install failed.\n");
            wait_for_input();
         }
      }

      FSFILE_Close(ciaFile);
      FSUSER_CloseArchive(ciaArchive);

      param.argc        = 0;
      argsLength        = 0;
      char *argLocation = param.args;

      while(args[param.argc])
      {
         strcpy(argLocation, args[param.argc]);
         argLocation += strlen(args[param.argc]) + 1;
         argsLength += strlen(args[param.argc]) + 1;
         param.argc++;
      }

      res = APT_PrepareToDoApplicationJump(0, ciaInfo.titleID, 0x1);
      if (R_FAILED(res))
         error_and_quit("CIA cant run, cant prepare.");

      res = APT_DoApplicationJump(&param, sizeof(param.argc) 
            + argsLength, argvHmac);
      if (R_FAILED(res))
         error_and_quit("CIA cant run, cant jump.");

      /* wait for application jump, for some reason its not instant */
      while(1);
   }

   /* should never be reached */
   amExit();
   fsExit();
   errno = ENOTSUP;
   return -1;
}
