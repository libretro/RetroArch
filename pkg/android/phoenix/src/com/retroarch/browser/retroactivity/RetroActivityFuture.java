package com.retroarch.browser.retroactivity;

import android.view.View;
import android.view.WindowManager;
import android.content.Intent;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class RetroActivityFuture extends RetroActivityCamera {

  // If set to true then Retroarch will completely exit when it loses focus
  private boolean quitfocus = false;

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
