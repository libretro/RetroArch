#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

@interface game_view : UIViewController

@end

@interface module_list : UIViewController<UITableViewDelegate, UITableViewDataSource>

@end

@interface directory_list : UIViewController <UITableViewDelegate, UITableViewDataSource>
- (id)load_path:(const char*)directory;
@end
