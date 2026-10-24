/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.Presentation;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DeviceProductInfo;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.Choreographer;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import java.util.Arrays;
import java.util.Collections;

/**
 * The second display: publishes the display list to native code, and
 * shows a Presentation with a SurfaceView on the display native code
 * asks for. Everything here runs on the UI thread.
 */
@TargetApi(17)
final class SecondDisplayController implements DisplayManager.DisplayListener
{
  static final int FLAG_OWN      = 1;
  static final int FLAG_VALID    = 2;
  static final int FLAG_ON       = 4;
  static final int FLAG_PRIVATE  = 8;
  static final int FLAG_PHYSICAL = 16;

  private static final String TAG = "RetroArch";

  private final RetroActivityCommon activity;
  private final DisplayManager displayManager;
  private final Handler handler = new Handler(Looper.getMainLooper());
  /* The last published list, without the failure marking; its ids by
   * ordinal - 1 */
  private DisplayList published = new DisplayList(0);
  /* The display native code asked for, -1 for none */
  private int wantedId = -1;
  /* The request that asked for it; its Presentations' surfaces carry it */
  private int wantedRequest;
  /* Displays show() failed on, published invalid until the list
   * changes or onStart */
  private final SparseBooleanArray failedIds = new SparseBooleanArray();
  private boolean started;
  private boolean publishPending;
  private Presentation presentation;
  private int presentationId = -1;
  private Screen presentationScreen;
  /* The screen whose surface native code holds */
  private Screen surfaceOwner;
  /* What native code was last told of the Presentation's focus */
  private boolean presentationFocused;

  /* nativeMotion()'s axes, ANDROID_JOYSTICK_AXES' order */
  private static final int[] AXES = {
    MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z,
    MotionEvent.AXIS_RZ, MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y,
    MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER,
    MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_GAS
  };
  /* Sources native code reads as touches or a mouse, never as a stick
   * (android_input.c); compile-time constants, so safe below API 26 */
  private static final int POINTER_SOURCES = InputDevice.SOURCE_TOUCHPAD
      | InputDevice.SOURCE_TOUCHSCREEN | InputDevice.SOURCE_MOUSE_RELATIVE
      | InputDevice.SOURCE_STYLUS | InputDevice.SOURCE_MOUSE;
  private final float[] axisValues = new float[AXES.length];

  private final Choreographer.FrameCallback publishFrame =
      new Choreographer.FrameCallback() {
        @Override public void doFrame(long frameTimeNanos) {
          DisplayList list;
          publishPending = false;
          if (!started)
            return;
          list = listDisplays();
          /* A Presentation coming or going may report a change that
           * is none; failed displays must stay invalid through it */
          if (list.sameAs(published))
            return;
          failedIds.clear();
          publish(list);
        }
      };

  private final Runnable reshow = new Runnable() {
    @Override public void run() {
      if (started && wantedId >= 0)
        showOn(wantedId);
    }
  };

  /* Android cancels a Presentation itself when its display's metrics
   * change; our dismiss() clears presentation first */
  private final DialogInterface.OnDismissListener onDismiss =
      new DialogInterface.OnDismissListener() {
        @Override public void onDismiss(DialogInterface dialog) {
          int id        = presentationId;
          Screen screen = presentationScreen;
          if (presentation == null || dialog != presentation)
            return;
          presentation       = null;
          presentationId     = -1;
          presentationScreen = null;
          reportFocus(false);
          if (!started || wantedId != id)
            return;
          /* One that cancels before it draws would do so again */
          if (screen.hadSurface)
            handler.post(reshow);
          else
            showFailed(id, null);
        }
      };

  SecondDisplayController(RetroActivityCommon activity)
  {
    this.activity  = activity;
    displayManager = (DisplayManager)
        activity.getSystemService(Context.DISPLAY_SERVICE);
  }

  void onStart()
  {
    started = true;
    failedIds.clear();
    displayManager.registerDisplayListener(this, null);
    publish(listDisplays());
    if (wantedId >= 0)
      showOn(wantedId);
  }

  void onStop()
  {
    started = false;
    displayManager.unregisterDisplayListener(this);
    dismiss();
  }

  /** The Activity may be on another display: Android reports the old
   * one for a moment, so look one frame later. */
  void onConfigurationChanged()
  {
    publishSoon();
  }

