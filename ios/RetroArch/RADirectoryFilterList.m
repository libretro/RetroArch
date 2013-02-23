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

#import "RAConfig.h"
#import "browser.h"

@implementation RADirectoryFilterList
{
   NSString* _path;

   RAConfig* _filterList;
   unsigned _filterCount;
}

+ (RADirectoryFilterList*) directoryFilterListAtPath:(NSString*)path useExpression:(NSRegularExpression**)regex
{
   if (regex)
      *regex = nil;

   if (path && ra_ios_is_file([path stringByAppendingPathComponent:@".rafilter"]))
   {
      RAConfig* configFile = [[RAConfig alloc] initWithPath:[path stringByAppendingPathComponent:@".rafilter"]];
      unsigned filterCount = [configFile getUintNamed:@"filter_count" withDefault:0];
      
      if (filterCount > 1)
         return [[RADirectoryFilterList alloc] initWithPath:path config:configFile];
      
      if (regex && filterCount == 1)
      {
         NSString* expr = [configFile getStringNamed:@"filter_1_regex" withDefault:@".*"];
         *regex = [NSRegularExpression regularExpressionWithPattern:expr options:0 error:nil];
      }
   }

   return nil;
}

- (id)initWithPath:(NSString*)path config:(RAConfig*)config
{
   self = [super initWithStyle:UITableViewStylePlain];

   _path = path;
   _filterList = config;
   _filterCount = [_filterList getUintNamed:@"filter_count" withDefault:0];

   if (_filterCount == 0)
      [RetroArch_iOS displayErrorMessage:@"No valid filters were found."];

   [self setTitle: [path lastPathComponent]];
   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* regex = [NSString stringWithFormat:@"filter_%d_regex", indexPath.row + 1];
   regex = [_filterList getStringNamed:regex withDefault:@".*"];

   NSRegularExpression* expr = [NSRegularExpression regularExpressionWithPattern:regex options:0 error:nil];

   [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:_path filter:expr] isGame:NO];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return _filterCount ? _filterCount : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* name = [NSString stringWithFormat:@"filter_%d_name", indexPath.row + 1];
   name = [_filterList getStringNamed:name withDefault:@"BAD NAME"];
   
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"filter"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"filter"];
   cell.textLabel.text = name;
   
   return cell;
}

@end
