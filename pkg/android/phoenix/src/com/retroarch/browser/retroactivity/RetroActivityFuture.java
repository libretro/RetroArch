package com.retroarch.browser.retroactivity;

import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.content.Intent;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.input.InputManager;
import android.Manifest;
import android.os.Build;
import android.os.Bundle;
import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.List;
import java.util.ArrayList;

public final class RetroActivityFuture extends RetroActivityCamera {

  // If set to true then Retroarch will completely exit when it loses focus
  private boolean quitfocus = false;

  private boolean addPermission(List<String> permissionsList, String permission)
  {
     if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M)
     {
        if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED)
        {
           permissionsList.add(permission);

           // Check for Rationale Option
           if (!shouldShowRequestPermissionRationale(permission))
              return false;
        }
     }

     return true;
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {

   if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M)
   {
      // Android 6.0+ needs runtime permission checks
      List<String> permissionsNeeded = new ArrayList<String>();
      final List<String> permissionsList = new ArrayList<String>();

      if (!addPermission(permissionsList, Manifest.permission.READ_EXTERNAL_STORAGE))
         permissionsNeeded.add("Read External Storage");
      if (!addPermission(permissionsList, Manifest.permission.WRITE_EXTERNAL_STORAGE))
         permissionsNeeded.add("Write External Storage");

      if (permissionsList.size() > 0)
      {
         // We have permissions that have not yet been approved, so we need to close this activity and switch to MainMenuActivity instead
         Intent intent = new Intent(this, com.retroarch.browser.mainmenu.MainMenuActivity.class);
         intent.putExtra("ROM", getIntent().getStringExtra("ROM"));
         intent.putExtra("LIBRETRO", getIntent().getStringExtra("LIBRETRO"));
         intent.putExtra("CONFIGFILE", getIntent().getStringExtra("CONFIGFILE"));
         intent.putExtra("QUITFOCUS", getIntent().getStringExtra("QUITFOCUS"));
         intent.putExtra("IME", getIntent().getStringExtra("IME"));
         intent.putExtra("DATADIR", getIntent().getStringExtra("DATADIR"));
         startActivity(intent);
         finish();
         return;
      }
   }

    super.onCreate(savedInstanceState);

    Intent retro = getIntent();

    if (!retro.hasExtra("LIBRETRO")) {
      // No core file passed in
      return;
    }

    // Copies the core to the necessary folder if needed (for sideloading cores)
    File providedCoreFile = new File(retro.getStringExtra("LIBRETRO"));
    File coresFolder = new File(UserPreferences.getPreferences(this).getString("libretro_path", this.getApplicationInfo().dataDir + "/cores/"));

    // Check that both exist
    if (!coresFolder.exists())
      coresFolder.mkdirs();

    if (!providedCoreFile.exists() || providedCoreFile.isDirectory())
        return;

    File destination = new File(coresFolder, providedCoreFile.getName());

    if (destination.getAbsolutePath() == providedCoreFile.getAbsolutePath()){
      // Nothing needs to be done if the provided core path is already in the correct folder
      return;
    }

    // Otherwise, copy the core file to the correct folder
    Log.d("sideload", "Copying " + providedCoreFile.getAbsolutePath() + " to " + destination.getAbsolutePath());

    try
    {
        InputStream is = new FileInputStream(providedCoreFile);
        OutputStream os = new FileOutputStream(destination);

        byte[] buf = new byte[1024];
        int length;

        while ((length = is.read(buf)) > 0)
        {
            os.write(buf, 0, length);
        }

        is.close();
        os.close();
        retro.putExtra("LIBRETRO", destination.getAbsolutePath());
    }
    catch (IOException ex)
    {
        ex.printStackTrace();
        return;
    }
  }

  @Override
  public void onResume() {
    super.onResume();

    setSustainedPerformanceMode(sustainedPerformanceMode);

    if (Build.VERSION.SDK_INT >= 19) {
      // Immersive mode

      // Constants from API > 14
      final int API_SYSTEM_UI_FLAG_LAYOUT_STABLE = 0x00000100;
      final int API_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 0x00000200;
      final int API_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 0x00000400;
      final int API_SYSTEM_UI_FLAG_FULLSCREEN = 0x00000004;
      final int API_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 0x00001000;

      View thisView = getWindow().getDecorView();
      thisView.setSystemUiVisibility(API_SYSTEM_UI_FLAG_LAYOUT_STABLE
          | API_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
          | API_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
          | API_SYSTEM_UI_FLAG_FULLSCREEN
          | API_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

      // Check for Android UI specific parameters
      Intent retro = getIntent();

      if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
        String refresh = retro.getStringExtra("REFRESH");

        // If REFRESH parameter is provided then try to set refreshrate accordingly
        if(refresh != null) {
          WindowManager.LayoutParams params = getWindow().getAttributes();
          params.preferredRefreshRate = Integer.parseInt(refresh);
          getWindow().setAttributes(params);
        }
      }

      // If QUITFOCUS parameter is provided then enable that Retroarch quits when focus is lost
      quitfocus = retro.hasExtra("QUITFOCUS");

      // If HIDEMOUSE parameters is provided then hide the mourse cursor
      // This requires NVIDIA Android extensions (available on NVIDIA Shield), if they are not
      // available then nothing will be done
      if (retro.hasExtra("HIDEMOUSE")) hideMouseCursor();
    }

    //Checks if Android versions is above 9.0 (28) and enable the screen to write over notch if the user desires
    if (Build.VERSION.SDK_INT >= 28) {
      ConfigFile configFile = new ConfigFile(UserPreferences.getDefaultConfigPath(this));
      try {
        if (configFile.getBoolean("video_notch_write_over_enable")) {
          getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
      } catch (Exception e) {
        Log.w("Key doesn't exist yet.", e.getMessage());
      }
    }
  }

  public void hideMouseCursor() {

    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.JELLY_BEAN) {
      // Check for NVIDIA extensions and minimum SDK version
      Method mInputManager_setCursorVisibility;
      try { mInputManager_setCursorVisibility =
        InputManager.class.getMethod("setCursorVisibility", boolean.class);
      }
      catch (NoSuchMethodException ex) {
        return; // Extensions were not available so do nothing
      }

      // Hide the mouse cursor
      InputManager inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
      try { mInputManager_setCursorVisibility.invoke(inputManager, false); }
      catch (InvocationTargetException ite) { }
      catch (IllegalAccessException iae)    { }
    }
  }

  @Override
  public void onStop() {
    super.onStop();

    // If QUITFOCUS parameter was set then completely exit Retroarch when focus is lost
    if (quitfocus) System.exit(0);
  }
}
