package com.retroarch.browser.retroactivity;

import android.util.Log;
import android.view.PointerIcon;
import android.view.View;
import android.view.WindowManager;
import android.content.Intent;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import android.os.Build;
import android.view.Display;
import android.view.WindowManager;
import android.view.Window;
import android.util.Log;
import java.io.*;
import java.util.regex.*;


public final class RetroActivityFuture extends RetroActivityCamera {

  // Tracks activity lifecycle state for MainMenuActivity resume detection
  public static volatile boolean isRunning = false;

  // If set to true then RetroArch will completely exit when it loses focus
  private boolean quitfocus = false;

  // Top-level window decor view
  private View mDecorView;

  // Constants used for Handler messages
  private static final int HANDLER_WHAT_TOGGLE_IMMERSIVE = 1;
  private static final int HANDLER_WHAT_TOGGLE_POINTER_CAPTURE = 2;
  private static final int HANDLER_WHAT_TOGGLE_POINTER_NVIDIA = 3;
  private static final int HANDLER_WHAT_TOGGLE_POINTER_ICON = 4;
  private static final int HANDLER_ARG_TRUE = 1;
  private static final int HANDLER_ARG_FALSE = 0;
  private static final int HANDLER_MESSAGE_DELAY_DEFAULT_MS = 300;

  // Handler used for UI events
  private final Handler mHandler = new Handler(Looper.getMainLooper()) {
    @Override
    public void handleMessage(Message msg) {
      boolean state = (msg.arg1 == HANDLER_ARG_TRUE) ? true : false;

      if (msg.what == HANDLER_WHAT_TOGGLE_IMMERSIVE) {
        attemptToggleImmersiveMode(state);
      } else if (msg.what == HANDLER_WHAT_TOGGLE_POINTER_CAPTURE) {
        attemptTogglePointerCapture(state);
      } else if (msg.what == HANDLER_WHAT_TOGGLE_POINTER_NVIDIA) {
        attemptToggleNvidiaCursorVisibility(state);
      } else if (msg.what == HANDLER_WHAT_TOGGLE_POINTER_ICON) {
        attemptTogglePointerIcon(state);
      }
    }
  };

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    
    try { requestNativeGameRefreshRate(); } catch (Throwable ignored) {}
    isRunning = true;
    mDecorView = getWindow().getDecorView();

