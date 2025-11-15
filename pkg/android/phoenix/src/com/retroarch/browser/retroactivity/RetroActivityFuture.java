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
import com.retroarch.browser.mainmenu.MainMenuActivity;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import android.content.pm.PackageManager;
import android.Manifest;

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
    
    isRunning = true;
    mDecorView = getWindow().getDecorView();

    // Check if we need permissions before proceeding
    if (!checkPermissions()) {
      // Redirect to MainMenuActivity to get permissions
      Intent mainMenuIntent = new Intent(this, MainMenuActivity.class);
      mainMenuIntent.putExtras(getIntent());
      startActivity(mainMenuIntent);
      finish();
      return;
    }

    // Handle core sideloading if external core path provided
    handleCoreSideloading();

    // If QUITFOCUS parameter is provided then enable that Retroarch quits when focus is lost
    quitfocus = getIntent().hasExtra("QUITFOCUS");
  }

  private boolean checkPermissions() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      List<String> permissionsNeeded = new ArrayList<String>();
      
      if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
        permissionsNeeded.add(Manifest.permission.READ_EXTERNAL_STORAGE);
      }
      if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
        permissionsNeeded.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
      }
      
      return permissionsNeeded.isEmpty();
    }
    return true;
  }

  private void handleCoreSideloading() {
    String corePath = getIntent().getStringExtra("LIBRETRO");
    if (corePath == null || corePath.isEmpty()) {
      return;
    }

    File coreFile = new File(corePath);
    if (!coreFile.exists()) {
      Log.w("RetroActivityFuture", "Core file does not exist: " + corePath);
      return;
    }

    // Get the cores directory
    String coresDir = getApplicationInfo().dataDir + "/cores/";
    File coresDirFile = new File(coresDir);
    
    // Create cores directory if it doesn't exist
    if (!coresDirFile.exists()) {
      if (!coresDirFile.mkdirs()) {
        Log.e("RetroActivityFuture", "Failed to create cores directory: " + coresDir);
        return;
      }
    }

    // Check if core is outside RetroArch cores folder - if so, sideload it
    if (!corePath.startsWith(coresDir)) {
      String coreFileName = coreFile.getName();
      File destinationFile = new File(coresDir, coreFileName);
      
      try {
        copyFile(coreFile, destinationFile);
        Log.i("RetroActivityFuture", "Sideloaded core from " + corePath + " to " + destinationFile.getAbsolutePath());
        
        // Update intent to point to the new location
        getIntent().putExtra("LIBRETRO", destinationFile.getAbsolutePath());
      } catch (IOException e) {
        Log.e("RetroActivityFuture", "Failed to sideload core: " + e.getMessage());
      }
    }
  }

  private void copyFile(File source, File destination) throws IOException {
    FileInputStream inStream = null;
    FileOutputStream outStream = null;
    
    try {
      inStream = new FileInputStream(source);
      outStream = new FileOutputStream(destination);
      
      byte[] buffer = new byte[8192];
      int length;
      while ((length = inStream.read(buffer)) > 0) {
        outStream.write(buffer, 0, length);
      }
    } finally {
      if (inStream != null) {
        try {
          inStream.close();
        } catch (IOException e) {
          // Ignore
        }
      }
      if (outStream != null) {
        try {
          outStream.close();
        } catch (IOException e) {
          // Ignore
        }
      }
    }
  }

  @Override
  public void onNewIntent(Intent intent) {
    super.onNewIntent(intent);
    
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
}
