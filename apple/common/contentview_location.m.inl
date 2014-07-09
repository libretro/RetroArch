#include <CoreLocation/CoreLocation.h>

static CLLocationManager *locationManager;
static bool locationChanged;
static CLLocationDegrees currentLatitude;
static CLLocationDegrees currentLongitude;
static CLLocationAccuracy currentHorizontalAccuracy;
static CLLocationAccuracy currentVerticalAccuracy;

- (bool)onLocationHasChanged
{
   bool hasChanged = locationChanged;
    
   if (hasChanged)
      locationChanged = false;
    
   return hasChanged;
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation
{
    locationChanged = true;
    currentLatitude = newLocation.coordinate.latitude;
    currentLongitude = newLocation.coordinate.longitude;
    currentHorizontalAccuracy = newLocation.horizontalAccuracy;
    currentVerticalAccuracy = newLocation.verticalAccuracy;
    RARCH_LOG("didUpdateToLocation - latitude %f, longitude %f\n", (float)currentLatitude, (float)currentLongitude);
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray *)locations
{
    CLLocation *location = (CLLocation*)[locations objectAtIndex:([locations count] - 1)];
    
    locationChanged = true;
    currentLatitude  = [location coordinate].latitude;
    currentLongitude = [location coordinate].longitude;
    currentHorizontalAccuracy = location.horizontalAccuracy;
    currentVerticalAccuracy = location.verticalAccuracy;
    RARCH_LOG("didUpdateLocations - latitude %f, longitude %f\n", (float)currentLatitude, (float)currentLongitude);
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error
{
   RARCH_LOG("didFailWithError - %s\n", [[error localizedDescription] UTF8String]);
}

- (void)locationManagerDidPauseLocationUpdates:(CLLocationManager *)manager
{
   RARCH_LOG("didPauseLocationUpdates\n");
}

- (void)locationManagerDidResumeLocationUpdates:(CLLocationManager *)manager
{
   RARCH_LOG("didResumeLocationUpdates\n");
}

- (void)onLocationInit
{
    // Create the location manager if this object does not
    // already have one.
    
    if (locationManager == nil)
        locationManager = [[CLLocationManager alloc] init];
    locationManager.delegate = self;
    locationManager.desiredAccuracy = kCLLocationAccuracyBest;
    locationManager.distanceFilter = kCLDistanceFilterNone;
    [locationManager startUpdatingLocation];
}
