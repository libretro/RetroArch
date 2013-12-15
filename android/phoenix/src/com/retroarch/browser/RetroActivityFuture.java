package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.hardware.Camera;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

//For Android 3.0 and up

@SuppressLint("NewApi")
public final class RetroActivityFuture extends RetroActivityCommon
{
	private Camera mCamera = null;
	private long lastTimestamp = 0;
	private SurfaceTexture texture;
	private boolean updateSurface = true;
	private boolean camera_service_running = false;

	public void onCameraStart()
	{
		if (camera_service_running)
			return;
		
		mCamera.startPreview();
		camera_service_running = true;
	}

	public void onCameraStop()
	{
		if (!camera_service_running)
			return;
		
		mCamera.stopPreview();
		camera_service_running = false;
	}
	
	public void onCameraFree()
	{
		onCameraStop();
		mCamera.release();
	}

	public void onCameraInit()
	{
		if (mCamera != null)
			return;
		
		mCamera = Camera.open();
	}

	public boolean onCameraPoll()
	{
		if (!camera_service_running)
			return false;
		
		if (texture == null)
		{
			Log.i("RetroActivity", "No texture");
			return true;
		}
		else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH)
		{
			if (updateSurface)
			{
				texture.updateTexImage();
			}
			
			long newTimestamp = texture.getTimestamp();
			
			if (newTimestamp != lastTimestamp)
			{
				lastTimestamp = newTimestamp;
				return true;
			}
			
			return false;
		}

		return true;
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
        // Save the current setting for updates
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putBoolean("CAMERA_UPDATES_ON", false);
		edit.commit();
		
		camera_service_running = false;
		
		super.onCreate(savedInstanceState);
	}
	
	@Override
	public void onPause()
	{
        // Save the current setting for updates
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		edit.putBoolean("CAMERA_UPDATES_ON", camera_service_running);
		edit.commit();
		
		onCameraStop();
		super.onPause();
	}
	
	@Override
	public void onResume()
	{
		SharedPreferences prefs = UserPreferences.getPreferences(this);
		SharedPreferences.Editor edit = prefs.edit();
		
        /*
         * Get any previous setting for camera updates
         * Gets "false" if an error occurs
         */
        if (prefs.contains("CAMERA_UPDATES_ON"))
        {
            camera_service_running = prefs.getBoolean("CAMERA_UPDATES_ON", false);
            if (camera_service_running)
            {
            	onCameraStart();
            }
        // Otherwise, turn off camera updates
        }
        else
        {
            edit.putBoolean("CAMERA_UPDATES_ON", false);
            edit.commit();
            camera_service_running = false;
        }
		super.onResume();
	}
	
	@Override
	public void onDestroy()
	{
		onCameraFree();
		super.onDestroy();
	}
	
	@Override
	public void onStop()
	{
		onCameraStop();
		super.onStop();
	}

	public void onCameraTextureInit(int gl_texid)
	{
		texture = new SurfaceTexture(gl_texid);
		texture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
	}

	private final OnFrameAvailableListener onCameraFrameAvailableListener = new OnFrameAvailableListener()
	{
		@Override
		public void onFrameAvailable(SurfaceTexture surfaceTexture)
		{
			updateSurface = true;
		}
	};

	public void onCameraSetTexture(int gl_texid) throws IOException
	{
		if (texture == null)
			onCameraTextureInit(gl_texid);

		mCamera.setPreviewTexture(texture);
	}

	
}
