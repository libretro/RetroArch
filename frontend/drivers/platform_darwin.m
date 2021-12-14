/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Jason Fetters
 * Copyright (C) 2014-2015 - Jay McCarthy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <sys/utsname.h>

#include <mach/mach_host.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFArray.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef __OBJC__
#include <Foundation/NSPathUtilities.h>
#include <objc/message.h>
#endif

#if defined(OSX)
#include <Carbon/Carbon.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include <sys/sysctl.h>
#elif defined(IOS)
#include <UIKit/UIDevice.h>
#include <sys/sysctl.h>
#endif

#include <boolean.h>
#include <compat/apple_compat.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>
#include <string/stdstring.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend_driver.h"
#include "../../file_path_special.h"
#include "../../configuration.h"
#include "../../defaults.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"
#include "../../ui/ui_companion_driver.h"
#include "../../paths.h"

#if 1
#define RELEASE_BUILD
#endif

typedef enum
{
   CFApplicationDirectory           = 1,   /* Supported applications (Applications) */
   CFDemoApplicationDirectory       = 2,   /* Unsupported applications, demonstration versions (Demos) */
   CFDeveloperApplicationDirectory  = 3,   /* Developer applications (Developer/Applications). DEPRECATED - there is no one single Developer directory. */
   CFAdminApplicationDirectory      = 4,   /* System and network administration applications (Administration) */
   CFLibraryDirectory               = 5,   /* various documentation, support, and configuration files, resources (Library) */
   CFDeveloperDirectory             = 6,   /* developer resources (Developer) DEPRECATED - there is no one single Developer directory. */
   CFUserDirectory                  = 7,   /* User home directories (Users) */
   CFDocumentationDirectory         = 8,   /* Documentation (Documentation) */
   CFDocumentDirectory              = 9,   /* Documents (Documents) */
   CFCoreServiceDirectory           = 10,  /* Location of CoreServices directory (System/Library/CoreServices) */
   CFAutosavedInformationDirectory  = 11,  /* Location of autosaved documents (Documents/Autosaved) */
   CFDesktopDirectory               = 12,  /* Location of user's desktop */
   CFCachesDirectory                = 13,  /* Location of discardable cache files (Library/Caches) */
   CFApplicationSupportDirectory    = 14,  /* Location of application support files (plug-ins, etc) (Library/Application Support) */
   CFDownloadsDirectory             = 15,  /* Location of the user's "Downloads" directory */
   CFInputMethodsDirectory          = 16,  /* Input methods (Library/Input Methods) */
   CFMoviesDirectory                = 17,  /* Location of user's Movies directory (~/Movies) */
   CFMusicDirectory                 = 18,  /* Location of user's Music directory (~/Music) */
   CFPicturesDirectory              = 19,  /* Location of user's Pictures directory (~/Pictures) */
   CFPrinterDescriptionDirectory    = 20,  /* Location of system's PPDs directory (Library/Printers/PPDs) */
   CFSharedPublicDirectory          = 21,  /* Location of user's Public sharing directory (~/Public) */
   CFPreferencePanesDirectory       = 22,  /* Location of the PreferencePanes directory for use with System Preferences (Library/PreferencePanes) */
   CFApplicationScriptsDirectory    = 23,  /* Location of the user scripts folder for the calling application (~/Library/Application Scripts/code-signing-id) */
   CFItemReplacementDirectory       = 99,  /* For use with NSFileManager's URLForDirectory:inDomain:appropriateForURL:create:error: */
   CFAllApplicationsDirectory       = 100, /* all directories where applications can occur */
   CFAllLibrariesDirectory          = 101, /* all directories where resources can occur */
   CFTrashDirectory                 = 102  /* location of Trash directory */
} CFSearchPathDirectory;

typedef enum
{
   CFUserDomainMask     = 1,       /* user's home directory --- place to install user's personal items (~) */
   CFLocalDomainMask    = 2,       /* local to the current machine --- place to install items available to everyone on this machine (/Library) */
   CFNetworkDomainMask  = 4,       /* publically available location in the local area network --- place to install items available on the network (/Network) */
   CFSystemDomainMask   = 8,       /* provided by Apple, unmodifiable (/System) */
   CFAllDomainsMask     = 0x0ffff  /* All domains: all of the above and future items */
} CFDomainMask;

