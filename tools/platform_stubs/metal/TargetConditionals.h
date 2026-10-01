/* Stand-in for the macOS SDK's <TargetConditionals.h>, for the compile matrix's Metal lane */
#ifndef TARGET_OS_IPHONE
#define TARGET_OS_IPHONE 0
#endif
#ifndef TARGET_OS_OSX
#define TARGET_OS_OSX 1
#endif
#define TARGET_OS_MAC 1
#define TARGET_OS_TV 0
#define TARGET_OS_SIMULATOR 0
