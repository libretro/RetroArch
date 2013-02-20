//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "browser.h"

@implementation RADirectoryFilterList
{
   NSString* _path;

   config_file_t* _filterList;
   unsigned _filterCount;
}

- (id)initWithPath:(NSString*)path
{
   self = [super initWithStyle:UITableViewStylePlain];
   
   _path = path;
   _filterList = config_file_new([[path stringByAppendingPathComponent:@".rafilter"] UTF8String]);
   
   if (!_filterList || !config_get_uint(_filterList, "filter_count", &_filterCount) || _filterCount == 0)
   {
      [RetroArch_iOS displayErrorMessage:@"No valid filters were found."];
   }
   
   [self setTitle: [path lastPathComponent]];
  
   return self;
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
