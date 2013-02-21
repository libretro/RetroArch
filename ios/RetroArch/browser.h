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

extern BOOL ra_ios_is_directory(NSString* path);
extern BOOL ra_ios_is_file(NSString* path);
extern NSArray* ra_ios_list_directory(NSString* path, NSRegularExpression* regex);
extern NSString* ra_ios_get_browser_root();

@interface RADirectoryGrid : UICollectionViewController
- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex;
@end

@interface RADirectoryFilterList : UITableViewController
// Check path to see if a directory filter list is needed.
// If one is not needed useExpression will be set to a default expression to use.
+ (RADirectoryFilterList*) directoryFilterListAtPath:(NSString*)path useExpression:(NSRegularExpression**)regex;

- (id)initWithPath:(NSString*)path;
@end
