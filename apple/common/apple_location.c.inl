typedef struct apple_location
{
	void *empty;
} applelocation_t;

static void *apple_location_init(void)
{
	applelocation_t *applelocation = (applelocation_t*)calloc(1, sizeof(applelocation_t));
	if (!applelocation)
		return NULL;
    
   [[RAGameView get] onLocationInit];
	
	return applelocation;
}

static void apple_location_set_interval(void *data, unsigned interval_update_ms, unsigned interval_distance)
{
   (void)data;
	
   locationManager.distanceFilter = interval_distance ? interval_distance : kCLDistanceFilterNone;
}

static void apple_location_free(void *data)
{
	applelocation_t *applelocation = (applelocation_t*)data;
	
   /* TODO - free location manager? */
	
	if (applelocation)
		free(applelocation);
	applelocation = NULL;
}

static bool apple_location_start(void *data)
{
	(void)data;
	
   [locationManager startUpdatingLocation];
	return true;
}

static void apple_location_stop(void *data)
{
	(void)data;
	
   [locationManager stopUpdatingLocation];
}

static bool apple_location_get_position(void *data, double *lat, double *lon, double *horiz_accuracy,
      double *vert_accuracy)
{
	(void)data;

   bool ret = [[RAGameView get] onLocationHasChanged];

   if (!ret)
      goto fail;
	
   *lat            = currentLatitude;
   *lon            = currentLongitude;
   *horiz_accuracy = currentHorizontalAccuracy;
   *vert_accuracy  = currentVerticalAccuracy;
   return true;

fail:
   *lat            = 0.0;
   *lon            = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

location_driver_t location_apple = {
	apple_location_init,
	apple_location_free,
	apple_location_start,
	apple_location_stop,
	apple_location_get_position,
	apple_location_set_interval,
	"apple",
};
