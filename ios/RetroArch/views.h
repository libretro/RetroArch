#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#include "conf/config_file.h"

@interface game_view : UIViewController
- (id)initWithGame:(NSString*)path;
@end

@interface module_list : UITableViewController

@end

@interface directory_list : UITableViewController
- (id)initWithPath:(NSString*)path;
@end


@interface SettingsSubList : UITableViewController
- (id)initWithSettings:(NSArray*)values title:(NSString*)title;
- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t*)config;
@end

@interface SettingsList : SettingsSubList
+ (void)refreshConfigFile;
@end
