extern BOOL ra_ios_is_directory(NSString* path);
extern BOOL ra_ios_is_file(NSString* path);

@interface RADirectoryGrid : UICollectionViewController
- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex;
@end

@interface RADirectoryFilterList : UITableViewController
- (id)initWithPath:(NSString*)path;
@end
