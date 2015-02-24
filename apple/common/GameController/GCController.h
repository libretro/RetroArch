//
//  GCController.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import <GameController/GameController.h>

/**
 Use these constants with NSNotificationCenter to listen to connection and disconnection events.
 
 Use GCControllerDidConnectNotification for observing connections of controllers.
 Use GCControllerDidDisconnectNotification for observing disconnections of controllers.
 
 Connections and disconnections of controllers will also be reflected in the controllers array
 of the GCController class.
 
 The 'object' property of the notification will contain the GCController that was connected or disconnected.
 For example:
 
 - (void)controllerDidConnect:(NSNotification *)note {
 
 GCController *controller = note.object;
 
 ....
 }
 
 @see NSNotificationCenter
 @see GCController.controllers
 */
GAMECONTROLLER_EXTERN NSString *const GCControllerDidConnectNotification;
GAMECONTROLLER_EXTERN NSString *const GCControllerDidDisconnectNotification;

/**
 This is the player index that a connected controller will have if it has never been assigned a player index on the current system.
 Controllers retain the player index they have been assigned between game sessions, so if you wish to unset the player index of a
 controller set it back to this value.
 */
enum {
    GCControllerPlayerIndexUnset = -1,
};

/**
 Controllers are available to an application that links to GameController.framework. There are 2 ways to access controllers
 paired to the system, adopt both to ensure the best user experience:
 
 1: Querying for the the current array or controllers using [GCController controllers].
 2: Registering for Connection/Disconnection notifications from NSNotificationCenter.
 
 Only controllers that support one of the allowed profiles, such as GCGamepad, will be enumerated. Check for the profile
 supported before using a controller in your application. Ignore a controller that doesn't support a profile that suits
 your application, as the user will expect their controller to either be fully supported or not supported at all.
  */
GAMECONTROLLER_EXPORT
@interface GCController : NSObject

/**
 Set this block to be notified when a user intendeds to suspend or resume the current game state. A controller will have a button
 dedicated to suspending and resuming play and invoking context sensitive actions. During event handling the system will
 notify the application using this block such that the application can handle the suspension and resumption from the given controller.
 
 Use this to implement your canonical transition to a pause menu for example if that is your application's desired handling
 of suspension in play. You may pause and resume base don game state as well so the event is only called each time the
 pause/resume button is pressed.
 
 @param controller the controller that is suspending or resuming play.
 */
@property (copy) void (^controllerPausedHandler)(GCController *controller);

/**
 A vendor supplied name. May be nil, and is not guaranteed to be unique. This should not be used as a key in a dictionary,
 but simply as a way to present some basic information about the controller in testing or to the user.
 */
@property (readonly, copy) NSString *vendorName;

/**
 A controller may be form fitting or otherwise closely attached to the device. This closeness to other inputs on the device
 may suggest that interaction with the device may use other inputs easily. This is presented to developers to allow them to
 make informed descisions about UI and interactions to choose for their game in this situation.
 */
@property (readonly, getter = isAttachedToDevice) BOOL attachedToDevice;

/**
 A player index for the controller, defaults to GCControllerPlayerIndexUnset, unless the controller previously had
 a player index assigned to it on the current user's system.
 
 This can be set both for the application to keep track of controllers and as a signal to make a controller display a player
 index on a set of LEDs or some other mechanism.
 
 A controller is not guranteed to have a visual display of the playerIndex, but the API will keep track of the playerIndex
 for a controller while connected and in between being disconnected and connected again. Thus playerIndex persists for a controller
 with regards to a system. This makes it useful for persisting player-controller assignments across game sessions.
 
 Negative values less than GCControllerPlayerIndexUnset will just map back to GCControllerPlayerIndexUnset when read back.
 */
@property (nonatomic) NSInteger playerIndex;

/**
 Gets the profile for the controller that suits current application.
 
 There are only two supported profiles. Each controller may be able to map its inputs into all profiles or just one kind of
 profile. Query for the controller profile that suits your game, the simplest kind will be supported by the broadest variety
 of controllers. A controller supporting the Extended Gamepad profile for example supports the Gamepad profile and more.
 As such it can always be used just in the Gamepad profile if that suits the game.
 
 A physical controller that supports a profile must support it completely. That means that all buttons and axis inputs must
 be valid inputs that a developer can utilize.
 
 If a controller does not support the given profile the returned value will be nil. Use this to filter controllers if the
 application requires a specific kind of profile.
 */
@property (retain, readonly) GCGamepad *gamepad;
@property (retain, readonly) GCExtendedGamepad *extendedGamepad;

/**
 Get a list of controllers currently attached to the system.
 
 @see GCControllerDidConnectNotification
 @see GCControllerDidDisconnectNotification
 */
+ (NSArray *)controllers;

/**
 Start discovery of new wireless controllers that are discoverable. This is an asynchronous and the supplied completionHandler
 will get called once no more devices can be found. If there are already multiple controllers available for use, there
 may be little reason to automatically start discovery of new wireless controllers. In this situation it may be best to
 allow the user to start discovery manually via in-game UI.
 
 Once discovery has started new controllers will notify themselves as connected via GCControllerDidConnectNotification.
 As the notification arrives the controller is also available in the controllers array.

 The completionHandler could be used to update UI and/or game state to indicate that no more controllers will be found
 and the current set of controllers is what is available for use in the game.
 
 If a completionHandler was provided, it will be called once when discovery stops. Either from an explicit call to
 stopWirelessControllerDiscovery or from timing out or stopping in its natural course of operation. Thus the
 completionHandler will at most be called once per call to startWirelessControllerDiscoveryWithCompletionHandler:.
 
 The completionHandler may also not get called at all, if for example startWirelessControllerDiscoveryWithCompletionHandler:
 is called multiple times during dicovery. For this case the net effect is that the completionHandler is replaced with each call
 and only the last one set before discovery stops will be called.
 
 @param completionHandler an optional handler that is called when discovery stops. (may be nil, in which case you will not be notified when discovery stops)
 @see stopWirelessControllerDiscovery
 @see controllers
 */
+ (void)startWirelessControllerDiscoveryWithCompletionHandler:(void (^)(void))completionHandler;

/**
 If no more controllers are needed, depending on game state or number of controllers supported by a game, the discovery
 process can be stopped. Calling stopWirelessControllerDiscovery when no discovery is currently in progress will return
 immediately without any effect, thus it is safe to call even if the completionHandler of
 startWirelessControllerDiscoveryWithCompletionHandler: has been called.
 
 @see startWirelessControllerDiscoveryWithCompletionHandler:
 */
+ (void)stopWirelessControllerDiscovery;

@end

