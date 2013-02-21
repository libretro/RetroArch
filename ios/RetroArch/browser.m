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

NSArray* ra_ios_list_directory(NSString* path, NSRegularExpression* regex)
{
   NSArray* result = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
   result = [path stringsByAppendingPaths:result];
   
   if (regex)
   {
      result = [result filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^(id object, NSDictionary* bindings)
      {
         if (ra_ios_is_directory(object))
            return YES;
         
         return (BOOL)([regex numberOfMatchesInString:[object lastPathComponent] options:0 range:NSMakeRange(0, [[object lastPathComponent] length])] != 0);
      }]];
   }
   
   result = [result sortedArrayUsingComparator:^(id left, id right)
   {
      const BOOL left_is_dir = ra_ios_is_directory((NSString*)left);
      const BOOL right_is_dir = ra_ios_is_directory((NSString*)right);
      
      return (left_is_dir != right_is_dir) ?
               (left_is_dir ? -1 : 1) :
               ([left caseInsensitiveCompare:right]);
   }];
   
   return result;
}

NSString* ra_ios_get_browser_root()
{
   if (ra_ios_is_directory(@"/var/mobile/RetroArchGames"))  return @"/var/mobile/RetroArchGames";
   else if (ra_ios_is_directory(@"/var/mobile"))            return @"/var/mobile";
   else                                                     return @"/";
}
