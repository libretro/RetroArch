#include <Availability.h>

#ifndef __IPHONE_OS_VERSION_MAX_ALLOWED
#define __IPHONE_OS_VERSION_MAX_ALLOWED 00000
#endif

#if __IPHONE_7_0 && __IPHONE_OS_VERSION_MAX_ALLOWED >= 70000
#define HAVE_MFI
#endif

#include "../apple/common/CFExtensions.m"
#include "../apple/common/utility.m"

#include "../apple/common/RAGameView.m"

#if TARGET_OS_IPHONE
#include "../apple/iOS/platform.m"
#include "../apple/iOS/menu.m"
#include "../apple/iOS/browser.m"
#else
#include "../apple/OSX/platform.m"
#include "../apple/OSX/settings.m"
#endif

#ifdef HAVE_MFI
#include "../apple/common/apple_gamecontroller.m"
#endif