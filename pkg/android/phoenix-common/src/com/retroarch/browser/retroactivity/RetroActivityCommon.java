/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Adam "TideGear" Milecki
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
package com.retroarch.browser.retroactivity;

import com.retroarch.BuildConfig;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.playcore.PlayCoreManager;

import android.annotation.TargetApi;
import android.app.NativeActivity;
import android.app.PendingIntent;
import android.content.res.Configuration;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.hardware.input.InputManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.media.AudioAttributes;
import android.net.Uri;
import android.os.Bundle;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.system.Os;
import android.view.accessibility.AccessibilityManager;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.Surface;
import android.view.WindowManager;
import android.app.UiModeManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.os.CombinedVibration;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.os.VibratorManager;
import android.util.Log;


import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.Locale;

/**
 * Class which provides common methods for RetroActivity related classes.
 */
public class RetroActivityCommon extends NativeActivity
        implements InputManager.InputDeviceListener
{
  static {
    System.loadLibrary("retroarch-activity");
  }

  public static final int FRONTEND_POWERSTATE_NONE = 0;
  public static final int FRONTEND_POWERSTATE_NO_SOURCE = 1;
  public static final int FRONTEND_POWERSTATE_CHARGING = 2;
  public static final int FRONTEND_POWERSTATE_CHARGED = 3;
  public static final int FRONTEND_POWERSTATE_ON_POWER_SOURCE = 4;
  public static final int FRONTEND_ORIENTATION_0 = 0;
  public static final int FRONTEND_ORIENTATION_90 = 1;
  public static final int FRONTEND_ORIENTATION_180 = 2;
  public static final int FRONTEND_ORIENTATION_270 = 3;
  public static final int RETRO_RUMBLE_STRONG = 0;
  public static final int RETRO_RUMBLE_WEAK = 1;
  public static final int REQUEST_CODE_OPEN_DOCUMENT_TREE = 0;
  public boolean sustainedPerformanceMode = true;
  public int screenOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

  /* USB HID rumble: action string for the runtime permission broadcast. */
  private static final String ACTION_USB_PERMISSION = "com.retroarch.USB_PERMISSION";

  /* Supported USB VID/PID constants for Sony controllers (controlTransfer rumble). */
  private static final int VID_SONY      = 0x054C;
  private static final int PID_DS4_V1    = 0x05C4;
  private static final int PID_DS4_V2    = 0x09CC;
  private static final int PID_DUALSENSE = 0x0CE6;

  /* Pooled USB connections keyed by Android InputDevice ID.
   * Opened on first successful permission grant; closed on error or destroy.
   * Access only from the native JNI thread — no locking needed. */
  private final Map<Integer, UsbDeviceConnection> mUsbConnections =
          new HashMap<Integer, UsbDeviceConnection>();

  /* Guards against permission request storms — one in-flight request per device. */
  private final Map<Integer, Boolean> mUsbPermissionPending =
          new HashMap<Integer, Boolean>();

  /* Clears the in-flight permission flag when the user responds to the system dialog. */
  private BroadcastReceiver mUsbPermissionReceiver = new BroadcastReceiver() {
    @Override
    public void onReceive(Context context, Intent intent) {
      if (!ACTION_USB_PERMISSION.equals(intent.getAction()))
        return;
      UsbDevice device = (UsbDevice) intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
      if (device == null)
        return;
      /* Clear the pending flag by matching on VID/PID so the next rumble
       * call will proceed without re-requesting permission. */
      for (Map.Entry<Integer, Boolean> entry :
              new ArrayList<Map.Entry<Integer, Boolean>>(mUsbPermissionPending.entrySet()))
      {
        InputDevice inputDevice = InputDevice.getDevice(entry.getKey());
        if (inputDevice != null
                && inputDevice.getVendorId()  == device.getVendorId()
                && inputDevice.getProductId() == device.getProductId())
          mUsbPermissionPending.remove(entry.getKey());
      }
      Log.i("RetroActivity", "USB permission "
              + (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                      ? "granted" : "denied")
              + " for " + device.getProductName());
    }
  };

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    cleanupSymlinks();
    updateSymlinks();

    registerReceiver(mUsbPermissionReceiver, new IntentFilter(ACTION_USB_PERMISSION));
    ((InputManager) getSystemService(Context.INPUT_SERVICE))
            .registerInputDeviceListener(this, null);
    PlayCoreManager.getInstance().onCreate(this);
    super.onCreate(savedInstanceState);
  }

  @Override
  protected void onDestroy() {
    ((InputManager) getSystemService(Context.INPUT_SERVICE))
            .unregisterInputDeviceListener(this);
    unregisterReceiver(mUsbPermissionReceiver);
    closeAllUsbConnections();
    PlayCoreManager.getInstance().onDestroy();
    super.onDestroy();
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent intent)
  {
    if (intent == null)
      return;

    switch (requestCode)
    {
      case REQUEST_CODE_OPEN_DOCUMENT_TREE:
        {
          Uri uri = intent.getData();
          if (uri == null)
            break;
          if (Build.VERSION.SDK_INT >= 19)
            getContentResolver().takePersistableUriPermission(uri, intent.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
          safTreeAdded(uri.toString());
        }
        break;

      default:
        break;
    }
  }

  public void requestOpenDocumentTree()
  {
    startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQUEST_CODE_OPEN_DOCUMENT_TREE);
  }

  public void doVibrate(int id, int effect, int strength, int oneShot)
  {
    Vibrator vibrator = null;
    int repeat = 0;
    long[] pattern = {0, 16};
    int[] strengths = {0, strength};

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
      pattern[1] = 1000;

    if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
      if (id >= 0)
        Log.i("RetroActivity", "Vibrate id " + id + ": strength " + strength);

      vibrator.vibrate(VibrationEffect.createWaveform(pattern, strengths, repeat), new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build());
    }else{
      vibrator.vibrate(pattern, repeat);
    }
  }

  /**
   * Vibrates a controller's motors independently for dual-rumble gamepads.
   *
   * On Android 12+ (API 31) this uses VibratorManager to address each
   * controller motor separately, preserving the RETRO_RUMBLE_STRONG /
   * RETRO_RUMBLE_WEAK distinction that the legacy single-vibrator path lost
   * by OR-merging both channels into one amplitude value.
   *
   * Falls back to doVibrate (single-vibrator) on Android < 12 or when the
   * controller does not expose multiple vibrators.
   *
   * @param id             InputDevice ID of the controller
   * @param strongStrength  Large / low-frequency motor amplitude (0–255)
   * @param weakStrength    Small / high-frequency motor amplitude (0–255)
   * @param unused          Reserved; always pass 0
   */
  public void doVibrateJoypad(int id, int strongStrength, int weakStrength, int unused)
  {
    /* Android 12+ (API 31): attempt the multi-vibrator path. */
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    {
      if (doVibrateJoypadApi31(id, strongStrength, weakStrength))
        return;
    }

    /* Fallback for Android < 12 or when VibratorManager cannot be used:
     * drive the single vibrator with the stronger channel value so the
     * controller still rumbles rather than staying silent. */
    int fallbackStrength = Math.max(strongStrength, weakStrength);
    doVibrate(id, RETRO_RUMBLE_STRONG, fallbackStrength, 0);
  }

  /**
   * Inner implementation of doVibrateJoypad for Android 12+ (API 31).
   *
   * Uses InputDevice.getVibratorManager() to enumerate the controller's
   * vibrator IDs and drive them independently via CombinedVibration so each
   * motor receives its own amplitude.
   *
   * The mapping assumes vibratorIds[0] is the large / low-frequency
   * (strong) motor and vibratorIds[1] is the small / high-frequency (weak)
   * motor.  This ordering has been observed consistently for DualShock 4,
   * DualSense, and Xbox controllers on Android 12+, but is NOT guaranteed
   * by the Android API — treat this as a best-effort heuristic and code
   * defensively.
   *
   * @return true if vibration was handled, false to trigger fallback
   */
  @TargetApi(Build.VERSION_CODES.S)
  private boolean doVibrateJoypadApi31(int id, int strongStrength, int weakStrength)
  {
    InputDevice dev = InputDevice.getDevice(id);
    if (dev == null)
      return false;

    VibratorManager vm  = dev.getVibratorManager();
    int[]  vibratorIds  = vm.getVibratorIds();

    if (vibratorIds.length == 0)
      return false;

    if (vibratorIds.length == 1)
    {
      /* Single-motor controller: use the stronger channel value so the
       * controller still rumbles rather than going silent. */
      int singleStrength = Math.max(strongStrength, weakStrength);
      Vibrator singleVibrator = vm.getVibrator(vibratorIds[0]);

      if (singleStrength == 0)
      {
        singleVibrator.cancel();
        return true;
      }

      long[] timings = {0, 1000};
      int[]  amps    = {0, singleStrength};
      singleVibrator.vibrate(
          VibrationEffect.createWaveform(timings, amps, 0),
          new AudioAttributes.Builder()
              .setUsage(AudioAttributes.USAGE_GAME)
              .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
              .build());
      return true;
    }

    /* Dual-motor (or more) path: cancel cleanly when both are zero. */
    if (strongStrength == 0 && weakStrength == 0)
    {
      vm.cancel();
      return true;
    }

    /* Drive both motors with independent looping waveforms. */
    long[]          timings     = {0, 1000};
    VibrationEffect strongEffect = VibrationEffect.createWaveform(
        timings, new int[]{0, strongStrength}, 0);
    VibrationEffect weakEffect   = VibrationEffect.createWaveform(
        timings, new int[]{0, weakStrength},   0);

    CombinedVibration combined = CombinedVibration.startParallel()
        .addVibrator(vibratorIds[0], strongEffect)
        .addVibrator(vibratorIds[1], weakEffect)
        .combine();

    vm.vibrate(combined);

    Log.i("RetroActivity", "doVibrateJoypad id=" + id
        + " strong=" + strongStrength + " weak=" + weakStrength
        + " vibrators=" + vibratorIds.length);
    return true;
  }

  /**
   * Sends a dual-motor rumble report to a USB HID controller.
   *
   * Called from the native JNI thread during the rumble update cycle.
   * Returns true if the USB HID report was successfully sent so the C
   * caller can skip the VibratorManager fallback path.
   * Returns false if the device is not a supported USB HID target,
   * permission has not yet been granted, or the transfer failed.
   *
   * @param deviceId  Android InputDevice ID (from InputDevice.getId())
   * @param strong    Large/low-frequency motor amplitude (0–255)
   * @param weak      Small/high-frequency motor amplitude (0–255)
   * @return true if the HID report was sent successfully
   */
  public boolean doVibrateUSB(int deviceId, int strong, int weak)
  {
    UsbDevice usbDevice = findUsbDeviceForInputDevice(deviceId);
    if (usbDevice == null)
      return false;

    UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
    if (usbManager == null)
      return false;

    if (!usbManager.hasPermission(usbDevice))
    {
      requestUsbPermission(usbManager, usbDevice, deviceId);
      return false;
    }

    return sendUsbRumble(usbManager, usbDevice, deviceId, strong, weak);
  }

  /**
   * Finds the UsbDevice corresponding to the given Android InputDevice ID
   * by matching VID/PID. Returns null if the controller is not connected
   * via USB or is not a supported HID target.
   */
  private UsbDevice findUsbDeviceForInputDevice(int deviceId)
  {
    InputDevice inputDevice = InputDevice.getDevice(deviceId);
    if (inputDevice == null)
      return null;

    int vid = inputDevice.getVendorId();
    int pid = inputDevice.getProductId();

    if (!isSupportedUsbHidController(vid, pid))
      return null;

    UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
    if (usbManager == null)
      return null;

    for (UsbDevice device : usbManager.getDeviceList().values())
    {
      if (device.getVendorId() == vid && device.getProductId() == pid)
        return device;
    }

    return null;
  }

  /** Returns true for controllers whose USB HID output report format is known. */
  private boolean isSupportedUsbHidController(int vid, int pid)
  {
    return vid == VID_SONY
            && (pid == PID_DS4_V1 || pid == PID_DS4_V2 || pid == PID_DUALSENSE);
  }

  /**
   * Requests USB permission from the system. Shows a one-time dialog to the
   * user. The pending-flag guard ensures we only issue one request at a time
   * per device. The BroadcastReceiver clears the flag when the user responds.
   */
  private void requestUsbPermission(UsbManager usbManager,
          UsbDevice usbDevice, int deviceId)
  {
    Integer key = Integer.valueOf(deviceId);
    if (Boolean.TRUE.equals(mUsbPermissionPending.get(key)))
      return;
    mUsbPermissionPending.put(key, Boolean.TRUE);
    PendingIntent pi = PendingIntent.getBroadcast(this, 0,
            new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE);
    usbManager.requestPermission(usbDevice, pi);
    Log.i("RetroActivity", "doVibrateUSB: requested USB permission for "
            + usbDevice.getProductName() + " (inputDeviceId=" + deviceId + ")");
  }

  /**
   * Dispatches to the per-controller HID report builder. On transfer failure
   * the cached connection is closed so the next call will reopen it.
   */
  private boolean sendUsbRumble(UsbManager usbManager, UsbDevice usbDevice,
          int deviceId, int strong, int weak)
  {
    UsbDeviceConnection conn = getOrOpenUsbConnection(usbManager, usbDevice, deviceId);
    if (conn == null)
      return false;

    int vid = usbDevice.getVendorId();
    int pid = usbDevice.getProductId();

    try
    {
      if (vid == VID_SONY && (pid == PID_DS4_V1 || pid == PID_DS4_V2))
        return sendDs4Rumble(conn, strong, weak);
      if (vid == VID_SONY && pid == PID_DUALSENSE)
        return sendDualSenseRumble(conn, strong, weak);
    }
    catch (Exception e)
    {
      Log.e("RetroActivity", "doVibrateUSB: transfer failed, closing connection", e);
      closeUsbConnection(deviceId);
    }

    return false;
  }

  /** Returns the cached connection for this device, or opens a new one. */
  private UsbDeviceConnection getOrOpenUsbConnection(UsbManager usbManager,
          UsbDevice usbDevice, int deviceId)
  {
    Integer key = Integer.valueOf(deviceId);
    UsbDeviceConnection conn = mUsbConnections.get(key);
    if (conn != null)
      return conn;

    conn = usbManager.openDevice(usbDevice);
    if (conn == null)
    {
      Log.e("RetroActivity", "doVibrateUSB: failed to open USB device");
      return null;
    }

    mUsbConnections.put(key, conn);
    Log.i("RetroActivity", "doVibrateUSB: opened connection for inputDeviceId=" + deviceId);
    return conn;
  }

  private void closeUsbConnection(int deviceId)
  {
    UsbDeviceConnection conn = mUsbConnections.remove(Integer.valueOf(deviceId));
    if (conn != null)
      conn.close();
  }

  private void closeAllUsbConnections()
  {
    for (UsbDeviceConnection conn : mUsbConnections.values())
      conn.close();
    mUsbConnections.clear();
  }

  /* InputManager.InputDeviceListener — request USB permission as soon as a
   * supported controller is connected, before any rumble call can occur. */

  @Override
  public void onInputDeviceAdded(int deviceId)
  {
    UsbDevice usbDevice = findUsbDeviceForInputDevice(deviceId);
    if (usbDevice == null)
      return;
    UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
    if (usbManager == null || usbManager.hasPermission(usbDevice))
      return;
    requestUsbPermission(usbManager, usbDevice, deviceId);
  }

  /**
   * Proactively requests USB permission for any DS4/DualSense already connected.
   *
   * InputDeviceListener.onInputDeviceAdded() only fires for devices that connect
   * AFTER the listener is registered — it does not fire for devices that were
   * already connected at startup or while the activity was backgrounded.  This
   * method fills that gap by iterating all current InputDevice IDs and requesting
   * permission for any supported USB HID controller that does not yet have it.
   *
   * Safe to call multiple times; the mUsbPermissionPending guard ensures at most
   * one in-flight permission request per device at any time.
   */
  private void requestPermissionForConnectedSonyControllers()
  {
    UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
    if (usbManager == null)
      return;
    for (int deviceId : InputDevice.getDeviceIds())
    {
      UsbDevice usbDevice = findUsbDeviceForInputDevice(deviceId);
      if (usbDevice == null)
        continue;
      if (usbManager.hasPermission(usbDevice))
        continue;
      requestUsbPermission(usbManager, usbDevice, deviceId);
    }
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    /* Covers controllers connected at startup or while the activity was
     * backgrounded — cases where onInputDeviceAdded() never fires. */
    requestPermissionForConnectedSonyControllers();
  }

  @Override
  public void onInputDeviceChanged(int deviceId) { }

  @Override
  public void onInputDeviceRemoved(int deviceId)
  {
    /* Close any cached USB connection so we don't hold a stale handle. */
    closeUsbConnection(deviceId);
    mUsbPermissionPending.remove(Integer.valueOf(deviceId));
  }

  /**
   * DS4 USB rumble: 48-byte HID SET_REPORT (class request).
   * Report ID 0x05; rumble bytes at offsets 3 (right/weak) and 4 (left/strong).
   */
  private boolean sendDs4Rumble(UsbDeviceConnection conn, int strong, int weak)
  {
    byte[] report = new byte[48];
    report[0] = 0x05;                   /* Report ID */
    report[1] = 0x01;                   /* Flags: enable rumble */
    report[3] = (byte)(weak   & 0xFF);  /* Right motor (weak / high-freq) */
    report[4] = (byte)(strong & 0xFF);  /* Left  motor (strong / low-freq) */

    int result = conn.controlTransfer(
            0x21,    /* bmRequestType: Host→Device, Class, Interface */
            0x09,    /* bRequest: SET_REPORT */
            0x0305,  /* wValue: Report Type = Output (0x03), Report ID = 0x05 */
            0x0000,  /* wIndex: Interface 0 */
            report, report.length, 1000);

    return result == report.length;
  }

  /**
   * DualSense USB rumble: 48-byte HID SET_REPORT.
   * Report ID 0x02; rumble bytes at offsets 3 (right/weak) and 4 (left/strong).
   */
  private boolean sendDualSenseRumble(UsbDeviceConnection conn, int strong, int weak)
  {
    byte[] report = new byte[48];
    report[0] = 0x02;                   /* Report ID */
    report[1] = (byte)0xFF;             /* Enable compatible rumble */
    report[3] = (byte)(weak   & 0xFF);  /* Right motor */
    report[4] = (byte)(strong & 0xFF);  /* Left  motor */

    int result = conn.controlTransfer(
            0x21,
            0x09,
            0x0302,  /* wValue: Output report, ID 0x02 */
            0x0000,
            report, report.length, 1000);

    return result == report.length;
  }

  public void doHapticFeedback(int effect)
  {
    getWindow().getDecorView().performHapticFeedback(effect,
        HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING | HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING);
    Log.i("RetroActivity", "Haptic Feedback effect " + effect);
  }

  // Exiting cleanly from NDK seems to be nearly impossible.
  // Have to use exit(0) to avoid weird things happening, even with runOnUiThread() approaches.
  // Use a separate JNI function to explicitly trigger the readback.
  public void onRetroArchExit()
  {
      finish();
  }

  public int getVolumeCount()
  {
    int ret = 0;

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      StorageManager storageManager = (StorageManager) getApplicationContext().getSystemService(Context.STORAGE_SERVICE);
      List<StorageVolume> storageVolumeList = storageManager.getStorageVolumes();

      for (int i = 0; i < storageVolumeList.size(); i++) {
        ret++;
      }
      Log.i("RetroActivity", "volume count: " + ret);
    }

    return (int)ret;
  }

  public String getVolumePath(String input)
  {
    String ret = "";

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      int index = Integer.valueOf(input);
      int j = 0;

      StorageManager storageManager = (StorageManager) getApplicationContext().getSystemService(Context.STORAGE_SERVICE);
      List<StorageVolume> storageVolumeList = storageManager.getStorageVolumes();

      for (int i = 0; i < storageVolumeList.size(); i++) {
        if (i == j) {
          ret = String.valueOf(storageVolumeList.get(index).getDirectory());
        }
      }
      Log.i("RetroActivity", "volume path: " + ret);
    }

    return ret;
  }

  public String[] getPersistedSafTrees()
  {
    if (Build.VERSION.SDK_INT >= 19)
    {
      List<android.content.UriPermission> uriPermissions = getContentResolver().getPersistedUriPermissions();
      List<String> trees = new ArrayList<>();
      for (android.content.UriPermission uriPermission : uriPermissions)
      {
        Uri uri = uriPermission.getUri();
        if (uri != null)
          trees.add(uri.toString());
      }
      return trees.toArray(new String[0]);
    }
    else
      return new String[0];
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

  /**
   * Checks if this version of RetroArch is a Play Store build.
   *
   * @return true if this is a Play Store build, false otherwise
   */
  public boolean isPlayStoreBuild() {
    Log.i("RetroActivity", "isPlayStoreBuild: " + BuildConfig.PLAY_STORE_BUILD);

    return BuildConfig.PLAY_STORE_BUILD;
  }

  /**
   * Gets the list of available cores that can be downloaded as Dynamic Feature Modules.
   *
   * @return the list of available cores
   */
  public String[] getAvailableCores() {
    int id = getResources().getIdentifier("module_names_" + Build.CPU_ABI.replace('-', '_'), "array", getPackageName());

    String[] returnVal = getResources().getStringArray(id);
    Log.i("RetroActivity", "getAvailableCores: " + Arrays.toString(returnVal));
    return returnVal;
  }

  /**
   * Gets the list of cores that are currently installed as Dynamic Feature Modules.
   *
   * @return the list of installed cores
   */
  public String[] getInstalledCores() {
    String[] modules = PlayCoreManager.getInstance().getInstalledModules();
    List<String> cores = new ArrayList<>();
    List<String> availableCores = Arrays.asList(getAvailableCores());

    SharedPreferences prefs = UserPreferences.getPreferences(this);

    for(int i = 0; i < modules.length; i++) {
      String coreName = unsanitizeCoreName(modules[i]);
      if(!prefs.getBoolean("core_deleted_" + coreName, false)
              && availableCores.contains(coreName)) {
        cores.add(coreName);
      }
    }

    String[] returnVal = cores.toArray(new String[0]);
    Log.i("RetroActivity", "getInstalledCores: " + Arrays.toString(returnVal));
    return returnVal;
  }

  /**
   * Asks the system to download a core.
   *
   * @param coreName Name of the core to install
   */
  public void downloadCore(final String coreName) {
    Log.i("RetroActivity", "downloadCore: " + coreName);

    SharedPreferences prefs = UserPreferences.getPreferences(this);
    prefs.edit().remove("core_deleted_" + coreName).apply();

    PlayCoreManager.getInstance().downloadCore(coreName);
  }

  /**
   * Asks the system to delete a core.
   *
   * Note that the actual module deletion will not happen immediately (the OS will delete
   * it whenever it feels like it), but the symlink will still be immediately removed.
   *
   * @param coreName Name of the core to delete
   */
  public void deleteCore(String coreName) {
    Log.i("RetroActivity", "deleteCore: " + coreName);

    String newFilename = getCorePath() + coreName + "_libretro_android.so";
    new File(newFilename).delete();

    SharedPreferences prefs = UserPreferences.getPreferences(this);
    prefs.edit().putBoolean("core_deleted_" + coreName, true).apply();

    PlayCoreManager.getInstance().deleteCore(coreName);
  }



  /////////////// JNI methods ///////////////



  /**
   * Called when a core install is initiated.
   *
   * @param coreName Name of the core that the install is initiated for.
   * @param successful true if success, false if failure
   */
  public native void coreInstallInitiated(String coreName, boolean successful);

  /**
   * Called when the status of a core install has changed.
   *
   * @param coreNames Names of all cores that are currently being downloaded.
   * @param status One of INSTALL_STATUS_DOWNLOADING, INSTALL_STATUS_INSTALLING,
   *               INSTALL_STATUS_INSTALLED, or INSTALL_STATUS_FAILED
   * @param bytesDownloaded Number of bytes downloaded.
   * @param totalBytesToDownload Total number of bytes to download.
   */
  public native void coreInstallStatusChanged(String[] coreNames, int status, long bytesDownloaded, long totalBytesToDownload);

  /**
   * Called when the user grants access to a Storage Access Framework tree.
   */
  public native void safTreeAdded(String tree);



  /////////////// Private methods ///////////////



  /**
   * Sanitizes a core name so that it can be used when dealing with
   * Dynamic Feature Modules. Needed because Gradle modules cannot use
   * dashes, but we have at least one core name ("mesen-s") that uses them.
   *
   * @param coreName Name of the core to sanitize.
   * @return The sanitized core name.
   */
  public String sanitizeCoreName(String coreName) {
    return "core_" + coreName.replace('-', '_');
  }

  /**
   * Unsanitizes a core name from its module name.
   *
   * @param coreName Name of the core to unsanitize.
   * @return The unsanitized core name.
   */
  public String unsanitizeCoreName(String coreName) {
    if(coreName.equals("core_mesen_s")) {
      return "mesen-s";
    }

    return coreName.substring(5);
  }

  /**
   * Gets the path to the RetroArch cores directory.
   *
   * @return The path to the RetroArch cores directory
   */
  private String getCorePath() {
    String path = getApplicationInfo().dataDir + "/cores/";
    new File(path).mkdirs();

    return path;
  }

  /**
   * Cleans up existing symlinks before new ones are created.
   */
  private void cleanupSymlinks() {
    if(Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

    File[] files = new File(getCorePath()).listFiles();
    for(int i = 0; i < files.length; i++) {
      try {
        Os.readlink(files[i].getAbsolutePath());
        files[i].delete();
      } catch (Exception e) {
        // File is not a symlink, so don't delete.
      }
    }
  }

  /**
   * Triggers a symlink update in the known places that Dynamic Feature Modules
   * are installed to.
   */
  public void updateSymlinks() {
    if(!isPlayStoreBuild()) return;

    traverseFilesystem(getFilesDir());
    traverseFilesystem(new File(getApplicationInfo().nativeLibraryDir));
  }

  /**
   * Traverse the filesystem, looking for native libraries.
   * Symlinks any libraries it finds to the main RetroArch "cores" folder,
   * updating any existing symlinks with the correct path to the native libraries.
   *
   * This is necessary because Dynamic Feature Modules are first downloaded
   * and installed to a temporary location on disk, before being moved
   * to a more permanent location by the system at a later point.
   *
   * This could probably be done in native code instead, if that's preferred.
   *
   * @param file The parent directory of the tree to traverse.
   * @param cores List of cores to update.
   * @param filenames List of filenames to update.
   */
  private void traverseFilesystem(File file) {
    if(Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

    File[] list = file.listFiles();
    if(list == null) return;

    List<String> availableCores = Arrays.asList(getAvailableCores());

    // Check each file in a directory to see if it's a native library.
    for(int i = 0; i < list.length; i++) {
      File child = list[i];
      String name = child.getName();

      if(name.startsWith("lib") && name.endsWith(".so") && !name.contains("retroarch-activity")) {
        // Found a native library!
        String core = name.subSequence(3, name.length() - 3).toString();
        String filename = child.getAbsolutePath();

        SharedPreferences prefs = UserPreferences.getPreferences(this);
        if(!prefs.getBoolean("core_deleted_" + core, false)
                && availableCores.contains(core)) {
          // Generate the destination filename and delete any existing symlinks / cores
          String newFilename = getCorePath() + core + "_libretro_android.so";
          new File(newFilename).delete();

          try {
            Os.symlink(filename, newFilename);
          } catch (Exception e) {
            // Symlink failed to be created. Should never happen.
          }
        }
      } else if(file.isDirectory()) {
        // Found another directory, so traverse it
        traverseFilesystem(child);
      }
    }
  }

  public boolean isScreenReaderEnabled() {
    AccessibilityManager accessibilityManager = (AccessibilityManager) getSystemService(ACCESSIBILITY_SERVICE);
    boolean isAccessibilityEnabled = accessibilityManager.isEnabled();
    boolean isExploreByTouchEnabled = accessibilityManager.isTouchExplorationEnabled();
    return isAccessibilityEnabled && isExploreByTouchEnabled;
  }

  public void accessibilitySpeak(String message) {
    getWindow().getDecorView().announceForAccessibility(message);
  }
}
