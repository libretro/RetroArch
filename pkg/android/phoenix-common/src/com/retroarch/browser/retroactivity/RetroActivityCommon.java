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
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.system.Os;
import android.view.accessibility.AccessibilityManager;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.Surface;
import android.graphics.Point;
import android.view.Display;
import android.view.WindowManager;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.app.UiModeManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Environment;
import android.os.PowerManager;
import android.os.CombinedVibration;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.os.VibratorManager;
import android.util.Log;
import android.util.SparseArray;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.EditText;
import android.widget.TextView;


import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
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
  /* Startup permission flow; distinct from the in-session all-files
   * re-ask loop on requestCode 124, which restarts the app on grant. */
  private static final int REQUEST_CODE_STARTUP_ALL_FILES = 125;
  private static final int REQUEST_CODE_STARTUP_PERMISSIONS = 126;
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

  /* Resolution cache for findUsbDeviceForInputDevice, including negative
   * results.  doVibrateUSB runs for every controller on every rumble
   * amplitude change, and an uncached miss costs an InputDevice binder
   * lookup plus a full UsbManager.getDeviceList() marshal each time -
   * per frame during rumble ramps on non-USB controllers.  Entries are
   * invalidated from the InputDeviceListener callbacks.  Accessed from
   * both the JNI rumble thread and the main thread. */
  private final Map<Integer, UsbDevice> mUsbDeviceCache =
          new HashMap<Integer, UsbDevice>();

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

  /* Startup gate bookkeeping.  The native thread is held on one gate
   * until both the storage permission has been resolved and the core
   * symlink pass has finished; whichever of the two lands last
   * releases it.  All fields are guarded by mStartupGateLock. */
  private final Object mStartupGateLock = new Object();
  private boolean mStartupPermissionResolved;
  private boolean mStartupPermissionGranted;
  private boolean mStartupSymlinksDone;
  private boolean mStartupGateReleased;

  /* Serializes the symlink pass against the one PlayCoreManager runs
   * after a module install, so two passes never delete and recreate
   * the same links at once. */
  private final Object mSymlinkLock = new Object();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    /* The symlink pass walks the app data and native library trees,
     * which is disk I/O that does not belong on the UI thread.  Run
     * it on a worker and let it release its half of the startup gate
     * when done; the native side keeps waiting for it, so cores are
     * still in place before native core enumeration runs. */
    new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          cleanupSymlinks();
          updateSymlinks();
        } finally {
          startupSymlinksDone();
        }
      }
    }, "RetroArch-symlinks").start();

    if (Build.VERSION.SDK_INT >= 33) {
      registerReceiver(
        mUsbPermissionReceiver,
        new IntentFilter(ACTION_USB_PERMISSION),
        4 /* Context.RECEIVER_NOT_EXPORTED */
      );
    } else {
      registerReceiver(
        mUsbPermissionReceiver,
        new IntentFilter(ACTION_USB_PERMISSION)
      );
    }
    ((InputManager) getSystemService(Context.INPUT_SERVICE))
            .registerInputDeviceListener(this, null);
    /* Bind the hardware volume keys to the game audio stream;
     * previously done by the Java launcher activity. */
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    PlayCoreManager.getInstance().onCreate(this);
    super.onCreate(savedInstanceState);

    /* super.onCreate has loaded the native library and started the
     * native thread; it now waits on the permission gate until this
     * resolves. */
    resolveStartupPermissions();
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

  /** Current storage permission state for this SDK level. */
  private boolean hasStoragePermission()
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
      return android.os.Environment.isExternalStorageManager();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
      return checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
              == android.content.pm.PackageManager.PERMISSION_GRANTED
          && checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
              == android.content.pm.PackageManager.PERMISSION_GRANTED;
    return true;
  }

  /**
   * Records the startup permission outcome and releases the native
   * gate if the symlink pass has already finished.  Runs on the UI
   * thread, after super.onCreate has loaded the native library.
   */
  private void startupPermissionResolved(boolean granted)
  {
    synchronized (mStartupGateLock)
    {
      mStartupPermissionResolved = true;
      mStartupPermissionGranted  = granted;
      releaseStartupGateLocked();
    }
  }

  /**
   * Records completion of the symlink pass and releases the native
   * gate if the permission outcome is already known.  Runs on the
   * symlink worker; the native library is guaranteed loaded by then
   * because the permission outcome is only recorded after
   * super.onCreate.
   */
  private void startupSymlinksDone()
  {
    synchronized (mStartupGateLock)
    {
      mStartupSymlinksDone = true;
      releaseStartupGateLocked();
    }
  }

  /** Caller holds mStartupGateLock. */
  private void releaseStartupGateLocked()
  {
    if (mStartupGateReleased
        || !mStartupPermissionResolved
        || !mStartupSymlinksDone)
      return;
    mStartupGateReleased = true;
    permissionsResolved(mStartupPermissionGranted);
  }

  /**
   * Resolves the startup storage permission and releases the native
   * startup gate.  A launch through the Java launcher (recognized by
   * its CONFIGFILE extra), a Play Store build, or an already-settled
   * permission resolves immediately; only a direct launch that is
   * missing the permission asks, and any outcome - grant, denial,
   * cancel - releases the gate, with denial falling back to
   * app-private storage.
   */
  private void resolveStartupPermissions()
  {
    boolean viaLauncher = getIntent() != null && getIntent().hasExtra("CONFIGFILE");

    if (viaLauncher || isPlayStoreBuild() || hasStoragePermission())
    {
      startupPermissionResolved(hasStoragePermission());
      return;
    }

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
    {
      new android.app.AlertDialog.Builder(this)
        .setMessage("RetroArch requires All Files Access permission to scan and load game ROMs from your storage.")
        .setPositiveButton(android.R.string.ok, new android.content.DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(android.content.DialogInterface dialog, int which)
          {
            try
            {
              Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
              intent.addCategory("android.intent.category.DEFAULT");
              intent.setData(Uri.parse(String.format("package:%s", getPackageName())));
              startActivityForResult(intent, REQUEST_CODE_STARTUP_ALL_FILES);
            }
            catch (Exception e)
            {
              try
              {
                Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivityForResult(intent, REQUEST_CODE_STARTUP_ALL_FILES);
              }
              catch (Exception e2)
              {
                startupPermissionResolved(false);
              }
            }
          }
        })
        .setNegativeButton(android.R.string.cancel, new android.content.DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(android.content.DialogInterface dialog, int which)
          {
            startupPermissionResolved(false);
          }
        })
        .setOnCancelListener(new android.content.DialogInterface.OnCancelListener()
        {
          @Override
          public void onCancel(android.content.DialogInterface dialog)
          {
            startupPermissionResolved(false);
          }
        })
        .show();
      return;
    }

    /* Android 6 through 10: runtime storage permissions. */
    requestPermissions(new String[] {
        android.Manifest.permission.READ_EXTERNAL_STORAGE,
        android.Manifest.permission.WRITE_EXTERNAL_STORAGE },
        REQUEST_CODE_STARTUP_PERMISSIONS);
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    if (requestCode == REQUEST_CODE_STARTUP_PERMISSIONS)
    {
      startupPermissionResolved(hasStoragePermission());
      return;
    }
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent intent)
  {
    if (requestCode == REQUEST_CODE_STARTUP_ALL_FILES)
    {
      /* The Settings return carries no intent; resolve from the live
       * state and proceed either way. */
      startupPermissionResolved(hasStoragePermission());
      return;
    }

    if (requestCode == 124)
    {
      if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R)
      {
        if (android.os.Environment.isExternalStorageManager())
        {
          Intent restartIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());
          if (restartIntent != null)
          {
            restartIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(restartIntent);
          }
          finish();
        }
        else
        {
          try
          {
            Intent permIntent = new Intent(android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            permIntent.addCategory("android.intent.category.DEFAULT");
            permIntent.setData(Uri.parse(String.format("package:%s", getPackageName())));
            startActivityForResult(permIntent, 124);
          }
          catch (Exception e)
          {
            Intent permIntent = new Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
            startActivityForResult(permIntent, 124);
          }
        }
      }
      return;
    }

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

  /* Shared attributes for every vibrate() call.  Built lazily from
   * SDK-guarded paths only: AudioAttributes is API 21 and this class
   * must stay loadable on older devices.  The unsynchronized lazy init
   * is a benign race - the object is immutable and the reference write
   * is atomic. */
  private static AudioAttributes sVibrateAttrs;

  private static AudioAttributes vibrateAttrs()
  {
    if (sVibrateAttrs == null)
      sVibrateAttrs = new AudioAttributes.Builder()
          .setUsage(AudioAttributes.USAGE_GAME)
          .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
          .build();
    return sVibrateAttrs;
  }

  /* Per-device VibratorManager cache for the API 31+ dual-motor rumble
   * path.  Rumble amplitude changes arrive per frame during effect
   * ramps, and resolving InputDevice.getDevice() plus
   * getVibratorManager() costs a binder round trip each time; caching
   * them leaves only the vibrate() call itself on the hot path.
   * Referenced exclusively from SDK_INT >= S guarded code, so the
   * class is never loaded - and its API 31 member types never
   * resolved - on older devices. */
  @TargetApi(Build.VERSION_CODES.S)
  private static final class JoypadVibrators
  {
    private static final SparseArray<JoypadVibrators> sCache =
            new SparseArray<JoypadVibrators>();

    final VibratorManager vm;
    final int[] ids;

    private JoypadVibrators(VibratorManager vm, int[] ids)
    {
      this.vm = vm;
      this.ids = ids;
    }

    static JoypadVibrators get(int deviceId)
    {
      synchronized (sCache)
      {
        JoypadVibrators entry = sCache.get(deviceId);
        if (entry != null)
          return entry;

        InputDevice dev = InputDevice.getDevice(deviceId);
        if (dev == null)
          return null;

        VibratorManager vm = dev.getVibratorManager();
        entry = new JoypadVibrators(vm, vm.getVibratorIds());
        sCache.put(deviceId, entry);
        Log.i("RetroActivity", "doVibrateJoypad id=" + deviceId
            + ": " + entry.ids.length + " vibrator(s)");
        return entry;
      }
    }

    static void remove(int deviceId)
    {
      synchronized (sCache)
      {
        sCache.remove(deviceId);
      }
    }
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
      vibrator.vibrate(VibrationEffect.createWaveform(pattern, strengths, repeat), vibrateAttrs());
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
    JoypadVibrators cached = JoypadVibrators.get(id);
    if (cached == null)
      return false;

    VibratorManager vm  = cached.vm;
    int[]  vibratorIds  = cached.ids;

    if (vibratorIds.length == 0)
      return false;

    try
    {
      return doVibrateJoypadMotors(id, vm, vibratorIds,
          strongStrength, weakStrength);
    }
    catch (RuntimeException e)
    {
      /* Disconnect race: the cached manager can go stale between the
       * removal callback and a rumble call in flight.  Drop the entry
       * and let the caller take the single-vibrator fallback. */
      Log.w("RetroActivity", "doVibrateJoypad id=" + id
          + ": stale vibrator, invalidating", e);
      JoypadVibrators.remove(id);
      return false;
    }
  }

  @TargetApi(Build.VERSION_CODES.S)
  private boolean doVibrateJoypadMotors(int id, VibratorManager vm,
      int[] vibratorIds, int strongStrength, int weakStrength)
  {

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
          VibrationEffect.createWaveform(timings, amps, 0), vibrateAttrs());
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
    Integer key = Integer.valueOf(deviceId);

    synchronized (mUsbDeviceCache)
    {
      if (mUsbDeviceCache.containsKey(key))
        return mUsbDeviceCache.get(key);
    }

    UsbDevice resolved = resolveUsbDeviceForInputDevice(deviceId);

    synchronized (mUsbDeviceCache)
    {
      mUsbDeviceCache.put(key, resolved);
    }
    return resolved;
  }

  /** Uncached resolution behind findUsbDeviceForInputDevice. */
  private UsbDevice resolveUsbDeviceForInputDevice(int deviceId)
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
    invalidateDeviceCaches(deviceId);

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
  public void onInputDeviceChanged(int deviceId)
  {
    /* The vibrator topology can change with the device. */
    invalidateDeviceCaches(deviceId);
  }

  @Override
  public void onInputDeviceRemoved(int deviceId)
  {
    /* Close any cached USB connection so we don't hold a stale handle. */
    closeUsbConnection(deviceId);
    mUsbPermissionPending.remove(Integer.valueOf(deviceId));
    invalidateDeviceCaches(deviceId);
  }

  /** Drops every cached per-device lookup for an input device id. */
  private void invalidateDeviceCaches(int deviceId)
  {
    synchronized (mUsbDeviceCache)
    {
      mUsbDeviceCache.remove(Integer.valueOf(deviceId));
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
      JoypadVibrators.remove(deviceId);
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

  /* Live values of native-owned window settings, pushed over JNI on
   * config load and whenever they change (see
   * android_app_set_window_settings in platform_unix.c). */
  protected volatile boolean notchWriteOver = false;
  protected volatile boolean autoMouseGrab = false;

  public void setWindowSettings(boolean notchWriteOver, boolean autoMouseGrab)
  {
    boolean notchChanged = (this.notchWriteOver != notchWriteOver);

    this.notchWriteOver = notchWriteOver;
    this.autoMouseGrab = autoMouseGrab;

    if (notchChanged)
      onNotchSettingChanged();
  }

  /* Overridden where a display cutout mode is applied, so a changed
   * notch setting takes effect without waiting for the next resume. */
  protected void onNotchSettingChanged()
  {
  }

  @TargetApi(24)
  public void setSustainedPerformanceMode(boolean on)
  {
    sustainedPerformanceMode = on;

    if (Build.VERSION.SDK_INT >= 24) {
      if (isSustainedPerformanceModeSupported()) {
        /* Window attributes are the UI thread's to touch, and this is
         * called from the native thread (config load, settings change) as
         * well as from onResume(). Post and return: the native thread must
         * not wait on the UI thread's looper, which may be occupied by a
         * lifecycle transition that is itself waiting for the native thread
         * to acknowledge it. */
        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            Log.i("RetroActivity", "setting sustained performance mode to " + sustainedPerformanceMode);

            getWindow().setSustainedPerformanceMode(sustainedPerformanceMode);
          }
        });
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

  /**
   * The refresh rate the screen is actually running at, in Hz, or 0
   * if it cannot be determined.
   *
   * Three tiers, because the way to reach the Display changed twice
   * and this ships back to API 16:
   *
   *  - API 30+ : Activity.getDisplay().  getDefaultDisplay() is
   *              deprecated from here, and on a non-visual context it
   *              can hand back something that reports nothing useful.
   *  - API 23+ : the display's current Display.Mode, which is the
   *              exact rate of the mode in effect rather than a
   *              rounded nominal figure.
   *  - below   : Display.getRefreshRate(), present since API 1.
   *
   * Read live rather than cached: the rate changes underneath us when
   * the system switches mode, which is precisely what a user checking
   * this setting wants to see.
   */
  /**
   * The display modes the screen supports, packed for JNI as
   * {id, width, height, millihertz} per mode.
   *
   * Mode enumeration is API 23: Display.getSupportedModes() and
   * Display.Mode both arrived in Marshmallow, and nothing before it
   * exposes more than the size currently in effect.  So on older
   * devices - Lollipop, KitKat, and the API 16 floor the jelly-bean
   * tree still builds against - this reports a single mode
   * describing the current state, which is all the platform knows.
   * That keeps the caller's shape identical everywhere: there is
   * always at least one mode, and exactly one of them is current.
   *
   * Refresh rate is carried as millihertz because the caller's
   * config carries an integer rate alongside the float one, and
   * 59.94 must not become 59 on the way through.
   *
   * Returns null when nothing can be determined.
   */
  @SuppressWarnings("deprecation")
  public int[] getDisplayModes()
  {
    try
    {
      Display display = getActiveDisplay();

      if (display == null)
        return null;

      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
      {
        Display.Mode[] modes = display.getSupportedModes();

        if (modes != null && modes.length > 0)
        {
          int[] packed = new int[modes.length * 4];

          for (int i = 0; i < modes.length; i++)
          {
            packed[i * 4]     = modes[i].getModeId();
            packed[i * 4 + 1] = modes[i].getPhysicalWidth();
            packed[i * 4 + 2] = modes[i].getPhysicalHeight();
            packed[i * 4 + 3] = Math.round(modes[i].getRefreshRate() * 1000.0f);
          }

          return packed;
        }
      }

      /* Pre-Marshmallow, or a display that reports no modes: describe
       * the one state we can see. */
      {
        Point size = new Point();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
          display.getRealSize(size);
        else
          display.getSize(size);

        if (size.x <= 0 || size.y <= 0)
          return null;

        return new int[] {
          0, size.x, size.y,
          Math.round(display.getRefreshRate() * 1000.0f)
        };
      }
    }
    catch (Exception e)
    {
      Log.w("RetroActivityCommon", "getDisplayModes failed: " + e.getMessage());
      return null;
    }
  }

  /**
   * The mode id currently in effect, or 0 when it cannot be
   * determined - which is also the id reported for the synthesised
   * single mode on pre-Marshmallow devices, so the two agree.
   */
  public int getCurrentDisplayModeId()
  {
    try
    {
      Display display = getActiveDisplay();

      if (display == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
        return 0;

      Display.Mode mode = display.getMode();
      return (mode != null) ? mode.getModeId() : 0;
    }
    catch (Exception e)
    {
      Log.w("RetroActivityCommon",
            "getCurrentDisplayModeId failed: " + e.getMessage());
      return 0;
    }
  }

  /**
   * Asks the system for a display mode by id.  Returns false when
   * the request cannot be made, which on anything before API 23
   * means always: preferredDisplayModeId arrived with the mode API
   * itself, and there is no older way to ask.
   *
   * A request, not a guarantee - the system may keep the current
   * mode.  getCurrentDisplayModeId() is what says whether it took.
   */
  public boolean setDisplayModeId(final int modeId)
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return false;

    try
    {
      /* Window attributes are the UI thread's to touch. */
      runOnUiThread(new Runnable() {
        @Override
        public void run()
        {
          try
          {
            WindowManager.LayoutParams params = getWindow().getAttributes();

            params.preferredDisplayModeId     = modeId;

            /* Clearing the rate preference is what makes the mode
             * request effective.
             *
             * preferredRefreshRate is a SEPARATE request, and the two
             * contradict each other: a rate preference of 60 makes
             * the framework vote
             *   APP_REQUEST_REFRESH_RATE_RANGE [60, 60]
             *                     disableRefreshRateSwitching=true
             * which pins the display at 60 no matter which mode id
             * was asked for.  Selecting a 120 Hz mode then changed
             * nothing, because the app was still asking not to leave
             * 60.  The mode id names a rate already, so a rate
             * preference alongside it is redundant as well as
             * conflicting. */
            params.preferredRefreshRate       = 0.0f;

            getWindow().setAttributes(params);

            /* Remember it, so onResume() does not re-pin the rate and
             * undo this the next time the app comes forward. */
            explicitDisplayModeId             = modeId;

            Log.i("RetroActivityCommon",
                  "preferredDisplayModeId set to " + modeId
                  + " (rate preference cleared); display now reports"
                  + " mode " + getCurrentDisplayModeId());
          }
          catch (Exception e)
          {
            Log.w("RetroActivityCommon",
                  "setDisplayModeId failed: " + e.getMessage());
          }
        }
      });
      return true;
    }
    catch (Exception e)
    {
      Log.w("RetroActivityCommon",
            "setDisplayModeId failed: " + e.getMessage());
      return false;
    }
  }

  /**
   * Re-assert a chosen display mode when the activity comes back to
   * the foreground.
   *
   * The window is torn down when the app goes to the background, and
   * the mode chosen for it does not survive - which is why a 120 Hz
   * selection came back as 60 on returning from the Android UI.
   * Nothing was overriding it; it had simply gone with the window.
   */
  protected void reapplyDisplayMode()
  {
    if (explicitDisplayModeId == 0)
      return;
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
      return;

    try
    {
      WindowManager.LayoutParams params = getWindow().getAttributes();

      if (params.preferredDisplayModeId == explicitDisplayModeId
            && params.preferredRefreshRate == 0.0f)
        return;

      params.preferredDisplayModeId = explicitDisplayModeId;
      params.preferredRefreshRate   = 0.0f;
      getWindow().setAttributes(params);

      Log.i("RetroActivityCommon",
            "Re-applied display mode " + explicitDisplayModeId
            + " on resume; display reports mode "
            + getCurrentDisplayModeId());
    }
    catch (Exception e)
    {
      Log.w("RetroActivityCommon",
            "reapplyDisplayMode failed: " + e.getMessage());
    }
  }

  /**
   * Non-zero once a display mode has been chosen explicitly, which
   * means the refresh rate preference must not be re-applied: doing
   * so re-pins the rate and undoes the selection.
   */
  protected int explicitDisplayModeId = 0;

  /**
   * True when a display mode has been chosen explicitly, so a caller
   * knows not to express a competing rate preference.
   */
  public boolean hasExplicitDisplayMode()
  {
    return explicitDisplayModeId != 0;
  }

  /**
   * The Display this activity is on.  getDefaultDisplay() is
   * deprecated from API 30, and on a non-visual context it can hand
   * back something that reports nothing useful, so prefer the
   * activity's own display where it exists.
   */
  @SuppressWarnings("deprecation")
  private Display getActiveDisplay()
  {
    Display display = null;

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
      display = getDisplay();

    if (display == null)
    {
      WindowManager wm = (WindowManager)getSystemService(Context.WINDOW_SERVICE);
      if (wm != null)
        display = wm.getDefaultDisplay();
    }

    return display;
  }

  @SuppressWarnings("deprecation")
  public float getRefreshRate()
  {
    try
    {
      Display display = getActiveDisplay();

      if (display == null)
        return 0.0f;

      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
      {
        Display.Mode mode = display.getMode();
        if (mode != null)
          return mode.getRefreshRate();
      }

      return display.getRefreshRate();
    }
    catch (Exception e)
    {
      Log.w("RetroActivityCommon", "getRefreshRate failed: " + e.getMessage());
      return 0.0f;
    }
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
    int id = getResources().getIdentifier(
      "module_names_" + Build.CPU_ABI.replace('-', '_'),
      "array",
      getPackageName()
    );

    if (id == 0) {
      Log.w("RetroActivity", "No dynamic feature core list found for ABI: " + Build.CPU_ABI);
      return new String[0];
    }

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

  /////////////// System (IME) keyboard ///////////////

  /* A near-invisible EditText that proxies the system soft keyboard for the
   * menu's text entry, so users get clipboard paste and password managers
   * (which the built-in on-screen keyboard cannot offer). Committed/pasted
   * text is forwarded to native via onSystemKeyboardInput(). */
  private EditText keyboardEditText;
  private boolean  keyboardActive;
  private boolean  keyboardSuppressWatcher;

  /**
   * Raises the native system keyboard for menu text entry.
   *
   * Called from native code (the menu on-screen keyboard path). Runs the
   * UI work on the UI thread.
   *
   * @param label       Hint shown in the keyboard field, or null.
   * @param initialText Text to pre-fill the field with, or null.
   */
  public void showKeyboard(final String label, final String initialText) {
    runOnUiThread(new Runnable() {
      @Override public void run() {
        if (keyboardEditText == null) {
          keyboardEditText = new EditText(RetroActivityCommon.this) {
            @Override public boolean onKeyPreIme(int keyCode, KeyEvent event) {
              /* BACK while the keyboard is up = dismiss without confirming. */
              if (keyCode == KeyEvent.KEYCODE_BACK
                    && event.getAction() == KeyEvent.ACTION_UP
                    && keyboardActive) {
                hideKeyboard();
                onSystemKeyboardInput(null, true);
                return true;
              }
              return super.onKeyPreIme(keyCode, event);
            }
          };
          keyboardEditText.setSingleLine(true);
          keyboardEditText.setImeOptions(EditorInfo.IME_ACTION_DONE
                | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN);
          keyboardEditText.setAlpha(0.0f);

          keyboardEditText.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void afterTextChanged(Editable s) {
              if (keyboardActive && !keyboardSuppressWatcher)
                onSystemKeyboardInput(s.toString(), false);
            }
          });

          keyboardEditText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
              if (actionId == EditorInfo.IME_ACTION_DONE
                    || actionId == EditorInfo.IME_ACTION_GO
                    || actionId == EditorInfo.IME_ACTION_SEARCH
                    || actionId == EditorInfo.IME_ACTION_NEXT
                    || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                String text = v.getText().toString();
                hideKeyboard();
                onSystemKeyboardInput(text, true);
                return true;
              }
              return false;
            }
          });

          addContentView(keyboardEditText, new ViewGroup.LayoutParams(1, 1));
        }

        keyboardActive = true;
        keyboardSuppressWatcher = true;
        keyboardEditText.setText(initialText != null ? initialText : "");
        keyboardEditText.setSelection(keyboardEditText.getText().length());
        keyboardSuppressWatcher = false;
        if (label != null)
          keyboardEditText.setHint(label);
        keyboardEditText.setVisibility(View.VISIBLE);
        keyboardEditText.setFocusable(true);
        keyboardEditText.setFocusableInTouchMode(true);
        keyboardEditText.requestFocus();

        InputMethodManager imm = (InputMethodManager)
              getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null)
          imm.showSoftInput(keyboardEditText, InputMethodManager.SHOW_FORCED);
      }
    });
  }

  /**
   * Dismisses the system keyboard.
   *
   * Called from native code and after the user confirms or cancels.
   */
  public void hideKeyboard() {
    runOnUiThread(new Runnable() {
      @Override public void run() {
        if (keyboardEditText == null)
          return;
        keyboardActive = false;
        InputMethodManager imm = (InputMethodManager)
              getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null)
          imm.hideSoftInputFromWindow(keyboardEditText.getWindowToken(), 0);
        keyboardEditText.clearFocus();
        keyboardEditText.setVisibility(View.GONE);
      }
    });
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

  /**
   * Tells the native side that the startup storage-permission flow has
   * finished, releasing the startup gate.  granted reflects the current
   * permission state; startup proceeds either way, falling back to
   * app-private storage when denied.
   */
  public native void permissionsResolved(boolean granted);

  /**
   * Forwards system-keyboard text to native menu input.
   *
   * @param text     Current text, or null to cancel (keyboard dismissed).
   * @param finished true when the user confirmed (Done/Enter) or cancelled.
   */
  public native void onSystemKeyboardInput(String text, boolean finished);



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

    synchronized (mSymlinkLock) {
      File[] files = new File(getCorePath()).listFiles();
      if(files == null) return;
      for(int i = 0; i < files.length; i++) {
        try {
          Os.readlink(files[i].getAbsolutePath());
          files[i].delete();
        } catch (Exception e) {
          // File is not a symlink, so don't delete.
        }
      }
    }
  }

  /**
   * Triggers a symlink update in the known places that Dynamic Feature Modules
   * are installed to.
   */
  public void updateSymlinks() {
    if(!isPlayStoreBuild()) return;

    synchronized (mSymlinkLock) {
      traverseFilesystem(getFilesDir());
      traverseFilesystem(new File(getApplicationInfo().nativeLibraryDir));
    }
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
