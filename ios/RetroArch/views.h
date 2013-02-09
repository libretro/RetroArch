#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

@interface game_view : UIViewController

@end

@interface module_list : UITableViewController

@end

@interface directory_list : UITableViewController
- (id)initWithPath:(NSString*)path;
@end

@interface settings_list : UITableViewController
@end