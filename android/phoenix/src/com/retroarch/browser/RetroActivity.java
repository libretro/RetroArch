package com.retroarch.browser;

import java.io.IOException;

import com.retroarch.browser.preferences.util.UserPreferences;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
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

	public void onCameraStart()
	{
		Log.i("RetroActivity", "onCameraStart");
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
		boolean ret;
		if (texture == null)
		{
			Log.i("RetroActivity", "no texture");
			ret = false;
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
				ret = true;
			}
			else
			{
				ret = false;
			}
		}
		else
		{
			ret = true;
		}

		return ret;
	}

	public void onCameraFree()
	{
		mCamera.release();
	}
	
	@SuppressLint("NewApi")
	public void onCameraTextureInit(int gl_texid)
	{
		Log.e("RetroActivity", "onCameraTextureInit" + gl_texid);
		texture = new SurfaceTexture(gl_texid);
		texture.setOnFrameAvailableListener(onCameraFrameAvailableListener);
	}
	
    @SuppressLint("NewApi")
	private SurfaceTexture.OnFrameAvailableListener onCameraFrameAvailableListener =
            new SurfaceTexture.OnFrameAvailableListener() {
        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        	Log.e("RetroActivity", "onFrameAvailable");
			updateSurface = true;
        }
    };

	@SuppressLint("NewApi")
	public void onCameraSetTexture(int gl_texid) throws IOException
	{
		Log.i("RetroActivity", "onCameraSetTexture: " + gl_texid);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			Camera.Parameters params = mCamera.getParameters();
			params.setSceneMode(Camera.Parameters.SCENE_MODE_PORTRAIT);
			mCamera.setParameters(params);
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
	}
}
