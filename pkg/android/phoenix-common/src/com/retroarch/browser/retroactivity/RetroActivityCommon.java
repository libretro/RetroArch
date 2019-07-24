package com.retroarch.browser.retroactivity;

import com.retroarch.browser.preferences.util.UserPreferences;
import android.annotation.TargetApi;
import android.app.NativeActivity;
import android.content.res.Configuration;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.media.AudioAttributes;
import android.view.InputDevice;
import android.view.Surface;
import android.view.WindowManager;
import android.app.UiModeManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.util.Log;
import java.lang.Math;
import java.util.concurrent.CountDownLatch;
import java.util.Locale;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends NativeActivity
{
  public static int FRONTEND_POWERSTATE_NONE = 0;
  public static int FRONTEND_POWERSTATE_NO_SOURCE = 1;
  public static int FRONTEND_POWERSTATE_CHARGING = 2;
  public static int FRONTEND_POWERSTATE_CHARGED = 3;
  public static int FRONTEND_POWERSTATE_ON_POWER_SOURCE = 4;
  public static int FRONTEND_ORIENTATION_0 = 0;
  public static int FRONTEND_ORIENTATION_90 = 1;
  public static int FRONTEND_ORIENTATION_180 = 2;
  public static int FRONTEND_ORIENTATION_270 = 3;
  public static int RETRO_RUMBLE_STRONG = 0;
  public static int RETRO_RUMBLE_WEAK = 1;
  public boolean sustainedPerformanceMode = true;
  public int screenOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

  public void doVibrate(int id, int effect, int strength, int oneShot)
  {
    Vibrator vibrator = null;
    int repeat = 0;
    long[] pattern = {16};
    int[] strengths = {strength};

    if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.JELLY_BEAN) {
      if (id == -1)
        vibrator = (Vibrator)getSystemService(Context.VIBRATOR_SERVICE);
      else
      {
        InputDevice dev = InputDevice.getDevice(id);

        if (dev != null)
          vibrator = dev.getVibrator();
      }
    }

    if (vibrator == null)
      return;

    if (strength == 0) {
      vibrator.cancel();
      return;
    }

    if (oneShot > 0)
      repeat = -1;
    else
      pattern[0] = 1000;

    if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
      if (id >= 0)
        Log.i("RetroActivity", "Vibrate id " + id + ": strength " + strength);

      vibrator.vibrate(VibrationEffect.createWaveform(pattern, strengths, repeat), new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build());
    }else{
      vibrator.vibrate(pattern, repeat);
    }
  }

  // Exiting cleanly from NDK seems to be nearly impossible.
  // Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
  // Use a separate JNI function to explicitly trigger the readback.
  public void onRetroArchExit()
  {
      finish();
  }

