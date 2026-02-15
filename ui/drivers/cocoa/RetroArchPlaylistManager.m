//
//  RetroArchPlaylistManager.m
//  RetroArch
//
//  Unified playlist management for iOS and tvOS
//

#import "RetroArchPlaylistManager.h"
#include "../../../playlist.h"
#include "../../../paths.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../verbosity.h"
#include "../../../file_path_special.h"
#include "../../../defaults.h"
#include "../../../libretro-common/include/lists/dir_list.h"
#include "../../../libretro-common/include/file/file_path.h"
#include "../../../libretro-common/include/string/stdstring.h"

NS_ASSUME_NONNULL_BEGIN

// Block type for enumerating playlist entries
typedef void (^PlaylistEntryBlock)(const struct playlist_entry *entry, playlist_t *playlist, NSString *playlistName, uint32_t index);

@implementation RetroArchPlaylistGame
@end

@implementation RetroArchPlaylistManager

+ (NSArray<NSString *> * _Nonnull)getPlaylistNames
{
    NSMutableArray<NSString *> *playlistNames = [[NSMutableArray alloc] init];

    // Get RetroArch's playlist directory
    char playlist_dir[PATH_MAX_LENGTH];
    settings_t *settings = config_get_ptr();

    if (!settings) {
        RARCH_LOG("RetroArch not initialized yet, cannot access playlists\n");
        return playlistNames;
    }

    fill_pathname_join_special(playlist_dir,
                               settings->paths.directory_playlist,
                               "",
                               sizeof(playlist_dir));

    // Use dir_list to discover actual playlist files (like menu system does)
    struct string_list str_list = {0};
    if (!dir_list_initialize(&str_list, playlist_dir, NULL, true,
                             settings->bools.show_hidden_files, true, false)) {
        RARCH_LOG("Could not scan playlist directory: %s\n", playlist_dir);
        return playlistNames;
    }

    // Sort playlists (same as menu system)
    dir_list_sort_ignore_ext(&str_list, true);

    // Process each file, filtering for valid playlists
    for (size_t i = 0; i < str_list.size; i++) {
        const char *path = str_list.elems[i].data;
        const char *playlist_file = path_basename(path);

        if (string_is_empty(playlist_file))
            continue;

        // Only include .lpl files (same logic as menu_displaylist_parse_playlists)
        if (!string_is_equal_noncase(path_get_extension(playlist_file), "lpl"))
            continue;

        // Exclude history and favorites files (same logic as menu system)
        if (string_ends_with_size(path, "_history.lpl",
                                  strlen(path), STRLEN_CONST("_history.lpl")) ||
            string_is_equal(playlist_file, FILE_PATH_CONTENT_FAVORITES))
            continue;

        // Add valid playlist to our list
        [playlistNames addObject:[NSString stringWithUTF8String:playlist_file]];
    }

    dir_list_deinitialize(&str_list);

    RARCH_LOG("App Intents: Discovered %lu playlists in %s\n",
              (unsigned long)playlistNames.count, playlist_dir);

    return playlistNames;
}

+ (void)enumerateAllPlaylistEntries:(PlaylistEntryBlock _Nonnull)block
{
    // Get RetroArch's playlist directory
    char playlist_dir[PATH_MAX_LENGTH];
    settings_t *settings = config_get_ptr();

    if (!settings) {
        RARCH_LOG("RetroArch not initialized yet, cannot enumerate playlists\n");
        return;
    }

    fill_pathname_join_special(playlist_dir,
                               settings->paths.directory_playlist,
                               "",
                               sizeof(playlist_dir));

    NSArray<NSString *> *playlistNames = [self getPlaylistNames];

    for (NSString *playlistName in playlistNames) {
        char playlist_path[PATH_MAX_LENGTH];
        fill_pathname_join_special(playlist_path,
                                   playlist_dir,
                                   [playlistName UTF8String],
                                   sizeof(playlist_path));

        // Try to load the playlist
        playlist_config_t config;
        config.capacity = COLLECTION_SIZE;
        config.old_format = false;
        config.compress = false;
        config.fuzzy_archive_match = false;
        config.autofix_paths = false;
        strlcpy(config.path, playlist_path, sizeof(config.path));
        strlcpy(config.base_content_directory, "", sizeof(config.base_content_directory));

        playlist_t *playlist = playlist_init(&config);
        if (!playlist)
            continue;

        uint32_t playlist_size = playlist_get_size(playlist);

        // Enumerate all entries in this playlist
        for (uint32_t i = 0; i < playlist_size; i++) {
            const struct playlist_entry *entry = NULL;
            playlist_get_index(playlist, i, &entry);

            if (entry && entry->path && entry->label) {
                block(entry, playlist, playlistName, i);
            }
        }

        playlist_free(playlist);
    }
}

