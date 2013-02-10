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
@property (strong, nonatomic) NSString *overlay_path;
@property (strong, nonatomic) UINavigationController *navigator;
@property (strong, nonatomic) UIImage* file_icon;
@property (strong, nonatomic) UIImage* folder_icon;
@property (strong, nonatomic) UIBarButtonItem* settings_button;

@property const char* system_directory;

@end
