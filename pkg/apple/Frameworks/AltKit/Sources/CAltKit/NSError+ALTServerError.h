//
//  NSError+ALTServerError.h
//  AltStore
//
//  Created by Riley Testut on 5/30/19.
//  Copyright © 2019 Riley Testut. All rights reserved.
//

#import <Foundation/Foundation.h>

extern NSErrorDomain const AltServerErrorDomain;
extern NSErrorDomain const AltServerInstallationErrorDomain;
extern NSErrorDomain const AltServerConnectionErrorDomain;

extern NSErrorUserInfoKey const ALTUnderlyingErrorDomainErrorKey;
extern NSErrorUserInfoKey const ALTUnderlyingErrorCodeErrorKey;
extern NSErrorUserInfoKey const ALTProvisioningProfileBundleIDErrorKey;
extern NSErrorUserInfoKey const ALTAppNameErrorKey;
extern NSErrorUserInfoKey const ALTDeviceNameErrorKey;

typedef NS_ERROR_ENUM(AltServerErrorDomain, ALTServerError)
{
    ALTServerErrorUnderlyingError = -1,
    
    ALTServerErrorUnknown = 0,
    ALTServerErrorConnectionFailed = 1,
    ALTServerErrorLostConnection = 2,
    
    ALTServerErrorDeviceNotFound = 3,
    ALTServerErrorDeviceWriteFailed = 4,
    
    ALTServerErrorInvalidRequest = 5,
    ALTServerErrorInvalidResponse = 6,
    
    ALTServerErrorInvalidApp = 7,
    ALTServerErrorInstallationFailed = 8,
    ALTServerErrorMaximumFreeAppLimitReached = 9,
    ALTServerErrorUnsupportediOSVersion = 10,
    
    ALTServerErrorUnknownRequest = 11,
    ALTServerErrorUnknownResponse = 12,
    
    ALTServerErrorInvalidAnisetteData = 13,
    ALTServerErrorPluginNotFound = 14,
    
    ALTServerErrorProfileNotFound = 15,
    
    ALTServerErrorAppDeletionFailed = 16,
    
    ALTServerErrorRequestedAppNotRunning = 100,
};

typedef NS_ERROR_ENUM(AltServerConnectionErrorDomain, ALTServerConnectionError)
{
    ALTServerConnectionErrorUnknown,
    ALTServerConnectionErrorDeviceLocked,
    ALTServerConnectionErrorInvalidRequest,
    ALTServerConnectionErrorInvalidResponse,
    ALTServerConnectionErrorUSBMUXD,
    ALTServerConnectionErrorSSL,
    ALTServerConnectionErrorTimedOut,
};

NS_ASSUME_NONNULL_BEGIN

@interface NSError (ALTServerError)
@end

NS_ASSUME_NONNULL_END
