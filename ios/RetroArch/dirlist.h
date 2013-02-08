//
//  UIViewController_m.h
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface dirlist_view : UIViewController <UITableViewDelegate, UITableViewDataSource>
- (id)load_path:(const char*)directory;
@end
