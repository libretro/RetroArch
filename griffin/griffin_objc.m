#ifdef IOS
#include <Availability.h>
#else
#include <AvailabilityMacros.h>
#endif

#ifndef __IPHONE_OS_VERSION_MAX_ALLOWED
#define __IPHONE_OS_VERSION_MAX_ALLOWED 00000
#endif

#include "../apple/common/apple_cocoa_common.m"

#if TARGET_OS_IPHONE
#include "../apple/iOS/platform.m"
#include "../apple/iOS/menu.m"
#include "../apple/iOS/browser.m"
#include "../ui/drivers/ui_cocoatouch.m"
#else
#include "../apple/OSX/platform.m"
#include "../apple/OSX/settings.m"
#endif

#ifdef HAVE_MFI
#include "../input/drivers_hid/mfi_hid.m"
#endif
