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
// Display name of the playlist/system this game belongs to
@property (nonatomic, strong, nullable) NSString *system;
@end

// Unified playlist management class
@interface RetroArchPlaylistManager : NSObject

// Convenience methods built on top of enumeration
+ (NSArray<RetroArchPlaylistGame *> *)getAllGames;
+ (nullable RetroArchPlaylistGame *)findGameByFilename:(NSString *)filename;

// Library sharing: serialize the whole library so another app can fetch it.
// Returns a JSON array of game dictionaries.
+ (nullable NSData *)exportAllGamesAsJSONData;
// Same, encoded as a URL-safe base64 string (base64url, no padding) suitable
// for embedding in a callback URL query parameter.
+ (nullable NSString *)exportAllGamesAsBase64URLString;

// History and favorites support for App Intents suggestions
+ (NSArray<RetroArchPlaylistGame *> *)getHistoryGames;
+ (NSArray<RetroArchPlaylistGame *> *)getFavoriteGames;

@end

NS_ASSUME_NONNULL_END
