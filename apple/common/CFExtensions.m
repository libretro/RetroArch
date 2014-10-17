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

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#import <Foundation/NSPathUtilities.h>
#include "CFExtensions.h"

void CFSearchPathForDirectoriesInDomains(unsigned flags,
                                               unsigned domain_mask, unsigned expand_tilde,
                                               char *buf, size_t sizeof_buf)
{
   CFArrayRef array = CFBridgingRetain(NSSearchPathForDirectoriesInDomains(
      flags, domain_mask, (BOOL)expand_tilde));
   CFStringRef path = CFBridgingRetain(CFArrayGetValueAtIndex(array, 0));
   CFStringGetCString(path, buf, sizeof_buf, kCFStringEncodingUTF8);
   CFRelease(path);
   CFRelease(array);
}
