package com.retroarch.browser.retroactivity;

import com.retroarch.browser.preferences.util.UserPreferences;
import android.annotation.TargetApi;
import android.content.res.Configuration;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.app.UiModeManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.util.Log;

import java.util.concurrent.CountDownLatch;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends RetroActivityLocation
{
  public static int FRONTEND_POWERSTATE_NONE = 0;
  public static int FRONTEND_POWERSTATE_NO_SOURCE = 1;
  public static int FRONTEND_POWERSTATE_CHARGING = 2;
  public static int FRONTEND_POWERSTATE_CHARGED = 3;
  public static int FRONTEND_POWERSTATE_ON_POWER_SOURCE = 4;
  public boolean sustainedPerformanceMode = true;

	// Exiting cleanly from NDK seems to be nearly impossible.
	// Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
	// Use a separate JNI function to explicitly trigger the readback.
	public void onRetroArchExit()
	{
      finish();
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
}
