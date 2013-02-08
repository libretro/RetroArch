//
//  browser.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "browser.h"
#import "dirlist.h"
@implementation browser

- (void)viewDidLoad
{
   [super viewDidLoad];
   
   [self pushViewController:[[[dirlist_view alloc] init] load_path:"/" ] animated:NO];
}


@end