#if (defined(OSX) && !(defined(__ppc__) || defined(__ppc64__)))
static int speak_pid                            = 0;
#endif

static char darwin_cpu_model_name[64] = {0};

static void CFSearchPathForDirectoriesInDomains(
      char *s, size_t len)
{
#if TARGET_OS_TV
   NSSearchPathDirectory dir = NSCachesDirectory;
#else
   NSSearchPathDirectory dir = NSDocumentDirectory;
#endif
#if __has_feature(objc_arc)
   CFStringRef       array_val = (__bridge CFStringRef)[
         NSSearchPathForDirectoriesInDomains(dir,
            NSUserDomainMask, YES) firstObject];
#else
   CFStringRef       array_val = (CFStringRef)[
         NSSearchPathForDirectoriesInDomains(dir,
            NSUserDomainMask, YES) firstObject];
#endif
   if (array_val)
      CFStringGetCString(array_val, s, len, kCFStringEncodingUTF8);
}

static void CFTemporaryDirectory(char *s, size_t len)
{
#if __has_feature(objc_arc)
   CFStringRef path = (__bridge CFStringRef)NSTemporaryDirectory();
#else
   CFStringRef path = (CFStringRef)NSTemporaryDirectory();
#endif
   CFStringGetCString(path, s, len, kCFStringEncodingUTF8);
}

#if defined(IOS)
void get_ios_version(int *major, int *minor);
#endif

#if defined(OSX)

#define PMGMT_STRMATCH(a,b) (CFStringCompare(a, b, 0) == kCFCompareEqualTo)
#define PMGMT_GETVAL(k,v)   CFDictionaryGetValueIfPresent(dict, CFSTR(k), (const void **) v)

/* Note that AC power sources also include a laptop battery it is charging. */
static void checkps(CFDictionaryRef dict, bool * have_ac, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   CFStringRef strval; /* don't CFRelease() this. */
   CFBooleanRef bval;
   CFNumberRef numval;
   bool charge = false;
   bool choose = false;
   bool  is_ac = false;
   int    secs = -1;
   int  maxpct = -1;
   int     pct = -1;

   if ((PMGMT_GETVAL(kIOPSIsPresentKey, &bval)) && (bval == kCFBooleanFalse))
      return;

   if (!PMGMT_GETVAL(kIOPSPowerSourceStateKey, &strval))
      return;

   if (PMGMT_STRMATCH(strval, CFSTR(kIOPSACPowerValue)))
      is_ac = *have_ac = true;
   else if (!PMGMT_STRMATCH(strval, CFSTR(kIOPSBatteryPowerValue)))
      return;                 /* not a battery? */

   if ((PMGMT_GETVAL(kIOPSIsChargingKey, &bval)) && (bval == kCFBooleanTrue))
      charge = true;

   if (PMGMT_GETVAL(kIOPSMaxCapacityKey, &numval))
   {
      SInt32 val = -1;
      CFNumberGetValue(numval, kCFNumberSInt32Type, &val);
      if (val > 0)
      {
         *have_battery = true;
         maxpct        = (int) val;
      }
   }

   if (PMGMT_GETVAL(kIOPSMaxCapacityKey, &numval))
   {
      SInt32 val = -1;
      CFNumberGetValue(numval, kCFNumberSInt32Type, &val);
      if (val > 0)
      {
         *have_battery = true;
         maxpct        = (int) val;
      }
   }

   if (PMGMT_GETVAL(kIOPSTimeToEmptyKey, &numval))
   {
      SInt32 val = -1;
      CFNumberGetValue(numval, kCFNumberSInt32Type, &val);

      /* Mac OS X reports 0 minutes until empty if you're plugged in. :( */
      if ((val == 0) && (is_ac))
         val = -1;           /* !!! FIXME: calc from timeToFull and capacity? */

      secs = (int) val;
      if (secs > 0)
         secs *= 60;         /* value is in minutes, so convert to seconds. */
   }

   if (PMGMT_GETVAL(kIOPSCurrentCapacityKey, &numval))
   {
      SInt32 val = -1;
      CFNumberGetValue(numval, kCFNumberSInt32Type, &val);
      pct = (int) val;
   }

   if ((pct > 0) && (maxpct > 0))
      pct = (int) ((((double) pct) / ((double) maxpct)) * 100.0);

   if (pct > 100)
      pct = 100;

   /*
    * We pick the battery that claims to have the most minutes left.
    *  (failing a report of minutes, we'll take the highest percent.)
    */
   if ((secs < 0) && (*seconds < 0))
   {
      if ((pct < 0) && (*percent < 0))
         choose = true;  /* at least we know there's a battery. */
      if (pct > *percent)
         choose = true;
   }
   else if (secs > *seconds)
      choose = true;

   if (choose)
   {
      *seconds  = secs;
      *percent  = pct;
      *charging = charge;
   }
}
#endif

