/* Stand-in for the Foundation declarations gfx/drivers/metal.m and the
 * headers it pulls in use, so the compile matrix can check the Metal
 * driver's syntax on a host without the macOS SDK. Shapes follow the
 * SDK; nothing here links. */
#ifndef STUB_FOUNDATION_H
#define STUB_FOUNDATION_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <TargetConditionals.h>
#include <AvailabilityMacros.h>

typedef signed char BOOL;
#ifndef nil
#define nil ((void *)0)
#endif
#ifndef Nil
#define Nil ((void *)0)
#endif
#define YES ((BOOL)1)
#define NO  ((BOOL)0)
typedef long NSInteger;
typedef unsigned long NSUInteger;
typedef double CGFloat;
typedef double NSTimeInterval;
#define NSNotFound ((NSInteger)0x7fffffffffffffffL)
#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
#define NS_ASSUME_NONNULL_BEGIN
#define NS_ASSUME_NONNULL_END
#define NS_AVAILABLE(a, b)
#define NS_AVAILABLE_MAC(a)
#define NS_AVAILABLE_IOS(a)
#define API_AVAILABLE(...)
#define API_UNAVAILABLE(...)
#define NS_DESIGNATED_INITIALIZER
#define NS_RETURNS_RETAINED
#define CF_RETURNS_RETAINED
#define nullable
#define nonnull
#define _Nullable
#define _Nonnull
#define __kindof

typedef struct CGPoint { CGFloat x, y; } CGPoint;
typedef struct CGSize { CGFloat width, height; } CGSize;
typedef struct CGRect { CGPoint origin; CGSize size; } CGRect;
typedef CGPoint NSPoint;
typedef CGSize NSSize;
typedef CGRect NSRect;
typedef struct _NSRange { NSUInteger location, length; } NSRange;
static inline NSRange NSMakeRange(NSUInteger l, NSUInteger n) { NSRange r; r.location = l; r.length = n; return r; }
static inline CGRect CGRectMake(CGFloat x, CGFloat y, CGFloat w, CGFloat h) { CGRect r; r.origin.x = x; r.origin.y = y; r.size.width = w; r.size.height = h; return r; }
static inline CGSize CGSizeMake(CGFloat w, CGFloat h) { CGSize s; s.width = w; s.height = h; return s; }
static inline CGPoint CGPointMake(CGFloat x, CGFloat y) { CGPoint p; p.x = x; p.y = y; return p; }
static inline bool CGSizeEqualToSize(CGSize a, CGSize b) { return a.width == b.width && a.height == b.height; }
static inline bool CGRectEqualToRect(CGRect a, CGRect b) { return a.origin.x == b.origin.x && a.origin.y == b.origin.y && CGSizeEqualToSize(a.size, b.size); }
static inline CGFloat CGRectGetMinX(CGRect r) { return r.origin.x; }
static inline CGFloat CGRectGetMinY(CGRect r) { return r.origin.y; }
static inline CGFloat CGRectGetMaxX(CGRect r) { return r.origin.x + r.size.width; }
static inline CGFloat CGRectGetMaxY(CGRect r) { return r.origin.y + r.size.height; }
void __sincosf(float a, float *s, float *c);
#define NSMakeRect CGRectMake
#define NSMakeSize CGSizeMake
#define NSMakePoint CGPointMake
typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
void CFRelease(CFTypeRef);
CFTypeRef CFRetain(CFTypeRef);