    // If QUITFOCUS parameter is provided then enable that Retroarch quits when focus is lost
    quitfocus = getIntent().hasExtra("QUITFOCUS");
  }

  @Override
  public void onNewIntent(Intent intent) {
    super.onNewIntent(intent);
    try { requestNativeGameRefreshRate(); } catch (Throwable ignored) {}
    // Extract game parameters from new intent
    String newRom = intent.getStringExtra("ROM");
    String newCore = intent.getStringExtra("LIBRETRO");
    
    // Get current intent parameters for comparison
    Intent currentIntent = getIntent();
    String currentRom = currentIntent != null ? currentIntent.getStringExtra("ROM") : null;
    String currentCore = currentIntent != null ? currentIntent.getStringExtra("LIBRETRO") : null;
    
    
    // Check if we're trying to launch different content  
    if ((newRom != null && !newRom.equals(currentRom)) ||
        (newCore != null && !newCore.equals(currentCore))) {
      // Different game content - exit cleanly and let launcher restart us
      finish();
      System.exit(0);
    } else {
      // Same content, just update intent
      setIntent(intent);
    }
  }

  @Override
  public void onResume() {
    super.onResume();

    setSustainedPerformanceMode(sustainedPerformanceMode);

    // Check for Android UI specific parameters
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      String refresh = getIntent().getStringExtra("REFRESH");

      // If REFRESH parameter is provided then try to set refreshrate accordingly
      if (refresh != null) {
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.preferredRefreshRate = Integer.parseInt(refresh);
        getWindow().setAttributes(params);
      }
    }

    // Checks if Android versions is above 9.0 (28) and enable the screen to write over notch if the user desires
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
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

  @Override
  public void onStop() {
    super.onStop();

    // If QUITFOCUS parameter was set then completely exit RetroArch when focus is lost
    if (quitfocus) System.exit(0);
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    isRunning = false;
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);

    mHandlerSendUiMessage(HANDLER_WHAT_TOGGLE_IMMERSIVE, hasFocus);

    try {
      ConfigFile configFile = new ConfigFile(UserPreferences.getDefaultConfigPath(this));
      if (configFile.getBoolean("input_auto_mouse_grab")) {
        inputGrabMouse(hasFocus);
      }
    } catch (Exception e) {
      Log.w("[onWindowFocusChanged] exception thrown:", e.getMessage());
    }
  }

  private void mHandlerSendUiMessage(int what, boolean state) {
    int arg1 = (state ? HANDLER_ARG_TRUE : HANDLER_ARG_FALSE);
    int arg2 = -1;

    Message message = mHandler.obtainMessage(what, arg1, arg2);
    mHandler.sendMessageDelayed(message, HANDLER_MESSAGE_DELAY_DEFAULT_MS);
  }

  public void inputGrabMouse(boolean state) {
    mHandlerSendUiMessage(HANDLER_WHAT_TOGGLE_POINTER_CAPTURE, state);
    mHandlerSendUiMessage(HANDLER_WHAT_TOGGLE_POINTER_NVIDIA, state);
    mHandlerSendUiMessage(HANDLER_WHAT_TOGGLE_POINTER_ICON, state);
  }

  private void attemptToggleImmersiveMode(boolean state) {
    // Attempt to toggle "Immersive Mode" for Android 4.4 (19) and up
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
      try {
        if (state) {
          mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LOW_PROFILE
            | View.SYSTEM_UI_FLAG_IMMERSIVE);
        } else {
          mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
      } catch (Exception e) {
        Log.w("[attemptToggleImmersiveMode] exception thrown:", e.getMessage());
      }
    }
  }

  private void attemptTogglePointerCapture(boolean state) {
    // Attempt requestPointerCapture for Android 8.0 (26) and up
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      try {
        if (state) {
          mDecorView.requestPointerCapture();
        } else {
          mDecorView.releasePointerCapture();
        }
      } catch (Exception e) {
        Log.w("[attemptTogglePointerCapture] exception thrown:", e.getMessage());
      }
    }
  }

  private void attemptToggleNvidiaCursorVisibility(boolean state) {
    // Attempt setCursorVisibility for Android 4.1 (16) and up
    // only works if NVIDIA Android extensions for NVIDIA Shield are available
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
      try {
        boolean cursorVisibility = !state;
        Method mInputManager_setCursorVisibility = InputManager.class.getMethod("setCursorVisibility", boolean.class);
        InputManager inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        mInputManager_setCursorVisibility.invoke(inputManager, cursorVisibility);
      } catch (NoSuchMethodException e) {
        // Extensions were not available so do nothing
      } catch (Exception e) {
        Log.w("[attemptToggleNvidiaCursorVisibility] exception thrown:", e.getMessage());
      }
    }
  }

  private void attemptTogglePointerIcon(boolean state) {
    // Attempt setPointerIcon for Android 7.x (24, 25) only
    // For Android 8.0+, requestPointerCapture is used
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
      try {
        if (state) {
          PointerIcon nullPointerIcon = PointerIcon.getSystemIcon(this, PointerIcon.TYPE_NULL);
          mDecorView.setPointerIcon(nullPointerIcon);
        } else {
          // Restore the pointer icon to it's default value
          mDecorView.setPointerIcon(null);
        }
      } catch (Exception e) {
        Log.w("[attemptTogglePointerIcon] exception thrown:", e.getMessage());
      }
    }
  }

  /** Requests a 60 Hz mode matching the current resolution, if available. */
  private void request60HzIfPossible() {
      final Window window = getWindow();
      if (window == null) return;
      final WindowManager wm = getWindowManager();
      if (wm == null) return;

      try {
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) { // API 23+
              final Display display = wm.getDefaultDisplay();
              if (display == null) return;
              final Display.Mode current = display.getMode();
              final Display.Mode[] modes = display.getSupportedModes();
              int bestId = current.getModeId();
              float bestScore = Float.MAX_VALUE;

              // Prefer 60 Hz at current resolution; penalize >60.5 so we don't land on 120
              for (Display.Mode m : modes) {
                  if (m.getPhysicalWidth() == current.getPhysicalWidth()
                          && m.getPhysicalHeight() == current.getPhysicalHeight()) {
                      final float refresh = m.getRefreshRate();
                      float penalty = Math.abs(refresh - 60.0f);
                      if (refresh > 60.5f) penalty += 1000f;
                      if (penalty < bestScore) {
                          bestScore = penalty;
                          bestId = m.getModeId();
                      }
                  }
              }

              WindowManager.LayoutParams lp = window.getAttributes();
              // preferredDisplayModeId (via reflection for vendor quirks)
              try {
                  lp.getClass().getField("preferredDisplayModeId").setInt(lp, bestId);
              } catch (NoSuchFieldException ignored) { }
              // preferredRefreshRate (hint)
              try {
                  lp.getClass().getField("preferredRefreshRate").setFloat(lp, 60.0f);
              } catch (NoSuchFieldException ignored) { }
              window.setAttributes(lp);
          } else {
              // Very old APIs: try legacy setter via reflection
              try {
                  Window.class.getMethod("setPreferredRefreshRate", float.class)
                          .invoke(window, 60.0f);
              } catch (Throwable ignored) { }
          }
      } catch (Throwable t) {
          try { Log.w("RetroArch", "Failed to request 60Hz: " + t); } catch (Throwable ignored) { }
      }
  }

  private void requestRefreshIfPossible(float targetHz) {
      final Window window = getWindow();
      if (window == null) return;
      final WindowManager wm = getWindowManager();
      if (wm == null) return;

      try {
          if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) { // API 23+
              final Display display = wm.getDefaultDisplay();
              if (display == null) return;
              final Display.Mode current = display.getMode();
              final Display.Mode[] modes = display.getSupportedModes();

              // Snap target to ‘nice’ broadcast rates when very close
              if (Math.abs(targetHz - 59.94f) < 0.15f) targetHz = 59.94f;
              else if (Math.abs(targetHz - 60.0f) < 0.15f) targetHz = 60.0f;
              else if (Math.abs(targetHz - 50.0f) < 0.15f) targetHz = 50.0f;

              int bestId = current.getModeId();
              float bestScore = Float.MAX_VALUE;

              for (Display.Mode m : modes) {
                  if (m.getPhysicalWidth() == current.getPhysicalWidth()
                          && m.getPhysicalHeight() == current.getPhysicalHeight()) {
                      final float refresh = m.getRefreshRate();
                      // cost = distance from target; add small bias against >120 for retro content
                      float cost = Math.abs(refresh - targetHz);
                      if (targetHz <= 62f && refresh > 90f) cost += 5f; // avoid 90/120 when we want ~60
                      if (cost < bestScore) {
                          bestScore = cost;
                          bestId = m.getModeId();
                      }
                  }
              }

              WindowManager.LayoutParams lp = window.getAttributes();
              try {
                  lp.getClass().getField("preferredDisplayModeId").setInt(lp, bestId);
              } catch (NoSuchFieldException ignored) {}
              try {
                  lp.getClass().getField("preferredRefreshRate").setFloat(lp, targetHz);
              } catch (NoSuchFieldException ignored) {}
              window.setAttributes(lp);
          } else {
              try {
                  Window.class.getMethod("setPreferredRefreshRate", float.class)
                          .invoke(window, targetHz);
              } catch (Throwable ignored) {}
          }
      } catch (Throwable t) {
          try { Log.w("RetroArch", "Failed to request " + targetHz + "Hz: " + t); } catch (Throwable ignored) {}
      }
  }


  // 2b) Fallback: guess by content extension/system
  private Float guessFpsFromContentPath(String pathLower) {
      if (pathLower == null) return null;
      // PAL first
      if (pathLower.contains("(e)") || pathLower.contains("[pal]") || pathLower.contains("pal"))
          return 50.0f;

      // System heuristics (common libretro rates)
      if (pathLower.endsWith(".gb") || pathLower.endsWith(".gbc")) return 59.73f;
      if (pathLower.endsWith(".gba")) return 59.73f; // GBA nominal
      if (pathLower.endsWith(".nes") || pathLower.endsWith(".fds")) return 60.0f;
      if (pathLower.endsWith(".sfc") || pathLower.endsWith(".smc")) return 60.0f;
      if (pathLower.endsWith(".gen") || pathLower.endsWith(".md") || pathLower.endsWith(".bin")) return 59.92f;
      if (pathLower.endsWith(".pce")) return 59.82f;
      if (pathLower.endsWith(".ngp") || pathLower.endsWith(".ngc")) return 60.0f;
      if (pathLower.endsWith(".n64") || pathLower.endsWith(".z64") || pathLower.endsWith(".v64")) return 60.0f;
      // Archive inner names
      if (pathLower.contains(".gba#") || pathLower.contains(".gba|")) return 59.73f;

      return null;
  }


  private static native float nativeGetContentFps();

  private void requestNativeGameRefreshRate() {
     Log.w("[Retroarch]","requestNative FPS");
    Float fps = 0f;
     try { fps = nativeGetContentFps(); } catch (Throwable ignored) {}
     
      if (fps <= 0f || fps == null) {
          // Try to get the current content path from intent extras, if available
          String content = getIntent() != null ? getIntent().getStringExtra("content_path") : null;
          fps = guessFpsFromContentPath(content != null ? content.toLowerCase() : null);
      }
      if (fps == null || fps <= 0f) {
        fps = 60.0f;
      }

      // Clamp to sane range
      if (fps < 45f) fps = 50.0f;      // avoid weird low reads
      if (fps > 65f && fps < 85f) fps = 60.0f; // round 70ish mistakes down to 60
      Log.w("[Retroarch]","fps found" + fps);
      requestRefreshIfPossible(fps);
  }


}
