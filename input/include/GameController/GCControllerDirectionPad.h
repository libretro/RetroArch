//
//  GCControllerDirectionPad.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import "GameController.h"

/**
 A direction pad is a common grouping of 2 axis inputs where the input can also be interpreted as 2 sets of mutually exclusive button pairs.
 Only one button in each pair, {up, down} and {left, right}, can be pressed at any one time.
 */
GAMECONTROLLER_EXPORT
@interface GCControllerDirectionPad : GCControllerElement

/**
 Set this block if you want to be notified when the value on this axis changes.

 @param dpad the direction pad collection whose axis have been modified.
 @param xValue the value the x axis was set to at the time the valueChangedHandler fired.
 @param yValue the value the y axis was set to at the time the valueChangedHandler fired.
 */
typedef void (^GCControllerDirectionPadValueChangedHandler)(GCControllerDirectionPad *dpad, float xValue, float yValue);
@property (copy) GCControllerDirectionPadValueChangedHandler valueChangedHandler;

@property (readonly) GCControllerAxisInput *xAxis;
@property (readonly) GCControllerAxisInput *yAxis;

@property (readonly) GCControllerButtonInput *up;
@property (readonly) GCControllerButtonInput *down;

@property (readonly) GCControllerButtonInput *left;
@property (readonly) GCControllerButtonInput *right;

@end
