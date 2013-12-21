package com.retroarch.browser.retroactivity;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesClient.ConnectionCallbacks;
import com.google.android.gms.common.GooglePlayServicesClient.OnConnectionFailedListener;
import com.google.android.gms.location.LocationClient;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.NativeActivity;
import android.content.IntentSender;
import android.content.SharedPreferences;
import android.location.Location;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

/**
 * Class that implements location-based functionality for
 * the {@link RetroActivityFuture} and {@link RetroActivityPast}
 * activities.
 */
public class RetroActivityLocation extends NativeActivity
implements ConnectionCallbacks, OnConnectionFailedListener, LocationListener
{
	/* LOCATION VARIABLES */
	private static int CONNECTION_FAILURE_RESOLUTION_REQUEST = 0;
	private LocationClient mLocationClient = null;
	private Location mCurrentLocation;

	// Define an object that holds accuracy and frequency parameters
	LocationRequest mLocationRequest = null;
	boolean mUpdatesRequested = false;
	boolean locationChanged = false;
	boolean location_service_running = false;
	double current_latitude  = 0.0;
	double current_longitude = 0.0;
	double current_accuracy  = 0.0;

	/*
	 * Called by Location Services when the request to connect the
	 * client finishes successfully. At this point, you can
	 * request the current location or start periodic updates
	 */
	@Override
	public void onConnected(Bundle dataBundle)
	{
        // Display the connection status
        Toast.makeText(this, "Connected", Toast.LENGTH_SHORT).show();
        location_service_running = true;

        // If already requested, start periodic updates
        if (mUpdatesRequested)
        {
                mLocationClient.requestLocationUpdates(mLocationRequest, this, null);
        }
        else
        {
                // Get last known location
                mCurrentLocation = mLocationClient.getLastLocation();
                locationChanged = true;
        }
	}

	/*
	 * Called by Location Services if the connection to the
	 * location client drops because of an error.
	 */
	@Override
	public void onDisconnected()
	{
        // Display the connection status
        Toast.makeText(this, "Disconnected. Please re-connect.", Toast.LENGTH_SHORT).show();
       
        // If the client is connected
        if (mLocationClient.isConnected())
        {
                /*
                 * Remove location updates for a listener.
                 * The current Activity is the listener, so
                 * the argument is "this".
                 */
                mLocationClient.removeLocationUpdates(this);
        }
       
        location_service_running = false;
	}

	/*
	 * Called by Location Services if the attempt to
	 * Location Services fails.
	 */
	@Override
	public void onConnectionFailed(ConnectionResult connectionResult)
	{
        /*
         * Google Play services can resolve some errors it detects.
         * If the error has a resolution, try sending an Intent to
         * start a Google Play services activity that can resolve
         * error.
         */
        if (connectionResult.hasResolution())
        {
                try
                {
                        // Start an Activity that tries to resolve the error
                        connectionResult.startResolutionForResult(this, CONNECTION_FAILURE_RESOLUTION_REQUEST);
                }
                catch (IntentSender.SendIntentException e)
                {
                        // Thrown if Google Play services cancelled the original PendingIntent
                        e.printStackTrace();
                }
        }
        else
        {
                /*
                 * If no resolution is available, display a dialog to the
                 * user with the error.
                 */
                //showErrorDialog(connectionResult.getErrorCode());
        }
	}

   /**
    * Sets the update interval at which location-based updates 
    * should occur
    */
   public void onLocationSetInterval(int update_interval_in_ms, int distance_interval)
   {
       // Use high accuracy
       mLocationRequest.setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY);

       if (update_interval_in_ms == 0)
               mLocationRequest.setInterval(5 * 1000); // 5 seconds
       else
               mLocationRequest.setInterval(update_interval_in_ms);

       // Set the fastest update interval to 1 second
       mLocationRequest.setFastestInterval(1000);
   }

	/**
	 * Initializing methods for location based functionality.
	 */
	public void onLocationInit()
	{
        /*
         * Create a new location client, using the enclosing class to
         * handle callbacks.
         */
        if (mLocationClient == null)
                mLocationClient = new LocationClient(this, this, this);

        // Start with updates turned off
        mUpdatesRequested = false;

        // Create the LocationRequest object
        if (mLocationRequest == null)
                mLocationRequest = LocationRequest.create();

        onLocationSetInterval(0, 0);
	}


	/**
	 * Executed upon starting the {@link LocationClient}.
	 */
	public void onLocationStart()
	{
		if (location_service_running)
			return;

		// Connect the client.
		mLocationClient.connect();
	}

	/**
	 * Free up location services resources.
	 */
	public void onLocationFree()
	{
		/* TODO/FIXME */
	}

	/**
	 * Executed upon stopping the location client.
	 * Does nothing if called when the client is not started.
	 */
	public void onLocationStop()
	{
		if (!location_service_running)
			return;

		// Disconnecting the client invalidates it.
		mLocationClient.disconnect();
	}

	/**
	 * Gets the latitude at the current location in degrees.
	 * 
	 * @return the latitude at the current location.
	 */
	public double onLocationGetLatitude()
	{
		return mCurrentLocation.getLatitude();
	}

	/**
	 * Gets the longitude at the current location in degrees.
	 * 
	 * @return the longitude at the current location.
	 */
	public double onLocationGetLongitude()
	{
		return mCurrentLocation.getLongitude();
	}
	
	/*
	 * Gets the horizontal accuracy of the current location 
	 * in meters. (NOTE: There seems to be no vertical accuracy
	 * for a given location with the Android location API)
	 * 
	 * @return the horizontal accuracy of the current position.
	 */
	public double onLocationGetHorizontalAccuracy()
	{
		return mCurrentLocation.getAccuracy();
	}
	
	/*
	 * Tells us whether the location listener callback has
	 * updated the current location since the last time
	 * we polled.
	 */
	public boolean onLocationHasChanged()
	{
		boolean hasChanged = locationChanged;
		
		// Reset flag
		if (hasChanged)
			locationChanged = false;
		
		return hasChanged;
	}

	// Define the callback method that receives location updates
	@Override
	public void onLocationChanged(Location location)
	{
		if (!location_service_running)
			return;
		
		locationChanged = true;
		mCurrentLocation = location;

		// Report to the UI that the location was updated
		String msg = "Updated Location: " + location.getLatitude() + ", " + location.getLongitude();
		Log.i("RetroArch GPS", msg);
		//Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
	}

	@Override
	public void onPause()
	{
		// Save the current setting for updates
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putBoolean("LOCATION_UPDATES_ON", mUpdatesRequested);
		edit.commit();

		super.onPause();
	}

	@Override
	public void onResume()
	{
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();

		/*
		 * Get any previous setting for location updates
		 * Gets "false" if an error occurs
		 */
		if (prefs.contains("LOCATION_UPDATES_ON"))
		{
			mUpdatesRequested = prefs.getBoolean("LOCATION_UPDATES_ON", false);
			if (mUpdatesRequested)
				location_service_running = true;
		}
		else // Otherwise, turn off location updates
		{
			edit.putBoolean("LOCATION_UPDATES_ON", false);
			edit.commit();
			location_service_running = false;
		}

		super.onResume();
	}

	@Override
	public void onStop()
	{
		onLocationStop();
		super.onStop();
	}
}
