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
   _path = path ? path : ra_ios_get_browser_root();

   // Load template image
   NSString* templateName = [NSString stringWithFormat:@"%@/.coverart/template.png", _path];
   _templateImage = [UIImage imageWithContentsOfFile:templateName];
   
   if (!_templateImage)
   {
      [RetroArch_iOS displayErrorMessage:@"Coverart template.png is missing."];
      _templateImage = [RetroArch_iOS get].file_icon;
   }

   //
   UICollectionViewFlowLayout* layout = [UICollectionViewFlowLayout new];
   layout.itemSize = _templateImage.size;
   self = [super initWithCollectionViewLayout:layout];

   _list = ra_ios_list_directory(_path, regex);

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
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];

   if(path.isDirectory)
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:path.path]];
   else
      [[RetroArch_iOS get] runGame:path.path];
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = [_list objectAtIndex: indexPath.row];
   
   UICollectionViewCell* cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"filecell" forIndexPath:indexPath];
   if (path.isDirectory)
      cell.backgroundView = [[UIImageView alloc] initWithImage:[RetroArch_iOS get].folder_icon];
   else
   {
      NSString* img = [NSString stringWithFormat:@"%@/.coverart/%@.png", _path, [[path.path lastPathComponent] stringByDeletingPathExtension]];
      if (ra_ios_is_file(img))
         cell.backgroundView = [[UIImageView alloc] initWithImage:[UIImage imageWithContentsOfFile:img]];
      else
         cell.backgroundView = [[UIImageView alloc] initWithImage:_templateImage];
   }
   
   return cell;
}

@end