  /** Shows the second display on display @ordinal of the last published
   * list, or hides it for 0. An ordinal whose display has gone is
   * ignored: the next publication routes again. @request numbers the
   * request. */
  void show(int ordinal, int request)
  {
    Display display;
    if (ordinal <= 0)
    {
      wantedId = -1;
      dismiss();
      return;
    }
    if (ordinal > published.ids.length)
      return;
    display = displayManager.getDisplay(published.ids[ordinal - 1]);
    if (display == null || !display.isValid())
      return;
    wantedId      = display.getDisplayId();
    wantedRequest = request;
    if (started)
      showOn(wantedId);
  }

  @Override public void onDisplayAdded(int displayId)
  {
    publishSoon();
  }

  @Override public void onDisplayRemoved(int displayId)
  {
    if (displayId == presentationId)
      dismiss();
    publishSoon();
  }

  @Override public void onDisplayChanged(int displayId)
  {
    publishSoon();
  }

  private void publishSoon()
  {
    if (publishPending)
      return;
    publishPending = true;
    Choreographer.getInstance().postFrameCallback(publishFrame);
  }

  /* As if no show() had failed */
  private DisplayList listDisplays()
  {
    Display[] displays = displayManager.getDisplays();
    Display own        = activity.getWindowManager().getDefaultDisplay();
    int ownId          = own != null
        ? own.getDisplayId() : Display.DEFAULT_DISPLAY;
    DisplayList list   = new DisplayList(displays.length);
    Point size         = new Point();

    for (int i = 0; i < displays.length; i++)
    {
      Display d   = displays[i];
      int f       = 0;
      list.ids[i] = d.getDisplayId();
      if (list.ids[i] == ownId)
        f |= FLAG_OWN;
      if (d.isValid())
        f |= FLAG_VALID;
      if (Build.VERSION.SDK_INT < 20 || d.getState() == Display.STATE_ON)
        f |= FLAG_ON;
      if ((d.getFlags() & Display.FLAG_PRIVATE) != 0)
        f |= FLAG_PRIVATE;
      if (Build.VERSION.SDK_INT >= 31)
      {
        int connection = ProductInfo.connection(d);
        if (connection >= 0)
        {
          f |= FLAG_PHYSICAL;
          list.connections[i] = connection;
        }
      }
      list.flags[i] = f;
      d.getRealSize(size);
      list.widths[i]  = size.x;
      list.heights[i] = size.y;
      list.names[i]   = d.getName();
    }
    return list;
  }

  private void publish(DisplayList list)
  {
    int[] flags = list.flags.clone();
    for (int i = 0; i < flags.length; i++)
      if (failedIds.get(list.ids[i]))
        flags[i] &= ~FLAG_VALID;
    published = list;
    nativeDisplaysChanged(list.connections, flags, list.widths,
        list.heights, list.names);
  }

  private void showOn(int displayId)
  {
    Display display;
    Presentation p;
    Window window;
    SurfaceView view;
    Screen screen;

    if (presentation != null && presentationId == displayId)
      return;
    dismiss();
    if ((display = displayManager.getDisplay(displayId)) == null)
      return;

    p      = new SecondPresentation(display);
    /* From Android 12 a Presentation has a window context of its own */
    if (Build.VERSION.SDK_INT < 31)
      updateMetrics(p.getResources(), display);
    window = p.getWindow();
    /* It takes focus, so a focus mode or a tap can give it the pad; it
     * never takes the soft keyboard itself */
    if (window != null)
      window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
          | WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
    /* BACK goes to native code; never cancel on it */
    p.setCancelable(false);
    view   = new SurfaceView(p.getContext());
    screen = new Screen(wantedRequest);
    view.setFocusable(false);
    view.setFocusableInTouchMode(false);
    view.setKeepScreenOn(true);
    view.getHolder().addCallback(screen);
    view.setOnTouchListener(screen);
    view.addOnLayoutChangeListener(screen);
    p.setContentView(view);
    p.setOnDismissListener(onDismiss);
    try
    {
      p.show();
    }
    catch (WindowManager.BadTokenException e)
    {
      showFailed(displayId, e);
      return;
    }
    catch (WindowManager.InvalidDisplayException e)
    {
      showFailed(displayId, e);
      return;
    }
    presentation       = p;
    presentationId     = displayId;
    presentationScreen = screen;
  }

