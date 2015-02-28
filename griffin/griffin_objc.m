#include <Availability.h>

#if defined(__IPHONE_7_0) && defined(TARGET_OS_IPHONE)
#define HAVE_MFI
#endif

#include "../apple/common/CFExtensions.m"
#include "../apple/common/utility.m"

#include "../apple/common/RAGameView.m"

#if defined(OSX)
#include "../apple/OSX/platform.m"
#include "../apple/OSX/settings.m"
#elif defined(IOS)
#include "../apple/iOS/platform.m"
#include "../apple/iOS/menu.m"
#include "../apple/iOS/browser.m"
#endif

#ifdef HAVE_MFI
#include "../apple/common/apple_gamecontroller.m"
#endif