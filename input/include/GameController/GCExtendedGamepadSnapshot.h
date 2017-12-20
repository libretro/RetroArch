//
//  GCExtendedGamepadSnapshot.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import "GameController.h"

/**
 A GCExtendedGamepadSnapshot snapshot is a concrete GCExtendedGamepad implementation. It can be used directly in an
 application to implement controller input replays. It is also returned as the result of polling a controller.

 The current snapshotData is readily available to access as NSData. A developer can serialize this to any
 destination necessary using the NSData API.

 The data contains some version of a GCExtendedGamepadSnapShotData structure.

 @see -[GCExtendedGamepad saveSnapshot]
 */
GAMECONTROLLER_EXPORT
@interface GCExtendedGamepadSnapshot : GCExtendedGamepad
@property (copy) NSData *snapshotData;

- (instancetype)initWithSnapshotData:(NSData *)data;
- (instancetype)initWithController:(GCController *)controller snapshotData:(NSData *)data;

@end

#pragma pack(push, 1)
typedef struct {
    // Standard information
    uint16_t version; //0x0100
    uint16_t size;    //sizeof(GCExtendedGamepadSnapShotDataV100) or larger

    // Extended gamepad data
    // Axes in the range [-1.0, 1.0]
    float_t dpadX;
    float_t dpadY;

    // Buttons in the range [0.0, 1.0]
    float_t buttonA;
    float_t buttonB;
    float_t buttonX;
    float_t buttonY;
    float_t leftShoulder;
    float_t rightShoulder;

    // Axes in the range [-1.0, 1.0]
    float_t leftThumbstickX;
    float_t leftThumbstickY;
    float_t rightThumbstickX;
    float_t rightThumbstickY;

    // Buttons in the range [0.0, 1.0]
    float_t leftTrigger;
    float_t rightTrigger;

} GCExtendedGamepadSnapShotDataV100;
#pragma pack(pop)

/**Fills out a v100 snapshot from any compatible NSData source

 @return NO if data is nil, snapshotData is nil or the contents of data does not contain a compatible snapshot. YES for all other cases.
 */
GAMECONTROLLER_EXPORT
BOOL GCExtendedGamepadSnapShotDataV100FromNSData(GCExtendedGamepadSnapShotDataV100 *snapshotData, NSData *data);

/**Creates an NSData object from a v100 snapshot.
 If the version and size is not set in the snapshot the data will automatically have version 0x100 and sizeof(GCExtendedGamepadSnapShotDataV100) set as the values implicitly.

 @return nil if the snapshot is NULL, otherwise an NSData instance compatible with GCExtendedGamepadSnapshot.snapshotData
 */
GAMECONTROLLER_EXPORT
NSData *NSDataFromGCExtendedGamepadSnapShotDataV100(GCExtendedGamepadSnapShotDataV100 *snapshotData);
