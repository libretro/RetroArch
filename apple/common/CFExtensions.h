/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef _CFEXTENSIONS_H
#define _CFEXTENSIONS_H

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

#endif
