/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dirent.h>
#include <sys/stat.h>
#import "browser.h"

@implementation RADirectoryItem
+ (RADirectoryItem*)directoryItemFromPath:(const char*)thePath checkForCovers:(BOOL)checkCovers
{
   RADirectoryItem* result = [RADirectoryItem new];
   result.path = [NSString stringWithUTF8String:thePath];

   struct stat statbuf;
   if (stat(thePath, &statbuf) == 0)
      result.isDirectory = S_ISDIR(statbuf.st_mode);

   if (checkCovers && !result.isDirectory)
   {
      result.coverPath = [NSString stringWithFormat:@"%@/.coverart/%@.png", [result.path stringByDeletingLastPathComponent], [[result.path lastPathComponent] stringByDeletingPathExtension]];
      
      if (!ra_ios_is_file(result.coverPath))
         result.coverPath = nil;
   }
   
   return result;
}
@end


BOOL ra_ios_is_file(NSString* path)
{
   return [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:nil];
}

BOOL ra_ios_is_directory(NSString* path)
{
   BOOL result = NO;
   [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&result];
   return result;
}

NSArray* ra_ios_list_directory(NSString* path)
{
   NSMutableArray* result = [NSMutableArray array];

   // Build list
   char* cpath = malloc([path length] + sizeof(struct dirent));
   sprintf(cpath, "%s/", [path UTF8String]);
   size_t cpath_end = strlen(cpath);

   DIR* dir = opendir(cpath);
   if (!dir)
      return result;
   
   for(struct dirent* item = readdir(dir); item; item = readdir(dir))
   {
      if (strncmp(item->d_name, ".", 1) == 0)
         continue;
      
      cpath[cpath_end] = 0;
      strcat(cpath, item->d_name);
      
      [result addObject:[RADirectoryItem directoryItemFromPath:cpath checkForCovers:YES]];
   }
   
   closedir(dir);
   free(cpath);
   
   // Sort
   [result sortUsingComparator:^(RADirectoryItem* left, RADirectoryItem* right)
   {
      return (left.isDirectory != right.isDirectory) ?
               (left.isDirectory ? -1 : 1) :
               ([left.path caseInsensitiveCompare:right.path]);
   }];
     
   return result;
}

NSString* ra_ios_check_path(NSString* path)
{
   if (path && ra_ios_is_directory(path))
      return path;

   if (path)
      [RetroArch_iOS displayErrorMessage:@"Browsed path is not a directory."];

   if (ra_ios_is_directory(@"/var/mobile/RetroArchGames"))  return @"/var/mobile/RetroArchGames";
   else if (ra_ios_is_directory(@"/var/mobile"))            return @"/var/mobile";
   else                                                     return @"/";
}
