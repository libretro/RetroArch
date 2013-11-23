package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Build;
import android.util.Log;

public final class RetroActivity extends NativeActivity
{
	private Camera mCamera;
	private long lastTimestamp = 0;
	private SurfaceTexture texture;
	private Boolean updateSurface = true;
	private static Boolean onStateChanged = false;
	private static String fullpath = null;
	private static String libretro_path = null;
	private static String ime = null;
	
	public static String getIME()
	{
		return ime;
	}
	
	public static String getFullPath()
	{
		String tmp = fullpath;
		fullpath = null;
		return tmp;
	}
	
	public static String getCorePath()
	{
		String tmp = libretro_path;
		libretro_path = null;
		return tmp;
	}
	
	public static void onSetIME(String path)
	{
		ime = path;
	}
	
	public static void onSetCorePath(String path)
	{
		libretro_path = path;
	}
	
	public static void onSetFullPath(String path)
	{
		fullpath = path;
	}
	
	public static void onSetPendingStateChanges()
	{
		onStateChanged = true;
	}
	
	public boolean onPendingStateChanges()
	{
		Boolean state = onStateChanged;
		onStateChanged = false;
		return state;
	}

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

	public void onCameraFree()
	{
		mCamera.release();
	}
	
	@SuppressLint("NewApi")
	public void onCameraTextureInit(int gl_texid)
	{
		texture = new SurfaceTexture(gl_texid);
		texture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
	}
	
    @SuppressLint("NewApi")
	private SurfaceTexture.OnFrameAvailableListener onCameraFrameAvailableListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
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
		Intent startNewActivityOpen = new Intent(this, MainMenuActivity.class);
		startActivity(startNewActivityOpen);
	}
}