static void frontend_darwin_get_name(char *s, size_t len)
{
#if defined(IOS)
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return;

   strlcpy(s, buffer.machine, len);
#elif defined(OSX)
   size_t length = 0;
   sysctlbyname("hw.model", NULL, &length, NULL, 0);
    if (length)
        sysctlbyname("hw.model", s, &length, NULL, 0);
#endif
}

static void frontend_darwin_get_os(char *s, size_t len, int *major, int *minor)
{
#if defined(IOS)
   get_ios_version(major, minor);
   strcpy_literal(s, "iOS");
#elif defined(OSX)

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101300 // MAC_OS_X_VERSION_10_13
   NSOperatingSystemVersion version = NSProcessInfo.processInfo.operatingSystemVersion;
   *major = (int)version.majorVersion;
   *minor = (int)version.minorVersion;
#else
    /* MacOS 10.9 includes the [NSProcessInfo operatingSystemVersion] function, but it's not in the 10.9 SDK. So, call it via NSInvocation */
    /* Credit: OpenJDK (https://github.com/openjdk/jdk/commit/d4c7db50) */
   if ([[NSProcessInfo processInfo] respondsToSelector:@selector(operatingSystemVersion)])
   {
      typedef struct
      {
         NSInteger majorVersion;
         NSInteger minorVersion;
         NSInteger patchVersion;
      } NSMyOSVersion;
       NSMyOSVersion version;
       NSMethodSignature *sig = [[NSProcessInfo processInfo] methodSignatureForSelector:@selector(operatingSystemVersion)];
       NSInvocation *invoke = [NSInvocation invocationWithMethodSignature:sig];
       invoke.selector = @selector(operatingSystemVersion);
       [invoke invokeWithTarget:[NSProcessInfo processInfo]];
       [invoke getReturnValue:&version];
      *major = (int)version.majorVersion;
      *minor = (int)version.minorVersion;
   }
   else
   {
      Gestalt(gestaltSystemVersionMinor, (SInt32*)minor);
      Gestalt(gestaltSystemVersionMajor, (SInt32*)major);
   }
#endif
   strcpy_literal(s, "OSX");
#endif
}

static void frontend_darwin_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   CFURLRef bundle_url;
   CFStringRef bundle_path;
   CFURLRef resource_url;
   CFStringRef resource_path;
#if TARGET_OS_IPHONE
   char resolved_home_dir_buf[
      PATH_MAX_LENGTH]                   = {0};
   char resolved_bundle_dir_buf[
      PATH_MAX_LENGTH]                   = {0};