// https://stackoverflow.com/questions/4553650/how-to-check-device-natural-default-orientation-on-android-i-e-get-landscape/4555528#4555528
  public int getDeviceDefaultOrientation() {
    WindowManager windowManager = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
    Configuration config = getResources().getConfiguration();
    int rotation = windowManager.getDefaultDisplay().getRotation();

    if (((rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) &&
        config.orientation == Configuration.ORIENTATION_LANDSCAPE)
        || ((rotation == Surface.ROTATION_90 || rotation == Surface.ROTATION_270) &&
        config.orientation == Configuration.ORIENTATION_PORTRAIT))
    {
      return Configuration.ORIENTATION_LANDSCAPE;
    }else{
      return Configuration.ORIENTATION_PORTRAIT;
    }
  }

  public void setScreenOrientation(int orientation)
  {
    int naturalOrientation = getDeviceDefaultOrientation();
    int newOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    // We assume no device has a natural orientation that is reversed
    switch (naturalOrientation) {
      case Configuration.ORIENTATION_PORTRAIT:
      {
        if (orientation == FRONTEND_ORIENTATION_0) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        }else if (orientation == FRONTEND_ORIENTATION_90) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }else if (orientation == FRONTEND_ORIENTATION_180) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        }else if (orientation == FRONTEND_ORIENTATION_270) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        }
        break;
      }
      case Configuration.ORIENTATION_LANDSCAPE:
      {
        if (orientation == FRONTEND_ORIENTATION_0) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }else if (orientation == FRONTEND_ORIENTATION_90) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        }else if (orientation == FRONTEND_ORIENTATION_180) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        }else if (orientation == FRONTEND_ORIENTATION_270) {
          newOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        }
        break;
      }
    }

    screenOrientation = newOrientation;

    Log.i("RetroActivity", "setting new orientation to " + screenOrientation);

    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        setRequestedOrientation(screenOrientation);
      }
    });
  }

  public String getUserLanguageString()
  {
    String lang = Locale.getDefault().getLanguage();
    String country = Locale.getDefault().getCountry();

    if (lang.length() == 0)
      return "en";

    if (country.length() == 0)
      return lang;

    return lang + '_' + country;
  }

  @TargetApi(24)
  public void setSustainedPerformanceMode(boolean on)
  {
    sustainedPerformanceMode = on;

    if (Build.VERSION.SDK_INT >= 24) {
      if (isSustainedPerformanceModeSupported()) {
        final CountDownLatch latch = new CountDownLatch(1);

        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            Log.i("RetroActivity", "setting sustained performance mode to " + sustainedPerformanceMode);

            getWindow().setSustainedPerformanceMode(sustainedPerformanceMode);

            latch.countDown();
          }
        });

        try {
          latch.await();
        }catch(InterruptedException e) {
          e.printStackTrace();
        }
      }
    }
  }

  @TargetApi(24)
  public boolean isSustainedPerformanceModeSupported()
  {
    boolean supported = false;

    if (Build.VERSION.SDK_INT >= 24)
    {
      PowerManager powerManager = (PowerManager)getSystemService(Context.POWER_SERVICE);

      if (powerManager.isSustainedPerformanceModeSupported())
        supported = true;
    }

    Log.i("RetroActivity", "isSustainedPerformanceModeSupported? " + supported);

    return supported;
  }

  public int getBatteryLevel()
  {
    IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
    // This doesn't actually register anything (or need to) because we know this particular intent is sticky and we do not specify a BroadcastReceiver anyway
    Intent batteryStatus = registerReceiver(null, ifilter);
    int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
    int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, 100);

    float percent = ((float)level / (float)scale) * 100.0f;

    Log.i("RetroActivity", "battery: level = " + level + ", scale = " + scale + ", percent = " + percent);

    return (int)percent;
  }

  public int getPowerstate()
  {
    IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
    // This doesn't actually register anything (or need to) because we know this particular intent is sticky and we do not specify a BroadcastReceiver anyway
    Intent batteryStatus = registerReceiver(null, ifilter);
    int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
    boolean hasBattery = batteryStatus.getBooleanExtra(BatteryManager.EXTRA_PRESENT, false);
    boolean isCharging = (status == BatteryManager.BATTERY_STATUS_CHARGING);
    boolean isCharged = (status == BatteryManager.BATTERY_STATUS_FULL);
    int powerstate = FRONTEND_POWERSTATE_NONE;

    if (isCharged)
      powerstate = FRONTEND_POWERSTATE_CHARGED;
    else if (isCharging)
      powerstate = FRONTEND_POWERSTATE_CHARGING;
    else if (!hasBattery)
      powerstate = FRONTEND_POWERSTATE_NO_SOURCE;
    else
      powerstate = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

    Log.i("RetroActivity", "power state = " + powerstate);

    return powerstate;
  }

  public boolean isAndroidTV()
  {
    Configuration config = getResources().getConfiguration();
    UiModeManager uiModeManager = (UiModeManager)getSystemService(UI_MODE_SERVICE);

    if (uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION)
    {
      Log.i("RetroActivity", "isAndroidTV == true");
      return true;
    }
    else
    {
      Log.i("RetroActivity", "isAndroidTV == false");
      return false;
    }
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    int oldOrientation = 0;
    boolean hasOldOrientation = false;

    super.onConfigurationChanged(newConfig);

    Log.i("RetroActivity", "onConfigurationChanged: orientation is now " + newConfig.orientation);

    SharedPreferences prefs = UserPreferences.getPreferences(this);
    SharedPreferences.Editor edit = prefs.edit();

    hasOldOrientation = prefs.contains("ORIENTATION");

    if (hasOldOrientation)
      oldOrientation = prefs.getInt("ORIENTATION", 0);

    edit.putInt("ORIENTATION", newConfig.orientation);
    edit.apply();

    Log.i("RetroActivity", "hasOldOrientation? " + hasOldOrientation + " newOrientation: " + newConfig.orientation + " oldOrientation: " + oldOrientation);
  }
}
