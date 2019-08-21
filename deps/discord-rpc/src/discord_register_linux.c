#include "discord_rpc.h"
#include "discord_register.h"
#include <stdio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <boolean.h>

static bool Mkdir(const char* path)
{
   int result = mkdir(path, 0755);
   if (result == 0)
      return true;
   if (errno == EEXIST)
      return true;
   return false;
}

/* we want to register games so we can run them from 
 * Discord client as discord-<appid>:// */
void Discord_Register(const char* applicationId, const char* command)
{
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

   snprintf(desktopFilename, sizeof(desktopFilename), "/discord-%s.desktop", applicationId);

   snprintf(desktopFilePath, sizeof(desktopFilePath), "%s/.local", home);
   if (!Mkdir(desktopFilePath))
      return;
   strcat(desktopFilePath, "/share");
   if (!Mkdir(desktopFilePath))
      return;
   strcat(desktopFilePath, "/applications");
   if (!Mkdir(desktopFilePath))
      return;
   strcat(desktopFilePath, desktopFilename);

   fp = fopen(desktopFilePath, "w");
   if (!fp)
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

void Discord_RegisterSteamGame(
      const char* applicationId,
      const char* steamId)
{
   char command[256];
   sprintf(command, "xdg-open steam://rungameid/%s", steamId);
   Discord_Register(applicationId, command);
}
