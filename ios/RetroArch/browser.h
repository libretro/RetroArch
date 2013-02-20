extern BOOL ra_ios_is_directory(NSString* path);
extern BOOL ra_ios_is_file(NSString* path);

@interface RADirectoryGrid : UICollectionViewController
- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex;
@end

@interface RADirectoryFilterList : UITableViewController
// Check path to see if a directory filter list is needed.
// If one is not needed useExpression will be set to a default expression to use.
+ (RADirectoryFilterList*) directoryFilterListAtPath:(NSString*)path useExpression:(NSRegularExpression**)regex;

- (id)initWithPath:(NSString*)path;
@end