+ (NSArray<RetroArchPlaylistGame *> * _Nonnull)getAllGames
{
    NSMutableArray<RetroArchPlaylistGame *> *games = [[NSMutableArray alloc] init];

    // Check if RetroArch is properly initialized
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    if (!runloop_st || !(runloop_st->flags & RUNLOOP_FLAG_IS_INITED)) {
        RARCH_LOG("RetroArch not fully initialized, cannot access playlists\n");
        return games;
    }

    // Double-check that config is available
    settings_t *settings = config_get_ptr();
    if (!settings) {
        RARCH_LOG("RetroArch configuration not available, cannot access playlists\n");
        return games;
    }

    [self enumerateAllPlaylistEntries:^(const struct playlist_entry *entry, playlist_t *playlist, NSString *playlistName, uint32_t index) {
        RetroArchPlaylistGame *game = [[RetroArchPlaylistGame alloc] init];

        // Create a unique ID from path and playlist
        game.gameId = [NSString stringWithFormat:@"%@:%@", playlistName, @(index)];
        game.title = [NSString stringWithUTF8String:entry->label];
        game.fullPath = [NSString stringWithUTF8String:entry->path];

        // Extract filename from path
        const char *filename = path_basename(entry->path);
        game.filename = [NSString stringWithUTF8String:filename];

        if (!string_is_empty(entry->core_path) && !string_is_equal(entry->core_path, FILE_PATH_DETECT))
            game.corePath = [NSString stringWithUTF8String:entry->core_path];
        else if (!string_is_empty(playlist_get_default_core_path(playlist)))
            game.corePath = [NSString stringWithUTF8String:playlist_get_default_core_path(playlist)];
        if (!string_is_empty(entry->core_name) && !string_is_equal(entry->core_name, FILE_PATH_DETECT))
            game.coreName = [NSString stringWithUTF8String:entry->core_name];
        else if (!string_is_empty(playlist_get_default_core_name(playlist)))
            game.coreName = [NSString stringWithUTF8String:playlist_get_default_core_name(playlist)];

        [games addObject:game];
    }];

    RARCH_LOG("App Intents: Found %lu games across all playlists\n", (unsigned long)games.count);
    return games;
}

+ (nullable RetroArchPlaylistGame *)findGameByFilename:(NSString * _Nonnull)filename
{
    // Check if RetroArch is properly initialized
    runloop_state_t *runloop_st = runloop_state_get_ptr();
    if (!runloop_st || !(runloop_st->flags & RUNLOOP_FLAG_IS_INITED)) {
        RARCH_LOG("RetroArch not fully initialized, cannot find games\n");
        return nil;
    }

    NSArray<RetroArchPlaylistGame *> *allGames = [self getAllGames];

    for (RetroArchPlaylistGame *game in allGames) {
        if ([game.filename isEqualToString:filename]) {
            return game;
        }
    }

    return nil;
}

// Private helper method to extract games from a playlist
+ (NSArray<RetroArchPlaylistGame *> *)getGamesFromPlaylist:(playlist_t *)playlist
                                               withIdPrefix:(NSString *)idPrefix
                                                   maxGames:(uint32_t)maxGames
{
    NSMutableArray<RetroArchPlaylistGame *> *games = [[NSMutableArray alloc] init];

    if (!playlist) {
        return games;
    }

    uint32_t playlist_size = playlist_get_size(playlist);
    uint32_t games_to_get = MIN(maxGames, playlist_size);

    for (uint32_t i = 0; i < games_to_get; i++) {
        const struct playlist_entry *entry = NULL;
        playlist_get_index(playlist, i, &entry);

        if (entry && entry->path && entry->label) {
            RetroArchPlaylistGame *game = [[RetroArchPlaylistGame alloc] init];

            game.gameId = [NSString stringWithFormat:@"%@:%u", idPrefix, i];
            game.title = [NSString stringWithUTF8String:entry->label];
            game.fullPath = [NSString stringWithUTF8String:entry->path];
            game.filename = [NSString stringWithUTF8String:path_basename(entry->path)];

            if (entry->core_path)
                game.corePath = [NSString stringWithUTF8String:entry->core_path];
            if (entry->core_name)
                game.coreName = [NSString stringWithUTF8String:entry->core_name];

            [games addObject:game];
        }
    }

    return games;
}

+ (NSArray<RetroArchPlaylistGame *> *)getHistoryGames
{
    // Check if history is enabled
    settings_t *settings = config_get_ptr();
    if (!settings || !settings->bools.history_list_enable) {
        RARCH_LOG("History list disabled, cannot access history\n");
        return @[];
    }

    NSArray *games = [self getGamesFromPlaylist:g_defaults.content_history
                                   withIdPrefix:@"history"
                                       maxGames:10];

    RARCH_LOG("App Intents: Returning %lu history games\n", (unsigned long)games.count);
    return games;
}

+ (NSArray<RetroArchPlaylistGame *> *)getFavoriteGames
{
    NSArray *games = [self getGamesFromPlaylist:g_defaults.content_favorites
                                   withIdPrefix:@"favorites"
                                       maxGames:10];

    RARCH_LOG("App Intents: Returning %lu favorite games\n", (unsigned long)games.count);
    return games;
}

@end

NS_ASSUME_NONNULL_END