#endif
   char temp_dir[PATH_MAX_LENGTH]        = {0};
   char bundle_path_buf[PATH_MAX_LENGTH] = {0};
   char resource_path_buf[PATH_MAX_LENGTH] = {0};
   char full_resource_path_buf[PATH_MAX_LENGTH] = {0};
   char home_dir_buf[PATH_MAX_LENGTH]    = {0};
   CFBundleRef bundle                    = CFBundleGetMainBundle();

   if (!bundle)
      return;

   bundle_url  = CFBundleCopyBundleURL(bundle);
   bundle_path = CFURLCopyPath(bundle_url);
   
   resource_url = CFBundleCopyResourcesDirectoryURL(bundle);
   resource_path = CFURLCopyPath(resource_url);

   CFRelease(resource_url);

   CFStringGetCString(bundle_path,
         bundle_path_buf, sizeof(bundle_path_buf), kCFStringEncodingUTF8);
   CFStringGetCString(resource_path,
         resource_path_buf, sizeof(resource_path_buf), kCFStringEncodingUTF8);
   CFRelease(resource_path);
   fill_pathname_join(full_resource_path_buf, bundle_path_buf, resource_path_buf, sizeof(full_resource_path_buf));
   CFSearchPathForDirectoriesInDomains(
         home_dir_buf, sizeof(home_dir_buf));

#if TARGET_OS_IPHONE
   if (realpath(home_dir_buf, resolved_home_dir_buf))
   {
      retro_assert(strlcpy(home_dir_buf,
               resolved_home_dir_buf,
               sizeof(home_dir_buf)) < sizeof(home_dir_buf));
   }
   if (realpath(bundle_path_buf, resolved_bundle_dir_buf))
   {
      retro_assert(strlcpy(bundle_path_buf,
               resolved_bundle_dir_buf,
               sizeof(bundle_path_buf)) < sizeof(bundle_path_buf));
   }
#endif

   strlcat(home_dir_buf, "/RetroArch", sizeof(home_dir_buf));
#ifdef HAVE_METAL
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER],
                      home_dir_buf, "shaders_slang",
                      sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
#else
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER],
         home_dir_buf, "shaders_glsl",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
#endif
#ifdef HAVE_UPDATE_CORES
    fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE],
		    home_dir_buf, "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
#else
    fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE],
		    bundle_path_buf, "modules", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], home_dir_buf, "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], home_dir_buf, "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], home_dir_buf, "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], home_dir_buf, "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], home_dir_buf, "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], home_dir_buf, "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], home_dir_buf, "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], home_dir_buf, "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], home_dir_buf, "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], home_dir_buf, "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], home_dir_buf, "cht", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], home_dir_buf, "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], home_dir_buf, "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], home_dir_buf, "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG], home_dir_buf, "records_config", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT], home_dir_buf, "records", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], home_dir_buf, "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
#if defined(IOS)
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], home_dir_buf, "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
#endif
#if defined(OSX)
   char application_data[PATH_MAX_LENGTH];

   fill_pathname_application_data(application_data, sizeof(application_data));

#ifdef HAVE_CG
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], home_dir_buf, "shaders_cg", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], application_data, "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], application_data, "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], application_data, "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], application_data, "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], application_data, "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
#if defined(RELEASE_BUILD)
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], application_data, "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], application_data, "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], application_data, "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], application_data, "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], application_data, "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], application_data, "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], application_data, "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], application_data, "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], application_data, "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], application_data, "cht", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], application_data, "audio_filters", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], application_data, "video_filters", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
#else
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], home_dir_buf, "audio_filters", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], home_dir_buf, "video_filters", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
#endif

#endif

    char assets_zip_path[PATH_MAX_LENGTH];
#if TARGET_OS_IOS
    {
       int major, minor;
       get_ios_version(&major, &minor);
       if (major > 8)
          strcpy_literal(g_defaults.path_buildbot_server_url, "http://buildbot.libretro.com/nightly/apple/ios9/latest/");
    }
#endif

#if TARGET_OS_IOS
    fill_pathname_join(assets_zip_path, bundle_path_buf, "assets.zip", sizeof(assets_zip_path));
#else
    fill_pathname_join(assets_zip_path, full_resource_path_buf, "assets.zip", sizeof(assets_zip_path));
