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

#ifndef CF_RETURNS_RETAINED
#if __has_feature(attribute_cf_returns_retained)
#define CF_RETURNS_RETAINED __attribute__((cf_returns_retained))
#else
#define CF_RETURNS_RETAINED
#endif
#endif


NS_INLINE CF_RETURNS_RETAINED CFTypeRef CFBridgingRetainCompat(id X)
{
#ifdef __has_feature(objc_arc)
	return (__bridge_retained CFTypeRef)x;
#else
	return X;
#endif
}


void CFSearchPathForDirectoriesInDomains(unsigned flags,
                                               unsigned domain_mask, unsigned expand_tilde,
                                               char *buf, size_t sizeof_buf)
{
   CFTypeRef array_val = (CFTypeRef)CFBridgingRetainCompat(NSSearchPathForDirectoriesInDomains(flags, domain_mask, (BOOL)expand_tilde));
   CFArrayRef array = array_val ? CFRetain(array_val) : NULL;
   CFTypeRef path_val = (CFTypeRef)CFArrayGetValueAtIndex(array, 0);
   CFStringRef path = path_val ? CFRetain(path_val) : NULL;
   if (!path || !array)
      return;
   
   CFStringGetCString(path, buf, sizeof_buf, kCFStringEncodingUTF8);
   CFRelease(path);
   CFRelease(array);
}
