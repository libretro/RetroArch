//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

@interface button_getter : NSObject<UIAlertViewDelegate>
- (id)initWithSetting:(NSMutableDictionary*)setting fromTable:(UITableView*)table;
@end

@interface enumeration_list : UITableViewController
- (id)initWithSetting:(NSMutableDictionary*)setting fromTable:(UITableView*)table;
@end