#endif

    if (path_is_valid(assets_zip_path))
    {
       settings_t *settings = config_get_ptr();
       configuration_set_string(settings,
             settings->arrays.bundle_assets_src,
             assets_zip_path);
       configuration_set_string(settings,
             settings->arrays.bundle_assets_dst,
#if TARGET_OS_IOS || TARGET_OS_TV
             home_dir_buf
#else
             application_data
#endif
       );
       /* TODO/FIXME: Just hardcode this for now */
       configuration_set_uint(settings, settings->uints.bundle_assets_extract_version_current, 1);
    }

   CFTemporaryDirectory(temp_dir, sizeof(temp_dir));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CACHE],
         temp_dir,
         sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   path_mkdir(bundle_path_buf);

   if (access(bundle_path_buf, 0755) != 0) { }
   else
   {
      path_mkdir(g_defaults.dirs[DEFAULT_DIR_SYSTEM]);

      if (access(g_defaults.dirs[DEFAULT_DIR_SYSTEM], 0755) != 0) { }
   }

   CFRelease(bundle_path);
   CFRelease(bundle_url);

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void frontend_darwin_content_loaded(void)
{
   ui_companion_driver_notify_content_loaded();
}

static int frontend_darwin_get_rating(void)
{
   char model[PATH_MAX_LENGTH] = {0};

   frontend_darwin_get_name(model, sizeof(model));

   /* iPhone 4 */
#if 0
   if (strstr(model, "iPhone3"))
      return -1;
#endif

   /* iPad 1 */
#if 0
   if (strstr(model, "iPad1,1"))
      return -1;
#endif

   /* iPhone 4S */
   if (strstr(model, "iPhone4,1"))
      return 8;

   /* iPad 2/iPad Mini 1 */
   if (strstr(model, "iPad2"))
      return 9;

   /* iPhone 5/5C */
   if (strstr(model, "iPhone5"))
      return 13;

   /* iPhone 5S */
   if (strstr(model, "iPhone6,1") || strstr(model, "iPhone6,2"))
      return 14;

   /* iPad Mini 2/3 */
   if (     strstr(model, "iPad4,4")
         || strstr(model, "iPad4,5")
         || strstr(model, "iPad4,6")
         || strstr(model, "iPad4,7")
         || strstr(model, "iPad4,8")
         || strstr(model, "iPad4,9")
      )
      return 15;

   /* iPad Air */
   if (     strstr(model, "iPad4,1")
         || strstr(model, "iPad4,2")
         || strstr(model, "iPad4,3")
      )
      return 16;

   /* iPhone 6, iPhone 6 Plus */
   if (strstr(model, "iPhone7"))
      return 17;

   /* iPad Air 2 */
   if (strstr(model, "iPad5,3") || strstr(model, "iPad5,4"))
      return 18;

   /* iPad Pro (12.9 Inch) */
   if (strstr(model, "iPad6,7") || strstr(model, "iPad6,8"))
     return 19;

   /* iPad Pro (9.7 Inch) */
   if (strstr(model, "iPad6,3") || strstr(model, "iPad6,4"))
     return 19;

   /* iPad 5th Generation */
   if (strstr(model, "iPad6,11") || strstr(model, "iPad6,12"))
     return 19;

   /* iPad Pro (12.9 Inch 2nd Generation) */
   if (strstr(model, "iPad7,1") || strstr(model, "iPad7,2"))
     return 19;

   /* iPad Pro (10.5 Inch) */
   if (strstr(model, "iPad7,3") || strstr(model, "iPad7,4"))
     return 19;

   /* iPad Pro 6th Generation) */
   if (strstr(model, "iPad7,5") || strstr(model, "iPad7,6"))
     return 19;

   /* iPad Pro (11 Inch) */
   if (     strstr(model, "iPad8,1")
         || strstr(model, "iPad8,2")
         || strstr(model, "iPad8,3")
         || strstr(model, "iPad8,4")
      )
      return 19;

   /* iPad Pro (12.9 3rd Generation) */
    if (   strstr(model, "iPad8,5")
        || strstr(model, "iPad8,6")
        || strstr(model, "iPad8,7")
        || strstr(model, "iPad8,8")
       )
       return 19;

   /* iPad Air 3rd Generation) */
    if (   strstr(model, "iPad11,3")
        || strstr(model, "iPad11,4"))
       return 19;

   /* TODO/FIXME -
      - more ratings for more systems
      - determine rating more intelligently*/
   return -1;
}

