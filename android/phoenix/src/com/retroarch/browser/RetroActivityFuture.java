package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.hardware.Camera;
import android.os.Build;
import android.util.Log;

//For Android 3.0 and up

@SuppressLint("NewApi")
public final class RetroActivityFuture extends NativeActivity
{
	private Camera mCamera;
	private long lastTimestamp = 0;
	private SurfaceTexture texture;
	private boolean updateSurface = true;
	private Intent pendingIntent = null;

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

	public boolean onCameraPoll()
	{
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

	public void onCameraFree()
	{
		mCamera.release();
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
	public void onNewIntent(Intent intent)
	{
		Log.i("RetroActivity", "onNewIntent invoked.");
		super.onNewIntent(intent);
		setIntent(intent);
		pendingIntent = intent;
	}

	public String getPendingIntentFullPath()
	{
		return pendingIntent.getStringExtra("ROM");
	}

	public String getPendingIntentLibretroPath()
	{
		return pendingIntent.getStringExtra("LIBRETRO");
	}

	public String getPendingIntentConfigPath()
	{
		return pendingIntent.getStringExtra("CONFIGFILE");
	}

	public String getPendingIntentIME()
	{
		return pendingIntent.getStringExtra("IME");
	}

	public boolean hasPendingIntent()
	{
		if (pendingIntent == null)
			return false;
		return true;
	}

	public void clearPendingIntent()
	{
		pendingIntent = null;
	}

	@Override
	public void onBackPressed()
	{
		Log.i("RetroActivity", "onBackKeyPressed");
		Intent retro = new Intent(this, MainMenuActivity.class);
		retro.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
		startActivity(retro);
	}
}
