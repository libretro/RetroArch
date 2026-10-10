package com.retroarch.browser.retroactivity;

import java.io.IOException;

import com.retroarch.browser.preferences.util.UserPreferences;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.graphics.SurfaceTexture.OnFrameAvailableListener;
import android.hardware.Camera;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

/**
 * The legacy {@link Camera} for the native camera driver: a GL
 * SurfaceTexture a GL core samples, or NV21 preview buffers handed
 * over as they are. Camera2 is driven from native code and uses only
 * {@link #onCameraInit} of this, for the permission.
 */
@SuppressLint("NewApi")
public class RetroActivityCamera extends RetroActivityCommon
{
  private static final int REQUEST_CODE_CAMERA = 127;
  private static final int RAW_BUFFERS = 3;

  private Camera mCamera = null;
  private long lastTimestamp = 0;
  private SurfaceTexture texture;
  private boolean updateSurface = true;
  private boolean camera_service_running = false;

  /* the newest NV21 preview buffer not yet taken by native code; a
   * newer one replaces it and the older buffer goes back to the camera */
  private final AtomicReference<byte[]> rawPending = new AtomicReference<byte[]>();
  private SurfaceTexture rawSink;
  private int rawWidth;
  private int rawHeight;
  private int rawRotation;

  /**
   * Executed when the {@link Camera}
   * is staring to capture.
   */
  public void onCameraStart()
  {
    if (camera_service_running)
      return;

    if (mCamera != null)
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

    if (mCamera != null)
      mCamera.stopPreview();
    camera_service_running = false;
  }

  /**
   * Releases the currently held {@link Camera} instance.
   */
  public void onCameraFree()
  {
    onCameraStop();

    if (mCamera != null)
    {
      mCamera.setPreviewCallbackWithBuffer(null);
      mCamera.release();
    }
    mCamera = null;
    rawPending.set(null);
  }

  /**
   * The camera permission: true when held. When not, it is asked for,
   * which a port that declares it in its manifest turns into the
   * prompt, and this returns false; the core gets its camera on a
   * later run.
   */
  public boolean onCameraInit()
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return true;
    if (checkSelfPermission(android.Manifest.permission.CAMERA)
        == PackageManager.PERMISSION_GRANTED)
      return true;
    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        requestPermissions(new String[] { android.Manifest.permission.CAMERA },
            REQUEST_CODE_CAMERA);
      }
    });
    return false;
  }

  /** The legacy Camera, opened once; false when the device has none or it is in use. */
  private boolean openCamera()
  {
    if (mCamera != null)
      return true;
    try
    {
      mCamera = Camera.open();
    }
    catch (RuntimeException e)
    {
      Log.e("RetroActivity", "Camera.open: " + e.getMessage());
      mCamera = null;
    }
    if (mCamera == null)
      return false;
    try
    {
      Camera.CameraInfo info = new Camera.CameraInfo();
      Camera.getCameraInfo(0, info);
      rawRotation = info.orientation;
    }
    catch (RuntimeException e)
    {
      rawRotation = 0;
    }
    return true;
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
   * @return false when there is no camera to set it on.
   */
  public boolean onCameraSetTexture(int gl_texid)
  {
    if (!openCamera())
      return false;
    if (texture == null)
      onCameraTextureInit(gl_texid);
    try
    {
      mCamera.setPreviewTexture(texture);
    }
    catch (IOException e)
    {
      Log.e("RetroActivity", "setPreviewTexture: " + e.getMessage());
      return false;
    }
    return true;
  }

  /**
   * Preview into NV21 buffers instead of a texture, at the supported
   * size nearest @width x @height (any size when either is 0). The
   * buffers rotate between the camera, {@link #rawPending} and the
   * native reader, so a frame is never copied on its way out.
   */
  public boolean onCameraRawInit(int width, int height)
  {
    if (!openCamera())
      return false;
    try
    {
      Camera.Parameters p = mCamera.getParameters();
      List<Camera.Size> sizes = p.getSupportedPreviewSizes();
      Camera.Size best = null;
      long bestCost = Long.MAX_VALUE;
      int i;
      for (i = 0; sizes != null && i < sizes.size(); i++)
      {
        Camera.Size sz = sizes.get(i);
        long cost = (width > 0 && height > 0)
            ? Math.abs((long)sz.width * sz.height - (long)width * height)
            : Math.abs((long)sz.width * sz.height - 640L * 480L);
        if (best == null || cost < bestCost)
        {
          best = sz;
          bestCost = cost;
        }
      }
      if (best == null)
        return false;
      p.setPreviewFormat(ImageFormat.NV21);
      p.setPreviewSize(best.width, best.height);
      mCamera.setParameters(p);
      rawWidth  = best.width;
      rawHeight = best.height;

      /* a preview needs a sink even when nothing draws it */
      if (rawSink == null)
        rawSink = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            ? new SurfaceTexture(false) : new SurfaceTexture(0);
      mCamera.setPreviewTexture(rawSink);

      int bytes = rawWidth * rawHeight
          + 2 * ((rawWidth + 1) / 2) * ((rawHeight + 1) / 2);
      for (i = 0; i < RAW_BUFFERS; i++)
        mCamera.addCallbackBuffer(new byte[bytes]);
      mCamera.setPreviewCallbackWithBuffer(rawCallback);
    }
    catch (IOException e)
    {
      Log.e("RetroActivity", "raw preview: " + e.getMessage());
      return false;
    }
    catch (RuntimeException e)
    {
      Log.e("RetroActivity", "raw preview: " + e.getMessage());
      return false;
    }
    return true;
  }

  private final Camera.PreviewCallback rawCallback = new Camera.PreviewCallback()
  {
    @Override
    public void onPreviewFrame(byte[] data, Camera camera)
    {
      byte[] older = rawPending.getAndSet(data);
      if (older != null)
        camera.addCallbackBuffer(older);
    }
  };

  /** The newest preview buffer, or null; its owner until {@link #onCameraRawDone}. */
  public byte[] onCameraPollRaw()
  {
    return rawPending.getAndSet(null);
  }

  /** A buffer from {@link #onCameraPollRaw} the native side has read. */
  public void onCameraRawDone(byte[] buf)
  {
    if (mCamera != null && buf != null)
      mCamera.addCallbackBuffer(buf);
  }

  public int onCameraRawWidth()
  {
    return rawWidth;
  }

  public int onCameraRawHeight()
  {
    return rawHeight;
  }

  /** Degrees the frame is to be rotated clockwise to be upright. */
  public int onCameraRawRotation()
  {
    return rawRotation;
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    if (requestCode == REQUEST_CODE_CAMERA)
      return;
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
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
    edit.apply();

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
    edit.apply();
    
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
      edit.apply();
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