static enum frontend_powerstate frontend_darwin_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;
#if defined(OSX)
   CFIndex i, total;
   CFArrayRef list;
   bool have_ac, have_battery, charging;
   CFTypeRef blob  = IOPSCopyPowerSourcesInfo();

   *seconds        = -1;
   *percent        = -1;

   if (!blob)
      goto end;

   list = IOPSCopyPowerSourcesList(blob);

   if (!list)
      goto end;

   /* don't CFRelease() the list items, or dictionaries! */
   have_ac         = false;
   have_battery    = false;
   charging        = false;
   total           = CFArrayGetCount(list);

   for (i = 0; i < total; i++)
   {
      CFTypeRef ps = (CFTypeRef)CFArrayGetValueAtIndex(list, i);
      CFDictionaryRef dict = IOPSGetPowerSourceDescription(blob, ps);
      if (dict)
         checkps(dict, &have_ac, &have_battery, &charging,
               seconds, percent);
   }

   if (!have_battery)
      ret = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (charging)
      ret = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   CFRelease(list);
end:
   if (blob)
      CFRelease(blob);
#elif TARGET_OS_IOS
   float level;
   UIDevice *uidev = [UIDevice currentDevice];

   if (!uidev)
	   return ret;

   [uidev setBatteryMonitoringEnabled:true];

   switch (uidev.batteryState)
   {
	   case UIDeviceBatteryStateCharging:
		   ret = FRONTEND_POWERSTATE_CHARGING;
		   break;
	   case UIDeviceBatteryStateFull:
		   ret = FRONTEND_POWERSTATE_CHARGED;
		   break;
	   case UIDeviceBatteryStateUnplugged:
		   ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;
		   break;
	   case UIDeviceBatteryStateUnknown:
		   break;
   }

   level = uidev.batteryLevel;

   *percent = ((level < 0.0f) ? -1 : ((int)((level * 100) + 0.5f)));

   [uidev setBatteryMonitoringEnabled:false];
#endif
   return ret;
}

#ifndef OSX
#ifndef CPU_ARCH_ABI64
#define CPU_ARCH_ABI64          0x01000000
#endif

#ifndef CPU_TYPE_ARM64
#define CPU_TYPE_ARM64          (CPU_TYPE_ARM | CPU_ARCH_ABI64)
#endif
#endif

static enum frontend_architecture frontend_darwin_get_arch(void)
{
#ifdef OSX
    struct utsname buffer;

    if (uname(&buffer) != 0)
       return FRONTEND_ARCH_NONE;
    
   if (string_is_equal(buffer.machine, "x86_64"))
      return FRONTEND_ARCH_X86_64;
   if (string_is_equal(buffer.machine, "x86"))
      return FRONTEND_ARCH_X86;
   if (string_is_equal(buffer.machine, "Power Macintosh"))
      return FRONTEND_ARCH_PPC;
   if (string_is_equal(buffer.machine, "arm64"))
      return FRONTEND_ARCH_ARMV8;
#else
   cpu_type_t type;
   size_t size = sizeof(type);

   sysctlbyname("hw.cputype", &type, &size, NULL, 0);
    
   if (type == CPU_TYPE_X86_64)
      return FRONTEND_ARCH_X86_64;
   else if (type == CPU_TYPE_X86)
      return FRONTEND_ARCH_X86;
   else if (type == CPU_TYPE_ARM64)
      return FRONTEND_ARCH_ARMV8;
   else if (type == CPU_TYPE_ARM)
      return FRONTEND_ARCH_ARMV7;
#endif
    return FRONTEND_ARCH_NONE;
}

