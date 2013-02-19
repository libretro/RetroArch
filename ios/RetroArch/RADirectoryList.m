//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

static BOOL is_file(NSString* path)
{
   return [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:nil];
}

static BOOL is_directory(NSString* path)
{
   BOOL result = NO;
   [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&result];
   return result;
}

@implementation RADirectoryList
{
   NSString* _path;
   NSArray* _list;
}

+ (id)directoryListWithPath:(NSString*)path
{
   if (path && !is_directory(path))
   {
      [RetroArch_iOS displayErrorMessage:@"Browsed path is not a directory."];
      path = nil;
   }
   
   if (path && is_file([path stringByAppendingPathComponent:@".rafilter"]))
      return [[RADirectoryFilterList alloc] initWithPath:path];
   else
      return [[RADirectoryList alloc] initWithPath:path filter:nil];
   
}

- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex
{
   self = [super initWithStyle:UITableViewStylePlain];

   if (path == nil)
   {
      if (is_directory(@"/var/mobile/RetroArchGames"))    path = @"/var/mobile/RetroArchGames";
      else if (is_directory(@"/var/mobile"))              path = @"/var/mobile";
      else                                                path = @"/";
   }

   _path = path;

   _list = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:_path error:nil];
   _list = [_path stringsByAppendingPaths:_list];
   
   if (regex)
   {
      _list = [_list filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^(id object, NSDictionary* bindings)
      {
         if (is_directory(object))
            return YES;
         
         return (BOOL)([regex numberOfMatchesInString:[object lastPathComponent] options:0 range:NSMakeRange(0, [[object lastPathComponent] length])] != 0);
      }]];
   }
   
   _list = [_list sortedArrayUsingComparator:^(id left, id right)
   {
      const BOOL left_is_dir = is_directory((NSString*)left);
      const BOOL right_is_dir = is_directory((NSString*)right);
      
      return (left_is_dir != right_is_dir) ?
               (left_is_dir ? -1 : 1) :
               ([left caseInsensitiveCompare:right]);
   }];

   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;
   [self setTitle: [_path lastPathComponent]];
   
   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* path = [_list objectAtIndex: indexPath.row];

   if(is_directory(path))
   {
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:path]];
   }
   else
   {
      [[RetroArch_iOS get] runGame:path];
   }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [_list count];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* path = [_list objectAtIndex: indexPath.row];
   BOOL isdir = is_directory(path);

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   cell.textLabel.text = [path lastPathComponent];
   cell.accessoryType = (isdir) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   cell.imageView.image = (isdir) ? [RetroArch_iOS get].folder_icon : [RetroArch_iOS get].file_icon;
   return cell;
}

@end
