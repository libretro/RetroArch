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

   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"dircell"];
   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"textcell"];
   [self.collectionView registerClass:[UICollectionViewCell class] forCellWithReuseIdentifier:@"imagecell"];
   
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
   UICollectionViewCell* cell = nil;
   
   if (path.isDirectory)
   {
      cell = [self.collectionView dequeueReusableCellWithReuseIdentifier:@"dircell" forIndexPath:indexPath];

      if (!cell.backgroundView)
      {
         cell.backgroundView = [[UIImageView alloc] initWithImage:[RetroArch_iOS get].folder_icon];
         ((UIImageView*)cell.backgroundView).contentMode = UIViewContentModeScaleAspectFit;
      }
   }
   else if (path.coverPath)
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