@class NSString, NSError, NSArray, NSDictionary, NSURL, NSData, NSBundle, NSCoder;
@protocol NSObject
- (id)retain;
- (oneway void)release;
- (id)autorelease;
- (NSUInteger)retainCount;
- (BOOL)isKindOfClass:(Class)c;
- (BOOL)respondsToSelector:(SEL)s;
- (BOOL)conformsToProtocol:(Protocol *)p;
- (Class)class;
- (NSString *)description;
- (id)performSelector:(SEL)s;
@end
@protocol NSCopying
- (id)copyWithZone:(void *)zone;
@end
@protocol NSFastEnumeration
@end
__attribute__((objc_root_class))
@interface NSObject <NSObject>
{
   Class isa;
}
+ (id)alloc;
+ (id)new;
+ (Class)class;
+ (BOOL)instancesRespondToSelector:(SEL)s;
- (id)init;
- (void)dealloc;
- (id)copy;
- (id)mutableCopy;
@end
@interface NSString : NSObject <NSCopying>
+ (id)stringWithUTF8String:(const char *)s;
+ (id)stringWithFormat:(NSString *)fmt, ...;
+ (id)stringWithCString:(const char *)s encoding:(NSUInteger)e;
- (id)initWithUTF8String:(const char *)s;
- (id)initWithBytes:(const void *)b length:(NSUInteger)l encoding:(NSUInteger)e;
- (id)initWithFormat:(NSString *)fmt, ...;
- (const char *)UTF8String;
- (const char *)cStringUsingEncoding:(NSUInteger)e;
- (NSUInteger)length;
- (BOOL)isEqualToString:(NSString *)s;
- (NSString *)stringByAppendingString:(NSString *)s;
- (NSString *)stringByAppendingPathComponent:(NSString *)s;
- (NSString *)lastPathComponent;
- (NSString *)stringByDeletingLastPathComponent;
- (BOOL)hasPrefix:(NSString *)s;
- (BOOL)hasSuffix:(NSString *)s;
- (NSString *)stringByAppendingPathExtension:(NSString *)s;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)a withString:(NSString *)b;
- (BOOL)writeToFile:(NSString *)p atomically:(BOOL)a encoding:(NSUInteger)e error:(NSError **)err;
- (NSString *)localizedDescription;
@property (readonly) NSString *stringByDeletingPathExtension;
@end
#define NSUTF8StringEncoding 4
#define NSASCIIStringEncoding 1
@interface NSMutableString : NSString
@end
@interface NSArray<__covariant ObjectType> : NSObject <NSCopying, NSFastEnumeration>
+ (id)array;
+ (id)arrayWithObject:(id)o;
+ (id)arrayWithObjects:(id)first, ...;
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)i;
- (id)objectAtIndexedSubscript:(NSUInteger)i;
- (id)firstObject;
- (id)lastObject;
@end
@interface NSMutableArray<ObjectType> : NSArray<ObjectType>
+ (id)arrayWithCapacity:(NSUInteger)n;
- (void)addObject:(id)o;
- (void)removeAllObjects;
- (void)removeObjectAtIndex:(NSUInteger)i;
- (void)removeObject:(id)o;
- (void)insertObject:(id)o atIndex:(NSUInteger)i;
- (void)setObject:(id)o atIndexedSubscript:(NSUInteger)i;
@end
@interface NSDictionary : NSObject <NSCopying, NSFastEnumeration>
+ (id)dictionary;
+ (id)dictionaryWithObjectsAndKeys:(id)first, ...;
- (id)objectForKey:(id)k;
- (id)objectForKeyedSubscript:(id)k;
- (NSUInteger)count;
@end
@interface NSMutableDictionary : NSDictionary
+ (id)dictionaryWithCapacity:(NSUInteger)n;
- (void)setObject:(id)o forKey:(id)k;
- (void)setObject:(id)o forKeyedSubscript:(id)k;
- (void)removeObjectForKey:(id)k;
@end
@interface NSNumber : NSObject
+ (NSNumber *)numberWithInt:(int)v;
+ (NSNumber *)numberWithUnsignedInt:(unsigned)v;
+ (NSNumber *)numberWithBool:(BOOL)v;
+ (NSNumber *)numberWithFloat:(float)v;
+ (NSNumber *)numberWithInteger:(NSInteger)v;
+ (NSNumber *)numberWithUnsignedInteger:(NSUInteger)v;
- (int)intValue;
- (unsigned)unsignedIntValue;
- (BOOL)boolValue;
- (float)floatValue;
- (NSInteger)integerValue;
@end
@interface NSValue : NSObject
@end
@interface NSError : NSObject
- (NSString *)localizedDescription;
- (NSInteger)code;
- (NSString *)domain;
@end
@interface NSData : NSObject
+ (id)dataWithBytes:(const void *)b length:(NSUInteger)l;
+ (id)dataWithBytesNoCopy:(void *)b length:(NSUInteger)l freeWhenDone:(BOOL)f;
+ (id)dataWithContentsOfFile:(NSString *)p;
- (id)initWithBytesNoCopy:(void *)b length:(NSUInteger)l freeWhenDone:(BOOL)f;
- (const void *)bytes;
- (NSUInteger)length;
@end
@interface NSURL : NSObject
+ (id)fileURLWithPath:(NSString *)p;
@property (readonly) NSString *lastPathComponent;
- (NSString *)path;
@end
@interface NSBundle : NSObject
+ (NSBundle *)mainBundle;
+ (NSBundle *)bundleForClass:(Class)c;
- (NSString *)pathForResource:(NSString *)n ofType:(NSString *)t;
- (NSString *)resourcePath;
- (NSString *)executablePath;
- (NSString *)bundlePath;
- (NSURL *)URLForResource:(NSString *)n withExtension:(NSString *)e;
@end
@interface NSProcessInfo : NSObject
+ (NSProcessInfo *)processInfo;
- (NSUInteger)processorCount;
- (NSString *)operatingSystemVersionString;
- (BOOL)isOperatingSystemAtLeastVersion:(struct { NSInteger a, b, c; })v;
@end
@interface NSThread : NSObject
+ (BOOL)isMainThread;
@end
@interface NSAutoreleasePool : NSObject
- (void)drain;
@end
@interface NSLock : NSObject
- (void)lock;
- (void)unlock;
@end
@interface NSNotificationCenter : NSObject
+ (NSNotificationCenter *)defaultCenter;
- (void)addObserver:(id)o selector:(SEL)s name:(NSString *)n object:(id)obj;
- (void)removeObserver:(id)o;
@end
@interface NSCoder : NSObject
@end
void NSLog(NSString *fmt, ...);
NSString *NSStringFromSize(NSSize s);
NSString *NSStringFromRect(NSRect r);
typedef double CFTimeInterval;
#define NSStringEncodingConversionAllowLossy 1
typedef struct CGColorSpace *CGColorSpaceRef;
extern const CFStringRef kCGColorSpaceSRGB;
extern const CFStringRef kCGColorSpaceExtendedLinearSRGB;
extern const CFStringRef kCGColorSpaceITUR_2100_PQ;
CGColorSpaceRef CGColorSpaceCreateWithName(CFStringRef n);
void CGColorSpaceRelease(CGColorSpaceRef c);
#endif
