/* Stand-in for macOS's IOKit HID manager header: the types, constants
 * and calls input/drivers_hid/iohidmanager_hid.c names, so it builds on
 * a host with no Apple SDK. The objects are opaque; the test that
 * includes the driver defines them and answers the calls. */
#ifndef STANDIN_IOHIDMANAGER_H
#define STANDIN_IOHIDMANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef long          CFIndex;
typedef const void   *CFTypeRef;
typedef unsigned long CFTypeID;
typedef unsigned char Boolean;
typedef int32_t       IOReturn;
typedef uint32_t      IOOptionBits;
typedef uint32_t      IOHIDElementCookie;
typedef struct { CFIndex location, length; } CFRange;
typedef enum { kCFCompareLessThan = -1, kCFCompareEqualTo,
   kCFCompareGreaterThan } CFComparisonResult;
typedef enum { kCFNumberIntType = 9 } CFNumberType;

typedef const struct __CFString     *CFStringRef;
typedef const struct __CFNumber     *CFNumberRef;
typedef const struct __CFArray      *CFArrayRef;
typedef struct __CFArray            *CFMutableArrayRef;
typedef struct __CFDictionary       *CFMutableDictionaryRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFAllocator  *CFAllocatorRef;
typedef struct __CFRunLoop          *CFRunLoopRef;
typedef struct { int v; } CFDictionaryKeyCallBacks;
typedef struct { int v; } CFDictionaryValueCallBacks;
typedef struct { int v; } CFArrayCallBacks;
typedef CFComparisonResult (*CFComparatorFunction)(const void *,
      const void *, void *);

typedef struct __IOHIDManager *IOHIDManagerRef;
typedef struct __IOHIDDevice  *IOHIDDeviceRef;
typedef struct __IOHIDElement *IOHIDElementRef;
typedef struct __IOHIDValue   *IOHIDValueRef;

typedef enum {
   kIOHIDElementTypeInput_Misc   = 1,
   kIOHIDElementTypeInput_Button = 2,
   kIOHIDElementTypeInput_Axis   = 3
} IOHIDElementType;
typedef enum IOHIDReportType {
   kIOHIDReportTypeInput, kIOHIDReportTypeOutput, kIOHIDReportTypeFeature,
   kIOHIDReportTypeCount
} IOHIDReportType;

typedef void (*IOHIDValueCallback)(void *, IOReturn, void *, IOHIDValueRef);
typedef void (*IOHIDReportCallback)(void *, IOReturn, void *,
      IOHIDReportType, uint32_t, uint8_t *, CFIndex);
typedef void (*IOHIDDeviceCallback)(void *, IOReturn, void *,
      IOHIDDeviceRef);

#define kIOReturnSuccess        0
#define kIOHIDOptionsTypeNone   0

#define kHIDPage_GenericDesktop 0x01
#define kHIDPage_Simulation     0x02
#define kHIDPage_Game           0x05
#define kHIDPage_Button         0x09
#define kHIDPage_Consumer       0x0C
#define kHIDUsage_GD_Joystick   0x04
#define kHIDUsage_GD_GamePad    0x05
#define kHIDUsage_GD_X          0x30
#define kHIDUsage_GD_Y          0x31
#define kHIDUsage_GD_Z          0x32
#define kHIDUsage_GD_Rx         0x33
#define kHIDUsage_GD_Ry         0x34
#define kHIDUsage_GD_Rz         0x35
#define kHIDUsage_GD_Hatswitch  0x39
#define kHIDUsage_Sim_Rudder      0xBA
#define kHIDUsage_Sim_Throttle    0xBB
#define kHIDUsage_Sim_Accelerator 0xC4
#define kHIDUsage_Sim_Brake       0xC5
#define kHIDUsage_Sim_Steering    0xC8

#define CFSTR(s) ((CFStringRef)(s))
extern const CFAllocatorRef kCFAllocatorDefault;
extern const CFStringRef kCFRunLoopCommonModes;
extern const CFStringRef kCFRunLoopDefaultMode;
extern const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
extern const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
extern const CFArrayCallBacks kCFTypeArrayCallBacks;

