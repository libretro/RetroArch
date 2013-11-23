package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.hardware.Camera;
import android.os.Build;
import android.util.Log;

/**
 * {@link NativeActivity} where all emulation takes place.
 */
public final class RetroActivity extends NativeActivity
{
	private Camera mCamera;
	private long lastTimestamp = 0;
	private SurfaceTexture texture;
	private boolean updateSurface = true;

	@Override
	public void onDestroy()
	{
		UserPreferences.readbackConfigFile(this);
	}

	@Override
	public void onLowMemory()
	{
	}

	@Override
	public void onTrimMemory(int level)
	{
	}

	@Override
	public void onBackPressed()
	{
		Log.i("RetroActivity", "onBackKeyPressed");
	}

	//
	// Camera API
	//

	public void onCameraStart()
	{
		mCamera.startPreview();
	}

	public void onCameraStop()
	{
		mCamera.stopPreview();
	}

	public void onCameraInit()
	{
		mCamera = Camera.open();
	}

	public void onCameraFree()
	{
		mCamera.release();
	}

	@SuppressLint("NewApi")
	public boolean onCameraPoll()
	{
		if (texture == null)
		{
			Log.i("RetroActivity", "no texture");
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

	@SuppressLint("NewApi")
	public void onCameraTextureInit(int gl_texid)
	{
		texture = new SurfaceTexture(gl_texid);
		texture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
	}

	@SuppressLint("NewApi")
	private final OnFrameAvailableListener onCameraFrameAvailableListener = new OnFrameAvailableListener()
	{
		@Override
		public void onFrameAvailable(SurfaceTexture surfaceTexture)
		{
			updateSurface = true;
		}
	};

	@SuppressLint("NewApi")
	public void onCameraSetTexture(int gl_texid) throws IOException
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			if (texture == null)
				onCameraTextureInit(gl_texid);
			mCamera.setPreviewTexture(texture);
		}
		else
		{
			mCamera.setPreviewDisplay(null);
		}
	}
}
