//
//  ViewController.h
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

@interface game_view : UIViewController
- (void)load_game:(const char*)file_name;
@end