void CFRelease(CFTypeRef);
CFTypeID CFGetTypeID(CFTypeRef);
CFTypeID CFNumberGetTypeID(void);
CFNumberRef CFNumberCreate(CFAllocatorRef, CFNumberType, const void *);
Boolean CFNumberGetValue(CFNumberRef, CFNumberType, void *);
Boolean CFStringGetCString(CFStringRef, char *, CFIndex, uint32_t);
CFRange CFRangeMake(CFIndex, CFIndex);
CFIndex CFArrayGetCount(CFArrayRef);
const void *CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef, CFIndex,
      const CFArrayCallBacks *);
CFMutableArrayRef CFArrayCreateMutableCopy(CFAllocatorRef, CFIndex,
      CFArrayRef);
void CFArrayAppendValue(CFMutableArrayRef, const void *);
void CFArraySortValues(CFMutableArrayRef, CFRange, CFComparatorFunction,
      void *);
CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef, CFIndex,
      const CFDictionaryKeyCallBacks *, const CFDictionaryValueCallBacks *);
void CFDictionarySetValue(CFMutableDictionaryRef, const void *,
      const void *);
CFRunLoopRef CFRunLoopGetCurrent(void);
#define kCFStringEncodingUTF8 0x08000100

IOHIDManagerRef IOHIDManagerCreate(CFAllocatorRef, IOOptionBits);
IOReturn IOHIDManagerClose(IOHIDManagerRef, IOOptionBits);
void IOHIDManagerScheduleWithRunLoop(IOHIDManagerRef, CFRunLoopRef,
      CFStringRef);
void IOHIDManagerUnscheduleFromRunLoop(IOHIDManagerRef, CFRunLoopRef,
      CFStringRef);
void IOHIDManagerSetDeviceMatching(IOHIDManagerRef, CFDictionaryRef);
void IOHIDManagerSetDeviceMatchingMultiple(IOHIDManagerRef, CFArrayRef);
void IOHIDManagerRegisterDeviceMatchingCallback(IOHIDManagerRef,
      IOHIDDeviceCallback, void *);
void IOHIDManagerRegisterDeviceRemovalCallback(IOHIDManagerRef,
      IOHIDDeviceCallback, void *);
IOReturn IOHIDDeviceOpen(IOHIDDeviceRef, IOOptionBits);
void IOHIDDeviceScheduleWithRunLoop(IOHIDDeviceRef, CFRunLoopRef,
      CFStringRef);
CFTypeRef IOHIDDeviceGetProperty(IOHIDDeviceRef, CFStringRef);
CFArrayRef IOHIDDeviceCopyMatchingElements(IOHIDDeviceRef, CFDictionaryRef,
      IOOptionBits);
void IOHIDDeviceRegisterInputValueCallback(IOHIDDeviceRef,
      IOHIDValueCallback, void *);
void IOHIDDeviceRegisterInputReportCallback(IOHIDDeviceRef, uint8_t *,
      CFIndex, IOHIDReportCallback, void *);
IOReturn IOHIDDeviceSetReport(IOHIDDeviceRef, IOHIDReportType, CFIndex,
      const uint8_t *, CFIndex);
IOReturn IOHIDDeviceGetReport(IOHIDDeviceRef, IOHIDReportType, CFIndex,
      uint8_t *, CFIndex *);
IOHIDElementRef IOHIDValueGetElement(IOHIDValueRef);
CFIndex IOHIDValueGetIntegerValue(IOHIDValueRef);
IOHIDElementType IOHIDElementGetType(IOHIDElementRef);
uint32_t IOHIDElementGetUsagePage(IOHIDElementRef);
uint32_t IOHIDElementGetUsage(IOHIDElementRef);
IOHIDElementCookie IOHIDElementGetCookie(IOHIDElementRef);
CFIndex IOHIDElementGetLogicalMin(IOHIDElementRef);
CFIndex IOHIDElementGetLogicalMax(IOHIDElementRef);
CFIndex IOHIDElementGetPhysicalMin(IOHIDElementRef);
CFIndex IOHIDElementGetPhysicalMax(IOHIDElementRef);

#endif
