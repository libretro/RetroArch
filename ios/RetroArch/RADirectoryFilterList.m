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

#import "browser.h"

@implementation RADirectoryFilterList
{
   NSString* _path;

   config_file_t* _filterList;
   unsigned _filterCount;
}

+ (RADirectoryFilterList*) directoryFilterListAtPath:(NSString*)path useExpression:(NSRegularExpression**)regex
{
   if (regex)
      *regex = nil;

   if (path && ra_ios_is_file([path stringByAppendingPathComponent:@".rafilter"]))
   {
      config_file_t* configFile = config_file_new([[path stringByAppendingPathComponent:@".rafilter"] UTF8String]);
      
      unsigned filterCount = 0;
      char* regexValue= 0;
      
      if (configFile && config_get_uint(configFile, "filter_count", &filterCount) && filterCount > 1)
         return [[RADirectoryFilterList alloc] initWithPath:path config:configFile];
      else if (regex && filterCount == 1 && config_get_string(configFile, "filter_1_regex", &regexValue))
         *regex = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithUTF8String:regexValue] options:0 error:nil];
      
      free(regexValue);
   }

   return nil;
}

- (id)initWithPath:(NSString*)path config:(config_file_t*)config
{
   self = [super initWithStyle:UITableViewStylePlain];

   _path = path;
   _filterList = config;

   if (!_filterList || !config_get_uint(_filterList, "filter_count", &_filterCount) || _filterCount == 0)
   {
      [RetroArch_iOS displayErrorMessage:@"No valid filters were found."];
   }

   [self setTitle: [path lastPathComponent]];
   
   return self;
}

- (id)initWithPath:(NSString*)path
{
   return [self initWithPath:path config:config_file_new([[path stringByAppendingPathComponent:@".rafilter"] UTF8String])];
}

- (void)dealloc
{
   if (_filterList)
      config_file_free(_filterList);
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (_filterList)
   {
      NSString* regexKey = [NSString stringWithFormat:@"filter_%d_regex", indexPath.row + 1];
   
      char* regex = 0;
      if (config_get_string(_filterList, [regexKey UTF8String], &regex))
      {
         NSRegularExpression* expr = [NSRegularExpression regularExpressionWithPattern:[NSString stringWithUTF8String:regex] options:0 error:nil];
         free(regex);
      
         [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:_path filter:expr]];
      }
   }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return _filterCount;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* nameKey = [NSString stringWithFormat:@"filter_%d_name", indexPath.row + 1];
   
   char* nameString = 0;
   if (_filterList && config_get_string(_filterList, [nameKey UTF8String], &nameString))
   {
      nameKey = [NSString stringWithUTF8String:nameString];
      free(nameString);
   }
   
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"filter"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"filter"];
   cell.textLabel.text = nameKey;
   
   return cell;
}

@end
