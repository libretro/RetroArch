//
//  ContentProvider.h
//  RetroArchTopShelfExtension
//
//  Created by Eric Warmenhoven on 2/17/24.
//  Copyright © 2024 RetroArch. All rights reserved.
//

#import <TVServices/TVServices.h>

#ifndef kRetroArchAppGroup
#define kRetroArchAppGroup @"group.com.libretro.dist.tvos.RetroArchAppGroup"
#endif

API_AVAILABLE(tvos(13.0))
@interface ContentProvider : TVTopShelfContentProvider


@end

