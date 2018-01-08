//
//  GCGamepad.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import "GameController.h"

@class GCController;
@class GCGamepadSnapshot;

/**
 Standard Gamepad profile.

 All controller profiles provide a base level of information about the controller they belong to.

 A profile maps the hardware notion of a controller into a logical controller. One that a developer can design for
 and depend on, no matter the underlying hardware.
 */
GAMECONTROLLER_EXPORT
@interface GCGamepad : NSObject

/**
 A profile keeps a reference to the controller that this profile is mapping input from.
 */
#if !__has_feature(objc_arc)
@property (readonly, assign) GCController *controller;
#else
@property (readonly, weak) GCController *controller;
#endif

/**
 Set this block if you want to be notified when a value on a element changed. If multiple elements have changed this block will be called
 for each element that changed. As elements in a collection, such as the axis in a dpad, tend to change at the same time and thus
 will only call this once with the collection as the element.

 @param gamepad this gamepad that is being used to map the raw input data into logical values on controller elements such as the dpad or the buttons.
 @param element the element that has been modified.
 */
typedef void (^GCGamepadValueChangedHandler)(GCGamepad *gamepad, GCControllerElement *element);
@property (copy) GCGamepadValueChangedHandler valueChangedHandler;

/**
 Polls the state vector of the controller and saves it to a snapshot. The snapshot is stored in a device independent
 format that can be serialized and used at a later date. This is useful for features such as quality assurance,
 save game or replay functionality among many.

 If your application is heavily multithreaded this may also be useful to guarantee atomicity of input handling as
 a snapshot will not change based on user input once it is taken.
 */
- (GCGamepadSnapshot *)saveSnapshot;

/**
 Required to be analog in the Standard profile. All the elements of this directional input are thus analog.
 */
@property (readonly) GCControllerDirectionPad *dpad;

/**
 All face buttons are required to be analog in the Standard profile. These must be arranged
 in the diamond pattern given below:

   Y
  / \
 X   B
  \ /
   A

 */
@property (readonly) GCControllerButtonInput *buttonA;
@property (readonly) GCControllerButtonInput *buttonB;
@property (readonly) GCControllerButtonInput *buttonX;
@property (readonly) GCControllerButtonInput *buttonY;

/**
 Shoulder buttons are required to be analog inputs.
 */
@property (readonly) GCControllerButtonInput *leftShoulder;
/**
 Shoulder buttons are required to be analog inputs.
 */
@property (readonly) GCControllerButtonInput *rightShoulder;

@end
