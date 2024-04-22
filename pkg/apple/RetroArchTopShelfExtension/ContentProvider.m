//
//  ContentProvider.m
//  RetroArchTopShelfExtension
//
//  Created by Eric Warmenhoven on 2/17/24.
//  Copyright © 2024 RetroArch. All rights reserved.
//

#import "ContentProvider.h"

@implementation ContentProvider

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
         [tsitem setImageURL:[NSURL URLWithString:item[@"img"]] forTraits:(TVTopShelfItemImageTraitScreenScale1x | TVTopShelfItemImageTraitScreenScale2x)];
         [tsitem setImageShape:TVTopShelfSectionedItemImageShapeSquare];
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
