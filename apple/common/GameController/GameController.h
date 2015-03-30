//
//  GameController.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#include <AvailabilityMacros.h>

#ifdef __cplusplus
#define GAMECONTROLLER_EXTERN		extern "C" __attribute__((visibility ("default")))
#else
#define GAMECONTROLLER_EXTERN	        extern __attribute__((visibility ("default")))
#endif

#ifndef __IPHONE_7_0
#define GAMECONTROLLER_EXPORT
#else
#define GAMECONTROLLER_EXPORT NS_CLASS_AVAILABLE(10_9, 7_0)
#endif

#import "GCControllerElement.h"

#import "GCControllerAxisInput.h"
#import "GCControllerButtonInput.h"
#import "GCControllerDirectionPad.h"

#import "GCGamepad.h"
#import "GCGamepadSnapshot.h"

#import "GCExtendedGamepad.h"
#import "GCExtendedGamepadSnapshot.h"

#import "GCController.h"
