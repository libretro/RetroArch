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

#import "conf/config_file.h"
#import "browser.h"

@implementation RADirectoryGrid
{
   NSString* _path;
   NSArray* _list;
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
