#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#include "conf/config_file.h"

@interface RAGameView : UIViewController
- (id)initWithGame:(NSString*)path;\
- (void)pause;
- (void)resume;
- (void)exit;
@end

@interface RAModuleList : UITableViewController

@end

@interface RADirectoryList : UITableViewController
- (id)initWithPath:(NSString*)path;
@end


@interface RASettingsSubList : UITableViewController
- (id)initWithSettings:(NSArray*)values title:(NSString*)title;
- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t*)config;
@end

@interface RASettingsList : RASettingsSubList
+ (void)refreshConfigFile;
@end
