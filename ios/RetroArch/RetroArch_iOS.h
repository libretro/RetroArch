//
//  AppDelegate.h
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface RetroArch_iOS : UIResponder <UIApplicationDelegate>

+ (RetroArch_iOS*)get;

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) NSString *module_path;
@property (strong, nonatomic) UINavigationController *navigator;
@property (strong, nonatomic) NSString *nib_name;

@end
