#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <boolean.h>
#include <file/file_path.h>
#include <compat/strl.h>

#include <discord_rpc.h>

int get_process_id(void)
{
    return getpid();
}

/* we want to register games so we can run them from 
 * Discord client as discord-<appid>:// */
void Discord_Register(const char *applicationId, const char *command)
{
   size_t _len;
   FILE* fp;
   int fileLen;
   char xdgMimeCommand[1024];
   char desktopFilename[256];
   char desktopFilePath[1024];
   char desktopFile[2048];
   /* Add a desktop file and update some MIME handlers 
    * so that xdg-open does the right thing. */
   char exePath[1024];
   const char* home = getenv("HOME");
   if (!home)
      return;

   if (!command || !command[0])
   {
      ssize_t size = readlink("/proc/self/exe", exePath, sizeof(exePath));
      if (size <= 0 || size >= (ssize_t)sizeof(exePath))
         return;
      exePath[size] = '\0';
      command = exePath;
   }

   {
      const char* desktopFileFormat = "[Desktop Entry]\n"
         "Name=Game %s\n"
         "Exec=%s\n" /* note: it really wants that %u in there */
         "Type=Application\n"
         "NoDisplay=true\n"
         "Categories=Discord;Games;\n"
         "MimeType=x-scheme-handler/discord-%s;\n";
      fileLen = snprintf(
            desktopFile, sizeof(desktopFile), desktopFileFormat, applicationId, command, applicationId);
      if (fileLen <= 0)
         return;
   }

   _len  = strlcpy(desktopFilename,
         "/discord-",
         sizeof(desktopFilename));
   _len += strlcpy(desktopFilename + _len,
         applicationId,
         sizeof(desktopFilename)   - _len);
   _len += strlcpy(desktopFilename + _len,
         ".desktop",
         sizeof(desktopFilename)   - _len);

   _len  = strlcpy(desktopFilePath,
         home,
         sizeof(desktopFilePath));
   _len += strlcpy(desktopFilePath + _len,
         "/.local",
         sizeof(desktopFilePath)   - _len);
   if (!path_mkdir(desktopFilePath))
      return;
   _len += strlcpy(desktopFilePath + _len,
         "/share",
         sizeof(desktopFilePath)   - _len);
   if (!path_mkdir(desktopFilePath))
      return;
   _len += strlcpy(desktopFilePath + _len,
         "/applications",
         sizeof(desktopFilePath)   - _len);
   if (!path_mkdir(desktopFilePath))
      return;
   _len += strlcpy(desktopFilePath + _len,
         desktopFilename,
         sizeof(desktopFilePath)   - _len);

   if (!(fp = fopen(desktopFilePath, "w")))
      return;

   fwrite(desktopFile, 1, fileLen, fp);
   fclose(fp);

   snprintf(xdgMimeCommand,
         sizeof(xdgMimeCommand),
         "xdg-mime default discord-%s.desktop x-scheme-handler/discord-%s",
         applicationId,
         applicationId);
   if (system(xdgMimeCommand) < 0)
      fprintf(stderr, "Failed to register mime handler\n");
}

void Discord_RegisterSteamGame(const char *applicationId, const char *steamId)
{
   char command[256];
   size_t _len = strlcpy(command, "xdg-open steam://rungameid/", sizeof(command));
   strlcpy(command + _len, steamId, sizeof(command) - _len);
   Discord_Register(applicationId, command);
}
