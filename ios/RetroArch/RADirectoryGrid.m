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

@implementation RADirectoryGrid
{
   NSString* _path;
   NSArray* _list;
   RAConfig* _config;
}

- (id)initWithPath:(NSString*)path config:(RAConfig*)config
{
   _path = path;
   _config = config;
   _list = ra_ios_list_directory(_path);
   
   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;
   [self setTitle: [_path lastPathComponent]];

   // Init collection view
   UICollectionViewFlowLayout* layout = [UICollectionViewFlowLayout new];
   layout.itemSize = CGSizeMake([config getUintNamed:@"cover_width" withDefault:100], [config getUintNamed:@"cover_height" withDefault:100]);
   self = [super initWithCollectionViewLayout:layout];

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
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListOrGridWithPath:path.path] isGame:NO];
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
