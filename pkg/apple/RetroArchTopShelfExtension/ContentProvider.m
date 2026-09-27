//
//  ContentProvider.m
//  RetroArchTopShelfExtension
//
//  Created by Eric Warmenhoven on 2/17/24.
//  Copyright © 2024 RetroArch. All rights reserved.
//

#import "ContentProvider.h"

@implementation ContentProvider

/* On device the renderer can only read file URLs from the group container */
static NSURL *imageURLForItem(NSDictionary *item)
{
   NSString *name = item[@"imgfile"];
   NSFileManager *fm = [NSFileManager defaultManager];
   NSURL *fallback = item[@"img"] ? [NSURL URLWithString:item[@"img"]] : nil;
   NSURL *group, *src;
   if (![name length])
      return fallback;
   group = [fm containerURLForSecurityApplicationGroupIdentifier:kRetroArchAppGroup];
   if (!group)
      return fallback;
   src = [[group URLByAppendingPathComponent:@"Library/Caches/TopShelf" isDirectory:YES]
          URLByAppendingPathComponent:name];
   if (![fm fileExistsAtPath:[src path]])
      return fallback;
   return src;
}

static TVTopShelfSectionedItemImageShape imageShapeForItem(NSDictionary *item)
{
   NSString *shape = item[@"shape"];
   if ([shape isEqualToString:@"poster"])
      return TVTopShelfSectionedItemImageShapePoster;
   if ([shape isEqualToString:@"hdtv"])
      return TVTopShelfSectionedItemImageShapeHDTV;
   return TVTopShelfSectionedItemImageShapeSquare;
}

- (void)loadTopShelfContentWithCompletionHandler:(void (^) (id<TVTopShelfContent> content))completionHandler
{
   NSUserDefaults *ud = [[NSUserDefaults alloc] initWithSuiteName:kRetroArchAppGroup];

   NSDictionary *contentDict = [ud objectForKey:@"topshelf"];

   NSMutableArray *collections = [NSMutableArray arrayWithCapacity:[contentDict count]];
   for (NSString *key in [contentDict allKeys])
   {
      NSArray *contentArray = [contentDict objectForKey:key];
      NSMutableArray *items = [NSMutableArray arrayWithCapacity:[contentArray count]];

      for (NSDictionary *item in contentArray)
      {
         TVTopShelfSectionedItem *tsitem = [[TVTopShelfSectionedItem alloc] initWithIdentifier:item[@"id"]];
         tsitem.title = item[@"title"];
         NSURL *img = imageURLForItem(item);
         if (img)
            [tsitem setImageURL:img forTraits:(TVTopShelfItemImageTraitScreenScale1x | TVTopShelfItemImageTraitScreenScale2x)];
         [tsitem setImageShape:imageShapeForItem(item)];
         [tsitem setDisplayAction:[[TVTopShelfAction alloc] initWithURL:[NSURL URLWithString:item[@"play"]]]];
         [items addObject:tsitem];
      }

      TVTopShelfItemCollection<TVTopShelfSectionedItem *> *collection = [[TVTopShelfItemCollection alloc] initWithItems:items];
      collection.title = key;
      [collections addObject:collection];
   }
   TVTopShelfSectionedContent *content = [[TVTopShelfSectionedContent alloc] initWithSections:collections];
   completionHandler(content);
}

@end
