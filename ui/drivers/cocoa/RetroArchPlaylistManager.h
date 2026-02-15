//
//  RetroArchPlaylistManager.h
//  RetroArch
//
//  Unified playlist management for iOS and tvOS
//

#import <Foundation/Foundation.h>
#include "../../../playlist.h"

NS_ASSUME_NONNULL_BEGIN

// Model object representing a game in a playlist
@interface RetroArchPlaylistGame : NSObject
@property (nonatomic, strong) NSString *gameId;
@property (nonatomic, strong) NSString *title;
@property (nonatomic, strong) NSString *filename;
@property (nonatomic, strong) NSString *fullPath;
@property (nonatomic, strong, nullable) NSString *corePath;
@property (nonatomic, strong, nullable) NSString *coreName;
@end

// Unified playlist management class
@interface RetroArchPlaylistManager : NSObject

// Convenience methods built on top of enumeration
+ (NSArray<RetroArchPlaylistGame *> *)getAllGames;
+ (nullable RetroArchPlaylistGame *)findGameByFilename:(NSString *)filename;

// History and favorites support for App Intents suggestions
+ (NSArray<RetroArchPlaylistGame *> *)getHistoryGames;
+ (NSArray<RetroArchPlaylistGame *> *)getFavoriteGames;

@end

NS_ASSUME_NONNULL_END
