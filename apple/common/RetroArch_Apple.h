/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_APPLE_H
#define __RARCH_APPLE_H

#include <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <CoreFoundation/CFArray.h>

typedef enum
{
    CFApplicationDirectory = 1,             // supported applications (Applications)
    CFDemoApplicationDirectory,             // unsupported applications, demonstration versions (Demos)
    CFDeveloperApplicationDirectory,        // developer applications (Developer/Applications). DEPRECATED - there is no one single Developer directory.
    CFAdminApplicationDirectory,            // system and network administration applications (Administration)
    CFLibraryDirectory,                     // various documentation, support, and configuration files, resources (Library)
    CFDeveloperDirectory,                   // developer resources (Developer) DEPRECATED - there is no one single Developer directory.
    CFUserDirectory,                        // user home directories (Users)
    CFDocumentationDirectory,               // documentation (Documentation)
    CFDocumentDirectory,                    // documents (Documents)
    CFCoreServiceDirectory,                 // location of CoreServices directory (System/Library/CoreServices)
    CFAutosavedInformationDirectory = 11,   // location of autosaved documents (Documents/Autosaved)
    CFDesktopDirectory = 12,                // location of user's desktop
    CFCachesDirectory = 13,                 // location of discardable cache files (Library/Caches)
    CFApplicationSupportDirectory = 14,     // location of application support files (plug-ins, etc) (Library/Application Support)
    CFDownloadsDirectory = 15,              // location of the user's "Downloads" directory
    CFInputMethodsDirectory = 16,           // input methods (Library/Input Methods)
    CFMoviesDirectory = 17,                 // location of user's Movies directory (~/Movies)
    CFMusicDirectory = 18,                  // location of user's Music directory (~/Music)
    CFPicturesDirectory = 19,               // location of user's Pictures directory (~/Pictures)
    CFPrinterDescriptionDirectory = 20,     // location of system's PPDs directory (Library/Printers/PPDs)
    CFSharedPublicDirectory = 21,           // location of user's Public sharing directory (~/Public)
    CFPreferencePanesDirectory = 22,        // location of the PreferencePanes directory for use with System Preferences (Library/PreferencePanes)
    CFApplicationScriptsDirectory = 23,      // location of the user scripts folder for the calling application (~/Library/Application Scripts/code-signing-id)
    CFItemReplacementDirectory = 99,	    // For use with NSFileManager's URLForDirectory:inDomain:appropriateForURL:create:error:
    CFAllApplicationsDirectory = 100,       // all directories where applications can occur
    CFAllLibrariesDirectory = 101,          // all directories where resources can occur
    CFTrashDirectory = 102                   // location of Trash directory
    
} CFSearchPathDirectory;

typedef enum
{
    CFUserDomainMask = 1,       // user's home directory --- place to install user's personal items (~)
    CFLocalDomainMask = 2,      // local to the current machine --- place to install items available to everyone on this machine (/Library)
    CFNetworkDomainMask = 4,    // publically available location in the local area network --- place to install items available on the network (/Network)
    CFSystemDomainMask = 8,     // provided by Apple, unmodifiable (/System)
    CFAllDomainsMask = 0x0ffff  // all domains: all of the above and future items
} CFDomainMask;

void CFSearchPathForDirectoriesInDomains(unsigned flags,
      unsigned domain_mask, unsigned expand_tilde, char *buf, size_t sizeof_buf);

void CFTemporaryDirectory(char *buf, size_t sizeof_buf);

#include "../../core_info.h"
#include "../../playlist.h"
#include "../../settings.h"
#include "../../menu/menu.h"

@protocol RetroArch_Platform
- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;
@end

#ifdef IOS
#include <UIKit/UIKit.h>

#include <CoreLocation/CoreLocation.h>
#import <AVFoundation/AVCaptureOutput.h>

@protocol RAMenuItemBase
- (UITableViewCell*)cellForTableView:(UITableView*)tableView;
- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller;
@end

/*********************************************/
/* RAMenuBase                                */
/* A menu class that displays RAMenuItemBase */
/* objects.                                  */
/*********************************************/
@interface RAMenuBase : UITableViewController
@property (nonatomic) NSMutableArray* sections;
@property (nonatomic) BOOL hidesHeaders;

- (id)initWithStyle:(UITableViewStyle)style;
- (id)itemForIndexPath:(NSIndexPath*)indexPath;

@end

