//
//  GCGamepadSnapshot.h
//  GameController
//
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#import "GameController.h"

/**
 A GCGamepadSnapshot snapshot is a concrete GCGamepad implementation. It can be used directly in an
 application to implement controller input replays. It is also returned as the result of polling
 a controller.

 The current snapshotData is readily available to access as NSData. A developer can serialize this to any
 destination necessary using the NSData API.

 The data contains some version of a GCGamepadSnapShotData structure.

 @see -[GCGamepad saveSnapshot]
 */
GAMECONTROLLER_EXPORT
@interface GCGamepadSnapshot : GCGamepad
@property (copy) NSData *snapshotData;

- (instancetype)initWithSnapshotData:(NSData *)data;
- (instancetype)initWithController:(GCController *)controller snapshotData:(NSData *)data;

@end

#pragma pack(push, 1)
typedef struct {
    // Standard information
    uint16_t version; //0x0100
    uint16_t size;    //sizeof(GCGamepadSnapShotDataV100) or larger

    // Standard gamepad data
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

} GCGamepadSnapShotDataV100;
#pragma pack(pop)

/**Fills out a v100 snapshot from any compatible NSData source

 @return NO if data is nil, snapshotData is nil or the contents of data does not contain a compatible snapshot. YES for all other cases.
 */
GAMECONTROLLER_EXPORT
BOOL GCGamepadSnapShotDataV100FromNSData(GCGamepadSnapShotDataV100 *snapshotData, NSData *data);

/**Creates an NSData object from a v100 snapshot.
 If the version and size is not set in the snapshot the data will automatically have version 0x100 and sizeof(GCGamepadSnapShotDataV100) set as the values implicitly.

 @return nil if the snapshot is NULL, otherwise an NSData instance compatible with GCGamepadSnapshot.snapshotData
 */
GAMECONTROLLER_EXPORT
NSData *NSDataFromGCGamepadSnapShotDataV100(GCGamepadSnapShotDataV100 *snapshotData);
