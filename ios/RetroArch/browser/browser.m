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
#import "conf/config_file.h"

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
      
      if (ra_ios_is_file(result.coverPath))
         result.hasCover = true;
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

   return [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
}

@implementation RADirectoryList
{
   NSString* _path;
   NSArray* _list;
}

+ (id)directoryListOrGridWithPath:(NSString*)path
{
   path = ra_ios_check_path(path);

   if ([UICollectionViewController instancesRespondToSelector:@selector(initWithCollectionViewLayout:)])
   {
      NSString* coverDir = [path stringByAppendingPathComponent:@".coverart"];
      if (ra_ios_is_directory(coverDir))
         return [[RADirectoryGrid alloc] initWithPath:path];
   }

   return [[RADirectoryList alloc] initWithPath:path];
}

- (id)initWithPath:(NSString*)path
{
   self = [super initWithStyle:UITableViewStylePlain];

   _path = path;
   _list = ra_ios_list_directory(_path);

   [self setTitle: [_path lastPathComponent]];
   
   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];

   if(path.isDirectory)
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListOrGridWithPath:path.path] animated:YES];
   else
      [[RetroArch_iOS get] pushViewController:[[RAModuleList alloc] initWithGame:path.path] animated:YES];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [_list count];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   cell.textLabel.text = [path.path lastPathComponent];
   cell.accessoryType = (path.isDirectory) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   cell.imageView.image = [UIImage imageNamed:(path.isDirectory) ? @"ic_dir" : @"ic_file"];
   return cell;
}

@end

@implementation RADirectoryGrid
{
   NSString* _path;
   NSArray* _list;
   
   UIPopoverController* _imageSelect;
   uint32_t _imageSelectIndex;
}

- (id)initWithPath:(NSString*)path
{
   _path = path;
   _list = ra_ios_list_directory(_path);
   
   [self setTitle: [_path lastPathComponent]];

   unsigned tileWidth = 100;
   unsigned tileHeight = 100;

   config_file_t* config = config_file_new([[path stringByAppendingPathComponent:@".raconfig"] UTF8String]);
   if (config)
   {
      config_get_uint(config, "cover_width", &tileWidth);
      config_get_uint(config, "cover_height", &tileHeight);
      config_file_free(config);
   }

   // Init collection view
   UICollectionViewFlowLayout* layout = [UICollectionViewFlowLayout new];
   layout.itemSize = CGSizeMake(tileWidth, tileHeight);
   self = [super initWithCollectionViewLayout:layout];

   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"dircell"];
   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"textcell"];
   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"imagecell"];

   UILongPressGestureRecognizer* gesture = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longPress:)];
   [self.collectionView addGestureRecognizer:gesture];

   return self;
}

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView
{
   return 1;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
   return [_list count];
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];

   if(path.isDirectory)
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListOrGridWithPath:path.path] animated:YES];
   else
      [[RetroArch_iOS get] pushViewController:[[RAModuleList alloc] initWithGame:path.path] animated:YES];
}

- (void)longPress:(UILongPressGestureRecognizer*)gesture
{
   if (gesture.state == UIGestureRecognizerStateRecognized)
   {
      CGPoint location = [gesture locationInView:self.collectionView];
      NSIndexPath* indexPath = [self.collectionView indexPathForItemAtPoint:location];
   
      _imageSelectIndex = indexPath.row;
  
      UICollectionViewCell* cell = [self.collectionView cellForItemAtIndexPath:indexPath];
      if (cell)
      {
         UIImagePickerController* pick = [UIImagePickerController new];
         pick.delegate = self;
         pick.sourceType = UIImagePickerControllerSourceTypeSavedPhotosAlbum;
         
         if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
         {
            _imageSelect = [[UIPopoverController alloc] initWithContentViewController:pick];
            [_imageSelect presentPopoverFromRect:cell.frame inView:self.collectionView permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
         }
         else
            [self presentViewController:pick animated:YES completion:nil];
      }
   }
}

- (void)imagePickerController:(UIImagePickerController*)picker didFinishPickingMediaWithInfo:(NSDictionary*)info
{
   [self imagePickerControllerDidCancel:picker];
   
   // Copy image
   RADirectoryItem* path = _list[_imageSelectIndex];
   
   UIImage* image = [info valueForKey:@"UIImagePickerControllerOriginalImage"];
   [UIImagePNGRepresentation(image) writeToFile:path.coverPath atomically:YES];
   path.hasCover = true;
   
   [self.collectionView reloadData];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker
{
   if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
   {
      [_imageSelect dismissPopoverAnimated:YES];
      _imageSelect = nil;
   }
   else
      [picker dismissViewControllerAnimated:YES completion:nil];
}


- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];
   UICollectionViewCell* cell = nil;
   
   if (path.isDirectory)
   {
      cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"dircell" forIndexPath:indexPath];

      if (!cell.backgroundView)
      {
         cell.backgroundView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"ic_dir"]];
         ((UIImageView*)cell.backgroundView).contentMode = UIViewContentModeScaleAspectFit;
      }
   }
   else if (path.hasCover)
   {
      cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"imagecell" forIndexPath:indexPath];
      
      if (!cell.backgroundView)
      {
         cell.backgroundView = [UIImageView new];
         ((UIImageView*)cell.backgroundView).contentMode = UIViewContentModeScaleAspectFit;         
      }
      
      ((UIImageView*)cell.backgroundView).image = [UIImage imageWithContentsOfFile:path.coverPath];
   }
   else
   {
      cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"textcell" forIndexPath:indexPath];
      
      if (!cell.backgroundView)
      {
         cell.backgroundView = [UILabel new];
         ((UILabel*)cell.backgroundView).numberOfLines = 0;
         ((UILabel*)cell.backgroundView).textAlignment = NSTextAlignmentCenter;
      }
      
      ((UILabel*)cell.backgroundView).text = [path.path lastPathComponent];
   }
   
   return cell;
}

@end