/*********************************************/
/* RAMenuItemBasic                           */
/* A simple menu item that displays a text   */
/* description and calls a block object when */
/* selected.                                 */
/*********************************************/
@interface RAMenuItemBasic : NSObject<RAMenuItemBase>
@property (nonatomic) NSString* description;
@property (nonatomic) id userdata;
@property (copy) void (^action)(id userdata);
@property (copy) NSString* (^detail)(id userdata);

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action;
+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action detail:(NSString* (^)())detail;
+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description association:(id)userdata action:(void (^)())action detail:(NSString* (^)())detail;

@end

/*********************************************/
/* RAMenuItemGeneralSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a string or */
/* numeric setting.                          */
/*********************************************/
@interface RAMenuItemGeneralSetting : NSObject<RAMenuItemBase>
@property (nonatomic) rarch_setting_t* setting;
@property (copy) void (^action)();
@property (nonatomic, weak) UITableView* parentTable;
+ (id)itemForSetting:(rarch_setting_t*)setting action:(void (^)())action;
- (id)initWithSetting:(rarch_setting_t*)setting action:(void (^)())action;
@end

/*********************************************/
/* RAMenuItemBooleanSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a boolean   */
/* setting.                                  */
/*********************************************/
@interface RAMenuItemBooleanSetting : NSObject<RAMenuItemBase>
@property (nonatomic) rarch_setting_t* setting;
@property (copy) void (^action)();
- (id)initWithSetting:(rarch_setting_t*)setting action:(void (^)())action;
@end

/*********************************************/
/* RAMenuItemPathSetting                     */
/* A menu item that displays and allows      */
/* browsing for a path setting.              */
/*********************************************/
@interface RAMenuItemPathSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMenuItemEnumSetting                     */
/* A menu item that displays and allows      */
/* a setting to be set from a list of        */
/* allowed choices.                          */
/*********************************************/
@interface RAMenuItemEnumSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMenuItemBindSetting                     */
/* A menu item that displays and allows      */
/* mapping of a keybinding.                  */
/*********************************************/
@interface RAMenuItemBindSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMainMenu                                */
/* Menu object that is displayed immediately */
/* after startup.                            */
/*********************************************/
@interface RAMainMenu : RAMenuBase
@property (nonatomic) NSString* core;
@end

@interface RADirectoryItem : NSObject<RAMenuItemBase>
@property (nonatomic) NSString* path;
@property (nonatomic) bool isDirectory;
@end

@interface RADirectoryList : RAMenuBase<UIActionSheetDelegate>
@property (nonatomic, weak) RADirectoryItem* selectedItem;

@property (nonatomic, copy) void (^chooseAction)(RADirectoryList* list, RADirectoryItem* item);
@property (nonatomic, copy) NSString* path;
@property (nonatomic, copy) NSString* extensions;

@property (nonatomic) bool allowBlank;
@property (nonatomic) bool forDirectory;

- (id)initWithPath:(NSString*)path extensions:(const char*)extensions action:(void (^)(RADirectoryList* list, RADirectoryItem* item))action;
- (void)browseTo:(NSString*)path;
@end

@interface RAFoldersList : RAMenuBase
- (id) initWithFilePath:(NSString*)path;
@end

typedef struct
{
   char orientations[32];
   unsigned orientation_flags;
   char bluetooth_mode[64];
} apple_frontend_settings_t;
extern apple_frontend_settings_t apple_frontend_settings;

@interface RAGameView : UIViewController<CLLocationManagerDelegate, AVCaptureAudioDataOutputSampleBufferDelegate>
+ (RAGameView*)get;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate, RetroArch_Platform>

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)toggleUI;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;

- (void)refreshSystemConfig;
@end

void get_ios_version(int *major, int *minor);

#elif defined(OSX)
#include <AppKit/AppKit.h>
#ifdef HAVE_LOCATION
#include <CoreLocation/CoreLocation.h>
#endif


@interface RAGameView : NSView
#ifdef HAVE_LOCATION
<CLLocationManagerDelegate>
#endif

+ (RAGameView*)get;
#ifndef OSX
- (void)display;
#endif

@end

@interface RetroArch_OSX : NSObject<RetroArch_Platform>
{
   NSWindow* _window;
   NSWindowController* _settingsWindow;
   NSWindow* _coreSelectSheet;
   NSString* _file;
   NSString* _core;
}

@property (nonatomic, retain) NSWindow IBOutlet* window;

+ (RetroArch_OSX*)get;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;

@end

#endif

extern id<RetroArch_Platform> apple_platform;

/* utility.m */
extern void apple_display_alert(const char *message, const char *title);

@interface RANumberFormatter : NSNumberFormatter
#ifdef IOS
<UITextFieldDelegate>
#endif

- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

#endif
