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
import android.app.Presentation;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DeviceProductInfo;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.util.Log;
import android.view.Choreographer;
import android.view.Display;
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
  /* The last published list; its ids by ordinal - 1 */
  private DisplayList published = new DisplayList(0);
  /* The display native code asked for, -1 for none */
  private int wantedId = -1;
  /* The request that asked for it; its Presentations' surfaces carry it */
  private int wantedRequest;
  /* A display show() failed on, published invalid until the list
   * changes */
  private int failedId = -1;
  private boolean started;
  private boolean publishPending;
  private Presentation presentation;
  private int presentationId = -1;
  /* The screen whose surface native code holds */
  private Screen surfaceOwner;

  private final Choreographer.FrameCallback publishFrame =
      new Choreographer.FrameCallback() {
        @Override public void doFrame(long frameTimeNanos) {
          DisplayList list;
          publishPending = false;
          if (!started)
            return;
          list = listDisplays();
          /* A Presentation coming or going may report a change that
           * is none; the failed display must stay invalid through it */
          if (list.sameAs(published))
            return;
          failedId = -1;
          publish(list);
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
      if (list.ids[i] == failedId)
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

    p      = new Presentation(activity, display);
    window = p.getWindow();
    /* Touching it must not take focus or the gamepad from the Activity */
    if (window != null)
      window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
          | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
    view   = new SurfaceView(p.getContext());
    screen = new Screen(wantedRequest);
    view.setFocusable(false);
    view.setFocusableInTouchMode(false);
    view.setKeepScreenOn(true);
    view.getHolder().addCallback(screen);
    view.setOnTouchListener(screen);
    view.addOnLayoutChangeListener(screen);
    p.setContentView(view);
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
    presentation   = p;
    presentationId = displayId;
  }

  private void showFailed(int displayId, RuntimeException e)
  {
    Log.w(TAG, "[SecondDisplay] Cannot show on display " + displayId, e);
    failedId = displayId;
    publish(listDisplays());
  }

  private void dismiss()
  {
    Presentation p = presentation;
    presentation   = null;
    presentationId = -1;
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
        surfaceOwner = this;
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
}
