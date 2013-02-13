//
//  AppDelegate.h
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <UIKit/UIKit.h>

extern NSString* const GSEventKeyDownNotification;
extern NSString* const GSEventKeyUpNotification;

@interface RetroArch_iOS : UIResponder <UIApplicationDelegate>

+ (RetroArch_iOS*)get;
- (void)runGame:(NSString*)path;
- (void)gameHasExited;

- (void)pushViewController:(UIViewController*)theView;
- (void)popViewController;

@property (strong, nonatomic) NSString* system_directory;
@property (strong, nonatomic) NSString* config_file_path;

@property (strong, nonatomic) NSString* module_path;
@property (strong, nonatomic) UIImage* file_icon;
@property (strong, nonatomic) UIImage* folder_icon;
@property (strong, nonatomic) UIBarButtonItem* settings_button;

@end