  /* Before Android 12 a display's cached resources keep its old
   * metrics, and a new Presentation cancels itself on them in onStart */
  private static void updateMetrics(Resources res, Display display)
  {
    DisplayMetrics now = new DisplayMetrics();
    DisplayMetrics had = res.getDisplayMetrics();
    Configuration config;

    display.getMetrics(now);
    if (now.widthPixels == had.widthPixels
        && now.heightPixels == had.heightPixels
        && now.densityDpi == had.densityDpi
        && now.xdpi == had.xdpi
        && now.ydpi == had.ydpi)
      return;
    config                       = new Configuration(res.getConfiguration());
    config.densityDpi            = now.densityDpi;
    config.screenWidthDp         = (int)(now.widthPixels  / now.density);
    config.screenHeightDp        = (int)(now.heightPixels / now.density);
    config.smallestScreenWidthDp = Math.min(config.screenWidthDp,
        config.screenHeightDp);
    config.orientation           = now.widthPixels > now.heightPixels
        ? Configuration.ORIENTATION_LANDSCAPE
        : Configuration.ORIENTATION_PORTRAIT;
    res.updateConfiguration(config, now);
  }

  /* Native code counts the Presentation's focus with the Activity's */
  private void reportFocus(boolean focused)
  {
    if (focused == presentationFocused)
      return;
    presentationFocused = focused;
    nativeFocusChanged(focused);
  }

  /* Every key goes to native code, as the Activity's do; the answer is
   * native code's, so a key it uses never comes back as a fallback key
   * (BUTTON_B as BACK, BUTTON_MODE as HOME) */
  private boolean relayKey(KeyEvent e)
  {
    nativeKey(e.getDeviceId(), e.getSource(), e.getAction(),
        e.getKeyCode(), e.getFlags(), e.getMetaState(), e.getEventTime());
    return nativeUses(e);
  }

  /* android_input.c leaves the volume keys to the system, and the soft
   * keyboard's keys but BACK to the text field while that is open */
  private boolean nativeUses(KeyEvent e)
  {
    int code = e.getKeyCode();
    if (code == KeyEvent.KEYCODE_VOLUME_UP
        || code == KeyEvent.KEYCODE_VOLUME_DOWN)
      return false;
    return code == KeyEvent.KEYCODE_BACK
        || !activity.isKeyboardActive()
        || ((e.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) == 0
            && e.getDeviceId() != KeyCharacterMap.VIRTUAL_KEYBOARD);
  }

  /* A stick or hat move goes to native code, as the Activity's do;
   * touches and mouse moves stay with the view */
  private boolean relayMotion(MotionEvent e)
  {
    int index = e.getActionIndex();
    if ((e.getSource() & POINTER_SOURCES) != 0)
      return false;
    /* Sent as the focus leaves; it repeats the last axes */
    if (e.getActionMasked() == MotionEvent.ACTION_CANCEL)
      return true;
    for (int i = 0; i < AXES.length; i++)
      axisValues[i] = e.getAxisValue(AXES[i], index);
    nativeMotion(e.getDeviceId(), e.getSource(), axisValues);
    return true;
  }

  private void showFailed(int displayId, RuntimeException e)
  {
    Log.w(TAG, "[SecondDisplay] Cannot show on display " + displayId, e);
    failedIds.put(displayId, true);
    publish(listDisplays());
  }

  private void dismiss()
  {
    Presentation p     = presentation;
    presentation       = null;
    presentationId     = -1;
    presentationScreen = null;
    /* With one focus for all displays, it would pass to this
     * display's launcher; take it back first */
    if (p != null && presentationFocused && started)
      takeFocusBack();
    reportFocus(false);
    if (p == null)
      return;
    /* The display may already be gone */
    try
    {
      p.dismiss();
    }
    catch (RuntimeException e)
    {
    }
  }

  private void takeFocusBack()
  {
    ActivityManager am = (ActivityManager)
        activity.getSystemService(Context.ACTIVITY_SERVICE);
    try
    {
      am.moveTaskToFront(activity.getTaskId(), 0);
    }
    catch (RuntimeException e)
    {
      Log.w(TAG, "[SecondDisplay] Cannot take the focus back", e);
    }
  }

  /** A display list, one entry per display in getDisplays() order. */
  private static final class DisplayList
  {
    final int[] ids;
    final int[] connections;
    final int[] flags;
    final int[] widths;
    final int[] heights;
    final String[] names;