static int frontend_darwin_parse_drive_list(void *data, bool load_content)
{
   int ret = -1;
#if TARGET_OS_IPHONE
#ifdef HAVE_MENU
   file_list_t *list                     = (file_list_t*)data;
   char bundle_path_buf[PATH_MAX_LENGTH] = {0};
   char home_dir_buf[PATH_MAX_LENGTH]    = {0};
   CFBundleRef bundle                    = CFBundleGetMainBundle();
   enum msg_hash_enums enum_idx          = load_content 
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR 
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   CFURLRef bundle_url                   = CFBundleCopyBundleURL(bundle);
   CFStringRef bundle_path               = CFURLCopyPath(bundle_url);

   CFStringGetCString(bundle_path, bundle_path_buf,
         sizeof(bundle_path_buf), kCFStringEncodingUTF8);

   CFSearchPathForDirectoriesInDomains(
         home_dir_buf, sizeof(home_dir_buf));

   menu_entries_append_enum(list,
         home_dir_buf,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list, "/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
        FILE_TYPE_DIRECTORY, 0, 0);

   ret = 0;

   CFRelease(bundle_path);
   CFRelease(bundle_url);
#endif
#endif

   return ret;
}

static uint64_t frontend_darwin_get_total_mem(void)
{
#if defined(OSX)
    uint64_t size;
    int mib[2]     = { CTL_HW, HW_MEMSIZE };
    u_int namelen  = sizeof(mib) / sizeof(mib[0]);
    size_t len     = sizeof(size);

    if (sysctl(mib, namelen, &size, &len, NULL, 0) < 0)
        return 0;
    return size;
#else
    return 0;
#endif
}

