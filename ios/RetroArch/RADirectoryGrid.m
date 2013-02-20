//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "browser.h"

@implementation RADirectoryGrid
{
   NSString* _path;
   NSArray* _list;
   
   UIImage* _templateImage;
}

- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex
{
   UICollectionViewFlowLayout* layout = [UICollectionViewFlowLayout new];
   layout.itemSize = CGSizeMake(175, 248);
   self = [super initWithCollectionViewLayout:layout];

   if (path == nil)
   {
      if (ra_ios_is_directory(@"/var/mobile/RetroArchGames"))  path = @"/var/mobile/RetroArchGames";
      else if (ra_ios_is_directory(@"/var/mobile"))            path = @"/var/mobile";
      else                                                     path = @"/";
   }

   _path = path;

   NSString* templateName = [NSString stringWithFormat:@"%@/.coverart/template.png", _path];
   _templateImage = [UIImage imageWithContentsOfFile:templateName];
   
   if (!_templateImage)
   {
      [RetroArch_iOS displayErrorMessage:@"Coverart template.png is missing."];
      _templateImage = [RetroArch_iOS get].file_icon;
   }

   _list = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:_path error:nil];
   _list = [_path stringsByAppendingPaths:_list];
   
   if (regex)
   {
      _list = [_list filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^(id object, NSDictionary* bindings)
      {
         if (ra_ios_is_directory(object))
            return YES;
         
         return (BOOL)([regex numberOfMatchesInString:[object lastPathComponent] options:0 range:NSMakeRange(0, [[object lastPathComponent] length])] != 0);
      }]];
   }
   
   _list = [_list sortedArrayUsingComparator:^(id left, id right)
   {
      const BOOL left_is_dir = ra_ios_is_directory((NSString*)left);
      const BOOL right_is_dir = ra_ios_is_directory((NSString*)right);
      
      return (left_is_dir != right_is_dir) ?
               (left_is_dir ? -1 : 1) :
               ([left caseInsensitiveCompare:right]);
   }];

   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;
   [self setTitle: [_path lastPathComponent]];
   
   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"filecell"];
   
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
   NSString* path = [_list objectAtIndex: indexPath.row];

   if(ra_ios_is_directory(path))
   {
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:path]];
   }
   else
   {
      [[RetroArch_iOS get] runGame:path];
   }
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* path = [_list objectAtIndex: indexPath.row];
   BOOL isdir = ra_ios_is_directory(path);
   
   UICollectionViewCell* cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"filecell" forIndexPath:indexPath];
   if (isdir)
      cell.backgroundView = [[UIImageView alloc] initWithImage:[RetroArch_iOS get].folder_icon];
   else
   {
      NSString* img = [NSString stringWithFormat:@"%@/.coverart/%@.png", _path, [[path lastPathComponent] stringByDeletingPathExtension]];
      if (ra_ios_is_file(img))
         cell.backgroundView = [[UIImageView alloc] initWithImage:[UIImage imageWithContentsOfFile:img]];
      else
         cell.backgroundView = [[UIImageView alloc] initWithImage:_templateImage];
   }
   
   return cell;
}

@end
