/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025      - Joseph Mattiello
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import <CoreLocation/CoreLocation.h>
#include "../../location_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
@interface CoreLocationManager : NSObject <CLLocationManagerDelegate>
@property (strong, nonatomic) CLLocationManager *locationManager;
@property (assign) double latitude;
@property (assign) double longitude;
@property (assign) bool authorized;
@end

@implementation CoreLocationManager

+ (instancetype)sharedInstance {
    static CoreLocationManager *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[CoreLocationManager alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _locationManager = [[CLLocationManager alloc] init];
        _locationManager.delegate = self;
        _locationManager.desiredAccuracy = kCLLocationAccuracyBest;
        _authorized = false;
    }
    return self;
}

- (void)requestAuthorization {
    CLAuthorizationStatus status;
    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
        status = [_locationManager authorizationStatus];
    else
        status = [CLLocationManager authorizationStatus];

    if (status == kCLAuthorizationStatusNotDetermined)
        [_locationManager requestWhenInUseAuthorization];
    else
        [self locationManager:_locationManager didChangeAuthorizationStatus:status];
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations {
    CLLocation *location = [locations lastObject];
    self.latitude = location.coordinate.latitude;
    self.longitude = location.coordinate.longitude;
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error {
    RARCH_WARN("[LOCATION]: Location manager failed with error: %s\n", error.description.UTF8String);
    NSLog(@"Location manager failed with error: %@", error);
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
#if TARGET_OS_OSX
    if (@available(macOS 10.12, *))
        self.authorized = (status == kCLAuthorizationStatusAuthorizedAlways);
#elif TARGET_OS_IPHONE
    if (@available(iOS 8.0, tvOS 9.0, *))
        self.authorized = (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
                           status == kCLAuthorizationStatusAuthorizedAlways);
#endif
#if !TARGET_OS_TV
    if (self.authorized)
        [_locationManager startUpdatingLocation];
#endif
}

@end

typedef struct corelocation {
    CoreLocationManager *manager;
} corelocation_t;

static void *corelocation_init(void) {
    corelocation_t *corelocation = (corelocation_t*)calloc(1, sizeof(*corelocation));
    if (!corelocation)
        return NULL;

    corelocation->manager = [CoreLocationManager sharedInstance];
    [corelocation->manager requestAuthorization];

    return corelocation;
}

static void corelocation_free(void *data) {
    corelocation_t *corelocation = (corelocation_t*)data;
    if (!corelocation)
        return;

    free(corelocation);
}

static bool corelocation_start(void *data) {
    corelocation_t *corelocation = (corelocation_t*)data;
    if (!corelocation || !corelocation->manager.authorized)
        return false;

#if !TARGET_OS_TV
    [corelocation->manager.locationManager startUpdatingLocation];
#endif
    return true;
}

static void corelocation_stop(void *data) {
    corelocation_t *corelocation = (corelocation_t*)data;
    if (!corelocation)
        return;

    [corelocation->manager.locationManager stopUpdatingLocation];
}

static bool corelocation_get_position(void *data, double *lat, double *lon,
                                     double *horiz_accuracy, double *vert_accuracy) {
    corelocation_t *corelocation = (corelocation_t*)data;
    if (!corelocation || !corelocation->manager.authorized)
        return false;

#if TARGET_OS_TV
    CLLocation *location = [corelocation->manager.locationManager location];
    *lat = location.coordinate.latitude;
    *lon = location.coordinate.longitude;
#else
    *lat = corelocation->manager.latitude;
    *lon = corelocation->manager.longitude;
#endif
    *horiz_accuracy = 0.0; // CoreLocation doesn't provide this directly
    *vert_accuracy = 0.0;  // CoreLocation doesn't provide this directly
    return true;
}

static void corelocation_set_interval(void *data, unsigned interval_ms,
                                    unsigned interval_distance) {
    corelocation_t *corelocation = (corelocation_t*)data;
    if (!corelocation)
        return;

    corelocation->manager.locationManager.distanceFilter = interval_distance;
    corelocation->manager.locationManager.desiredAccuracy = kCLLocationAccuracyBest;
}

location_driver_t location_corelocation = {
    corelocation_init,
    corelocation_free,
    corelocation_start,
    corelocation_stop,
    corelocation_get_position,
    corelocation_set_interval,
    "corelocation",
};