    DisplayList(int count)
    {
      ids         = new int[count];
      connections = new int[count];
      flags       = new int[count];
      widths      = new int[count];
      heights     = new int[count];
      names       = new String[count];
    }

    boolean sameAs(DisplayList o)
    {
      return Arrays.equals(ids, o.ids)
          && Arrays.equals(connections, o.connections)
          && Arrays.equals(flags, o.flags)
          && Arrays.equals(widths, o.widths)
          && Arrays.equals(heights, o.heights)
          && Arrays.equals(names, o.names);
    }
  }

  /** One Presentation's view: its surface, touches and gesture area. */
  private final class Screen implements SurfaceHolder.Callback,
      View.OnTouchListener, View.OnLayoutChangeListener
  {
    /* The show request its Presentation was made for */
    private final int request;
    /* nativeSurfaceChanged()'s, 0 while there is none */
    private int generation;
    /* Native code has taken a surface of it */
    private boolean hadSurface;

    Screen(int request)
    {
      this.request = request;
    }

    @Override public void surfaceCreated(SurfaceHolder holder) { }

    /* Native code compares the ANativeWindow, which changes under the
     * same Surface object, so every call goes through */
    @Override public void surfaceChanged(SurfaceHolder holder, int format,
        int width, int height)
    {
      int g = nativeSurfaceChanged(holder.getSurface(), width, height,
          request);
      generation = g;
      if (g > 0)
      {
        surfaceOwner = this;
        hadSurface   = true;
      }
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder)
    {
      generation = 0;
      /* A later view's surface has already replaced this one's */
      if (surfaceOwner != this)
        return;
      surfaceOwner = null;
      nativeSurfaceDestroyed();
    }

    @Override public boolean onTouch(View v, MotionEvent e)
    {
      int action = e.getActionMasked();
      int index  = e.getActionIndex();
      int count  = e.getPointerCount();
      int g      = generation;
      if (g <= 0)
        return true;
      for (int i = 0; i < count; i++)
      {
        boolean up = action == MotionEvent.ACTION_UP
            || action == MotionEvent.ACTION_CANCEL
            || (action == MotionEvent.ACTION_POINTER_UP && i == index);
        nativeTouch(e.getPointerId(i), (int)e.getX(i), (int)e.getY(i),
            !up, g);
      }
      return true;
    }

    @Override public void onLayoutChange(View v, int left, int top,
        int right, int bottom, int oldLeft, int oldTop, int oldRight,
        int oldBottom)
    {
      if (Build.VERSION.SDK_INT >= 29)
        v.setSystemGestureExclusionRects(Collections.singletonList(
            new Rect(0, 0, right - left, bottom - top)));
    }
  }

  /** The second display's window: it takes focus, and passes the keys
   * and sticks it gets to native code. */
  private final class SecondPresentation extends Presentation
  {
    SecondPresentation(Display display)
    {
      super(activity, display);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e)
    {
      return relayKey(e) || super.dispatchKeyEvent(e);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent e)
    {
      return relayMotion(e) || super.dispatchGenericMotionEvent(e);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus)
    {
      super.onWindowFocusChanged(hasFocus);
      if (this == presentation)
        reportFocus(hasFocus);
    }
  }

  /** API 31: a real panel or monitor has product info; casts and
   * screen shares have none. Its own class, so older systems never
   * load the type. */
  @TargetApi(31)
  private static final class ProductInfo
  {
    /* The connection type, -1 without product info */
    static int connection(Display display)
    {
      DeviceProductInfo info = display.getDeviceProductInfo();
      return info != null ? info.getConnectionToSinkType() : -1;
    }
  }

  static native void nativeDisplaysChanged(int[] connections, int[] flags,
      int[] widths, int[] heights, String[] names);
  static native int nativeSurfaceChanged(Surface surface, int width,
      int height, int request);
  static native void nativeSurfaceDestroyed();
  static native void nativeTouch(int pointerId, int x, int y,
      boolean down, int generation);
  static native void nativeKey(int deviceId, int source, int action,
      int keyCode, int flags, int metaState, long eventTime);
  static native void nativeMotion(int deviceId, int source,
      float[] axes);
  static native void nativeFocusChanged(boolean focused);
}
