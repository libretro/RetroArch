//
//  main.m
//  RetroArch
//
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <UIKit/UIKit.h>

#define GSEVENT_TYPE 2
#define GSEVENT_FLAGS 12
#define GSEVENTKEY_KEYCODE 15
#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

NSString *const GSEventKeyDownNotification = @"GSEventKeyDownHackNotification";
NSString *const GSEventKeyUpNotification = @"GSEventKeyUpHackNotification";

@interface RApplication : UIApplication
@end

@implementation RApplication

#define HWKB_HACK
#ifdef HWKB_HACK // Disabled pending further testing
// Stolen from: http://nacho4d-nacho4d.blogspot.com/2012/01/catching-keyboard-events-in-ios.html
- (void)sendEvent:(UIEvent *)event
{
   [super sendEvent:event];

   if ([event respondsToSelector:@selector(_gsEvent)])
   {
      int* eventMem = (int *)(void*)CFBridgingRetain([event performSelector:@selector(_gsEvent)]);
      int eventType = eventMem ? eventMem[GSEVENT_TYPE] : 0;
       
      if (eventMem && (eventType == GSEVENT_TYPE_KEYDOWN || eventType == GSEVENT_TYPE_KEYUP))
      {
         // Read keycode from GSEventKey
         int tmp = eventMem[GSEVENTKEY_KEYCODE];
         UniChar *keycode = (UniChar *)&tmp;

         // Post notification
         NSDictionary *inf = [[NSDictionary alloc] initWithObjectsAndKeys:
                             [NSNumber numberWithShort:keycode[0]], @"keycode",
                             nil];
                   
         [[NSNotificationCenter defaultCenter]
             postNotificationName:(eventType == GSEVENT_TYPE_KEYDOWN) ? GSEventKeyDownNotification : GSEventKeyUpNotification
             object:nil userInfo:inf];
      }
       
      CFBridgingRelease(eventMem);
   }
}
#endif

@end

int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(argc, argv, NSStringFromClass([RApplication class]), NSStringFromClass([RetroArch_iOS class]));
    }
}

