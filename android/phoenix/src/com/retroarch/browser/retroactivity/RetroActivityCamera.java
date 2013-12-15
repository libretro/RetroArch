package com.retroarch.browser.retroactivity;

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

/**
 * Class which provides {@link Camera} functionality
 * to {@link RetroActivityFuture}.
 */
@SuppressLint("NewApi")
public class RetroActivityCamera extends RetroActivityCommon
{
	private Camera mCamera = null;
	private long lastTimestamp = 0;
	private SurfaceTexture texture;
	private boolean updateSurface = true;
	private boolean camera_service_running = false;

	/**
	 * Executed when the {@link Camera}
	 * is staring to capture.
	 */
	public void onCameraStart()
	{
		if (camera_service_running)
			return;
		
		mCamera.startPreview();
		camera_service_running = true;
	}

	/**
	 * Executed when the {@link Camera} is done capturing.
	 * <p>
	 * Note that this does not release the currently held 
	 * {@link Camera} instance and must be freed by calling
	 * {@link #onCameraFree}
	 */
	public void onCameraStop()
	{
		if (!camera_service_running)
			return;
		
		mCamera.stopPreview();
		camera_service_running = false;
	}

	/**
	 * Releases the currently held {@link Camera} instance.
	 */
	public void onCameraFree()
	{
		onCameraStop();
		mCamera.release();
	}

	/**
	 * Initializes the camera for use.
	 */
	public void onCameraInit()
	{
		if (mCamera != null)
			return;
		
		mCamera = Camera.open();
	}

	/**
	 * Polls the camera for updates to the {@link SurfaceTexture}.
	 * 
	 * @return true if polling was successful, false otherwise.
	 */
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

	/**
	 * Initializes the {@link SurfaceTexture} used by the
	 * {@link Camera} with a given OpenGL texure ID.
	 * 
	 * @param gl_texid texture ID to initialize the 
	 *        {@link SurfaceTexture} with.
	 */
	public void onCameraTextureInit(int gl_texid)
	{
		texture = new SurfaceTexture(gl_texid);
		texture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
	}

	/**
	 * Sets the {@link Camera} texture with the texture represented
	 * by the given OpenGL texture ID.
	 * 
	 * @param gl_texid     The texture ID representing the texture to set the camera to.
	 * @throws IOException If setting the texture fails.
	 */
	public void onCameraSetTexture(int gl_texid) throws IOException
	{
		if (texture == null)
			onCameraTextureInit(gl_texid);

		mCamera.setPreviewTexture(texture);
	}

	private final OnFrameAvailableListener onCameraFrameAvailableListener = new OnFrameAvailableListener()
	{
		@Override
		public void onFrameAvailable(SurfaceTexture surfaceTexture)
		{
			updateSurface = true;
		}
	};

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
		}
		else // Otherwise, turn off camera updates
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
}
