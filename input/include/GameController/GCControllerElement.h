//
//  GCControllerElement.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import "GameController.h"

/**
 Every controller element knows which collection it belongs to and whether its input value is analog or digital.
 */
GAMECONTROLLER_EXPORT
@interface GCControllerElement : NSObject

/**
 Each element can be part of a wider collection of inputs that map to a single logical element. A directional pad (dpad)
 is a logical collection of two axis inputs and thus each axis belongs to the same collection element - the dpad.
 */
#if !__has_feature(objc_arc)
@property (assign, readonly) GCControllerElement *collection;
#else
@property (weak, readonly) GCControllerElement *collection;
#endif

/**
 Check if the element can support more than just digital values, such as decimal ranges between 0 and 1.
 */
@property (readonly, getter = isAnalog) BOOL analog;

@end