static uint64_t frontend_darwin_get_free_mem(void)
{
#if (defined(OSX) && !(defined(__ppc__) || defined(__ppc64__)))
    vm_size_t page_size;
    vm_statistics64_data_t vm_stats;
    mach_port_t mach_port        = mach_host_self();
    mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);

    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                          (host_info64_t)&vm_stats, &count))
    {

        long long used_memory = ((int64_t)vm_stats.active_count +
                                 (int64_t)vm_stats.inactive_count +
                                 (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
        return used_memory;
    }
#endif
    return 0;
}

static const char* frontend_darwin_get_cpu_model_name(void)
{
   cpu_features_get_model_name(darwin_cpu_model_name, sizeof(darwin_cpu_model_name));
   return darwin_cpu_model_name;
}

#if (defined(OSX) && !(defined(__ppc__) || defined(__ppc64__)))
static char* accessibility_mac_language_code(const char* language)
{
   if (string_is_equal(language,"en"))
      return "Alex";
   else if (string_is_equal(language,"it"))
      return "Alice";
   else if (string_is_equal(language,"sv"))
      return "Alva";
   else if (string_is_equal(language,"fr"))
      return "Amelie";
   else if (string_is_equal(language,"de"))
      return "Anna";
   else if (string_is_equal(language,"he"))
      return "Carmit";
   else if (string_is_equal(language,"id"))
      return "Damayanti";
   else if (string_is_equal(language,"es"))
      return "Diego";
   else if (string_is_equal(language,"nl"))
      return "Ellen";
   else if (string_is_equal(language,"ro"))
      return "Ioana";
   else if (string_is_equal(language,"pt_pt"))
      return "Joana";
   else if (string_is_equal(language,"pt_bt") || string_is_equal(language,"pt"))
      return "Luciana";
   else if (string_is_equal(language,"th"))
      return "Kanya";
   else if (string_is_equal(language,"ja"))
      return "Kyoko";
   else if (string_is_equal(language,"sk"))
      return "Laura";
   else if (string_is_equal(language,"hi"))
      return "Lekha";
   else if (string_is_equal(language,"ar"))
      return "Maged";
   else if (string_is_equal(language,"hu"))
      return "Mariska";
   else if (string_is_equal(language,"zh_tw") || string_is_equal(language,"zh"))
      return "Mei-Jia";
   else if (string_is_equal(language,"el"))
      return "Melina";
   else if (string_is_equal(language,"ru"))
      return "Milena";
   else if (string_is_equal(language,"nb"))
      return "Nora";
   else if (string_is_equal(language,"da"))
      return "Sara";
   else if (string_is_equal(language,"fi"))
      return "Satu";
   else if (string_is_equal(language,"zh_hk"))
      return "Sin-ji";
   else if (string_is_equal(language,"zh_cn"))
      return "Ting-Ting";
   else if (string_is_equal(language,"tr"))
      return "Yelda";
   else if (string_is_equal(language,"ko"))
      return "Yuna";
   else if (string_is_equal(language,"pl"))
      return "Zosia";
   else if (string_is_equal(language,"cs")) 
      return "Zuzana";
   else
      return "";
}

static bool is_narrator_running_macos(void)
{
   return (kill(speak_pid, 0) == 0);
}

static bool accessibility_speak_macos(int speed,
      const char* speak_text, int priority)
{
   int pid;
   const char *voice      = get_user_language_iso639_1(false);
   char* language_speaker = accessibility_mac_language_code(voice);
   char* speeds[10]       = {"80", "100", "125", "150", "170", "210", "260", "310", "380", "450"};

   if (speed < 1)
      speed = 1;
   else if (speed > 10)
      speed = 10;

   if (priority < 10 && speak_pid > 0)
   {
      /* check if old pid is running */
      if (is_narrator_running_macos())
         return true;
   }

   if (speak_pid > 0)
   {
      /* Kill the running say */
      kill(speak_pid, SIGTERM);
      speak_pid = 0;
   }

   pid = fork();
   /* Could not fork for say command */
   if (pid < 0) { }
   else if (pid > 0)
   {
      /* parent process */
      speak_pid = pid;

      /* Tell the system that we'll ignore the exit status of the child 
       * process.  This prevents zombie processes. */
      signal(SIGCHLD,SIG_IGN);
   }
   else
   { 
      /* child process: replace process with the say command */ 
      if (strlen(language_speaker)> 0)
      {
         char* cmd[] = {"say", "-v", NULL, 
                        NULL, "-r", NULL, NULL};
         cmd[2] = language_speaker;
         cmd[3] = (char *) speak_text;
         cmd[5] = speeds[speed-1];
         execvp("say", cmd);
      }
      else
      {
         char* cmd[] = {"say", NULL, "-r", NULL,  NULL};
         cmd[1] = (char*) speak_text;
         cmd[3] = speeds[speed-1];
         execvp("say",cmd);
      }
   }
   return true;
}
#endif

frontend_ctx_driver_t frontend_ctx_darwin = {
   frontend_darwin_get_env,         /* get_env */
   NULL,                            /* init */
   NULL,                            /* deinit */
   NULL,                            /* exitspawn */
   NULL,                            /* process_args */
   NULL,                            /* exec */
   NULL,                            /* set_fork */
   NULL,                            /* shutdown */
   frontend_darwin_get_name,        /* get_name */
   frontend_darwin_get_os,          /* get_os               */
   frontend_darwin_get_rating,      /* get_rating           */
   frontend_darwin_content_loaded,  /* content_loaded       */
   frontend_darwin_get_arch,        /* get_architecture     */
   frontend_darwin_get_powerstate,  /* get_powerstate       */
   frontend_darwin_parse_drive_list,/* parse_drive_list     */
   frontend_darwin_get_total_mem,   /* get_total_mem        */
   frontend_darwin_get_free_mem,    /* get_free_mem         */
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_signal_handler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* get_lakka_version */
   NULL,                            /* set_screen_brightness */
   NULL,                            /* watch_path_for_changes */
   NULL,                            /* check_for_path_changes */
   NULL,                            /* set_sustained_performance_mode */
#if (defined(OSX) && !(defined(__ppc__) || defined(__ppc64__)))
    frontend_darwin_get_cpu_model_name, /* get_cpu_model_name */
#else
   NULL,                            /* get_cpu_model_name */
#endif
   NULL,                            /* get_user_language   */
#if (defined(OSX) && !(defined(__ppc__) || defined(__ppc64__)))
   is_narrator_running_macos,       /* is_narrator_running */
   accessibility_speak_macos,       /* accessibility_speak */
#else
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
#endif
   NULL,                            /* set_gamemode        */
   "darwin",                        /* ident               */
   NULL                             /* get_video_driver    */
};
