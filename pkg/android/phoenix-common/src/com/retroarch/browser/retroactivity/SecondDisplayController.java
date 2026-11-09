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
import android.app.ActivityOptions;
import android.app.Presentation;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DeviceProductInfo;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.Choreographer;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import java.lang.reflect.Method;
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
  static final int FLAG_REFUSED  = 32;
  static final int FLAG_DEFAULT  = 64;
  static final int FLAG_EXTERNAL = 128;
  static final int FLAG_INTERNAL = 256;

  private static final String TAG = "RetroArch";
  private static final int REFOCUS_FIRST_MS   = 250;
  private static final int REFOCUS_RETRY_MS   = 250;
  private static final int REFOCUS_TIMEOUT_MS = 1000;
  private static final int TOP_PAIR_MS        = 1000;
  private static final int TOP_GAIN_WAIT_MS   = 250;
  private static final int QUIET_MARK_MS      = 1000;
  /* Display.TYPE_INTERNAL and TYPE_EXTERNAL, hidden */
  private static final int DISPLAY_TYPE_INTERNAL = 1;
  private static final int DISPLAY_TYPE_EXTERNAL = 2;
  /* The hidden Display.getType(), looked up once; null if it is missing */
  private static Method getType;
  private static boolean getTypeLooked;
  private static boolean getTypeLogged;

  private final RetroActivityCommon activity;
  private final DisplayManager displayManager;
  /* For bare(): managers whose contexts are on two different displays */
  private final DisplayManager[] bareManagers = new DisplayManager[2];
  private final int[] bareIds = new int[2];
  private final Handler handler = new Handler(Looper.getMainLooper());
  /* The last published list, without the failure marking; its ids by
   * ordinal - 1 */
  private DisplayList published = new DisplayList(0);
  /* The display native code asked for, -1 for none */
  private int wantedId = -1;
  /* The request that asked for it; its Presentations' surfaces carry it */
  private int wantedRequest;
  /* Displays show() failed on, published refused until onStart or a
   * list change other than RetroArch's own display */
  private final SparseBooleanArray failedIds = new SparseBooleanArray();
  private boolean started;
  private boolean publishPending;
  private Presentation presentation;
  private int presentationId = -1;
  private Screen presentationScreen;
  /* The screen whose surface native code holds */
  private Screen surfaceOwner;
  /* The display a move went to, -1 for none, until the Activity has
   * held the focus since */
  private int movedId = -1;
  /* RetroArch's display went away during this mark, so Android may
   * have put it on the default display rather than movedId */
  private boolean fellBack;
  /* Moves started, so a gain's check never ends a later move */
  private int moves;
  /* refocusAfterMove() has made the window unfocusable */
  private boolean refocusing;
  /* The last focus event was a loss no hold asked for. Only then may a
   * publication arm the hold: armed before a gain, it holds that gain
   * back */
  private boolean lost;
  /* The first hold ran out and its flag is clear: a loss now is its
   * late answer */
  private boolean retrying;
  /* refocusAfterMove() or topGainWait waits for a gain already on its
   * way */
  private boolean armPending;
  /* Focus events, so a wait from before the latest one never arms */
  private int arms;
  /* The Activity stopped being the top resumed one while focused */
  private boolean topLostFocused;
  /* uptimeMillis() of the last top-resumed loss, -1 since onStop */
  private long topLostAt = -1;

  private final Choreographer.FrameCallback publishFrame =
      new Choreographer.FrameCallback() {
        @Override public void doFrame(long frameTimeNanos) {
          DisplayList list;
          publishPending = false;
          if (!started)
            return;
          list = listDisplays();
          /* A Presentation coming or going may report a change that
           * is none; failed displays must stay refused through it */
          if (list.sameAs(published))
          {
            if (lost)
              refocusAfterMove();
            return;
          }
          if (!list.sameAsBut(published, FLAG_OWN))
            failedIds.clear();
          publishMarked(list);
        }
      };

  /* Android had no focus to take: the loss was real */
  private final Runnable refocusTimeout = new Runnable() {
    @Override public void run() {
      endRefocus();
      movedId  = -1;
      fellBack = false;
    }
  };

  /* Android may be waiting to focus the window rather than taking it */
  private final Runnable refocusFirstTimeout = new Runnable() {
    @Override public void run() {
      endRefocus();
      retrying = true;
      handler.postDelayed(refocusRetry, REFOCUS_RETRY_MS);
    }
  };

  private final Runnable refocusRetry = new Runnable() {
    @Override public void run() {
      retrying = false;
      if (lost && mayRefocus())
        refocus(REFOCUS_TIMEOUT_MS, refocusTimeout);
    }
  };

  /* A top regain read unfocused whose focus gain is late */
  private final Runnable topGainWait = new Runnable() {
    @Override public void run() {
      armPending = false;
      if (lost && mayRefocus())
        refocus(REFOCUS_FIRST_MS, refocusFirstTimeout);
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
    /* A loss heard after onStop went with the stop */
    lost    = false;
    failedIds.clear();
    displayManager.registerDisplayListener(this, null);
    publish(listDisplays());
    if (wantedId >= 0)
      showOn(wantedId);
  }

  void onStop()
  {
    started        = false;
    lost           = false;
    topLostFocused = false;
    topLostAt      = -1;
    displayManager.unregisterDisplayListener(this);
    if (refocusing || retrying)
      endRefocus();
    endTopGainWait();
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

  /** Moves the Activity onto display @displayId. Native code hears back,
   * by @move, only when the move cannot start; one that starts but
   * never shows is its timeout's to find. */
  void moveTo(int displayId, int move)
  {
    Display display = displayManager.getDisplay(displayId);
    if (Build.VERSION.SDK_INT < 26 || display == null || !display.isValid())
    {
      moveFailed(displayId, move, null);
      return;
    }
    try
    {
      startOn(displayId);
    }
    catch (RuntimeException e)
    {
      moveFailed(displayId, move, e);
      return;
    }
    /* Only a move that started replaces an earlier one's hold */
    if (refocusing || retrying)
      endRefocus();
    endTopGainWait();
    movedId  = displayId;
    fellBack = false;
    moves++;
  }

  /* The running Activity, as a copy of its own intent so onNewIntent()
   * sees the same content; the flags of the launch it came from would
   * clear the task */
  @TargetApi(26)
  private void startOn(int displayId)
  {
    Intent move = activity.getIntent() != null
        ? new Intent(activity.getIntent()) : new Intent();
    ActivityOptions options = ActivityOptions.makeBasic();
    move.setClass(activity, activity.getClass());
    move.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    options.setLaunchDisplayId(displayId);
    activity.startActivity(move, options.toBundle());
  }

  /** The Activity's window gained or lost the focus. */
  void onFocusChanged(boolean focused)
  {
    final int move     = moves;
    /* Android answers the flag with a loss, the focus it took, which
     * clearing the flag gives back; a gain queued before it is no
     * answer. Once the first hold has run out, either one ends it */
    final boolean held = (refocusing || retrying) && !focused;
    arms++;
    armPending = false;
    handler.removeCallbacks(topGainWait);
    if (held || retrying)
      endRefocus();
    lost = !focused && !held;
    /* Its top regain would be a return, not a move */
    if (lost)
      topLostFocused = false;
    if (!focused)
    {
      if (!held)
        refocusAfterMove();
      return;
    }
    if (movedId < 0)
      return;
    /* The old display's loss can still follow: the move ends only
     * once the gain has outlasted a traversal */
    afterTraversals(new Runnable() {
      @Override public void run() {
        Display own = activity.getWindowManager().getDefaultDisplay();
        /* On a removed display its loss is still to come */
        if (started && move == moves && own != null && own.isValid()
            && own.getDisplayId() == movedId
            && activity.hasWindowFocus())
        {
          movedId  = -1;
          fellBack = false;
        }
      }
    });
  }

  private void afterTraversals(final Runnable r)
  {
    Choreographer.getInstance().postFrameCallback(
        new Choreographer.FrameCallback() {
          private boolean traversed;
          @Override public void doFrame(long frameTimeNanos) {
            if (!traversed)
            {
              traversed = true;
              Choreographer.getInstance().postFrameCallback(this);
              return;
            }
            /* Behind a focus message already read */
            handler.post(r);
          }
        });
  }

  /* A focused window moved to another display, by RetroArch or by its
   * display's removal, can hear the old display's focus loss after the
   * new display's gain: unfocused while it is the focused window,
   * RetroArch would pause for good.
   * Unfocusable until Android has taken the focus, the window is then
   * sent the gain again. The first hold is short, as Android may be
   * waiting to focus the window instead; a loaded system can miss a
   * short toggle, so a longer one follows while still unfocused. */
  private void refocusAfterMove()
  {
    final int arm;
    if (armPending || !mayRefocus())
      return;
    armPending = true;
    arm        = arms;
    /* Waits as the gain's check does: a gain on its way needs no hold */
    afterTraversals(new Runnable() {
      @Override public void run() {
        if (arm != arms)
          return;
        armPending = false;
        if (lost && mayRefocus())
          refocus(REFOCUS_FIRST_MS, refocusFirstTimeout);
      }
    });
  }

  private boolean mayRefocus()
  {
    int ownId = ownDisplayId();
    return movedId >= 0 && !refocusing && !retrying && started
        && activity.getWindow() != null
        && (ownId == movedId
            || (ownId == Display.DEFAULT_DISPLAY && fellBack))
        && !activity.hasWindowFocus();
  }

  private void refocus(int ms, Runnable timeout)
  {
    refocusing = true;
    activity.getWindow().addFlags(
        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
    handler.postDelayed(timeout, ms);
  }

  private void endRefocus()
  {
    Window window = activity.getWindow();
    if (refocusing && window != null)
      window.clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
    refocusing = false;
    retrying   = false;
    handler.removeCallbacks(refocusFirstTimeout);
    handler.removeCallbacks(refocusRetry);
    handler.removeCallbacks(refocusTimeout);
  }

  /** The Activity became, or stopped being, the top resumed one; from
   * API 29. */
  void onTopResumedChanged(boolean top)
  {
    int own         = ownDisplayId();
    boolean focused = activity.hasWindowFocus();
    long now        = SystemClock.uptimeMillis();
    /* Android moved the focused task, maybe there and back with no
     * configuration change: the old display's loss can come last, even
     * before the regain when both moves give one pair of top-resumed
     * calls */
    if (top && started && !refocusing && own >= 0 && own != movedId
        && published.ids.length > 1
        && (focused ? topLostFocused
            : lost && topLostAt >= 0 && now - topLostAt <= TOP_PAIR_MS))
    {
      movedId = own;
      moves++;
      if (focused)
        endMarkIfQuiet();
      else
      {
        armPending = true;
        handler.postDelayed(topGainWait, TOP_GAIN_WAIT_MS);
      }
    }
    topLostFocused = !top && focused;
    if (!top)
      topLostAt = now;
  }

  private void endTopGainWait()
  {
    handler.removeCallbacks(topGainWait);
    armPending = false;
  }

  /* A mark set while focused gets no gain to end it, and the move's
   * focus events can still follow: it ends once none has come */
  private void endMarkIfQuiet()
  {
    final int move = moves;
    final int arm  = arms;
    handler.postDelayed(new Runnable() {
      private boolean waited;
      @Override public void run() {
        if (move != moves || arm != arms || !activity.hasWindowFocus())
          return;
        /* After a stall, a focus event can be queued behind this */
        if (!waited)
        {
          waited = true;
          afterTraversals(this);
          return;
        }
        movedId  = -1;
        fellBack = false;
      }
    }, QUIET_MARK_MS);
  }

  /* The Activity's display, -1 for none; a removed display's Activity
   * is on the default display */
  private int ownDisplayId()
  {
    Display own = activity.getWindowManager().getDefaultDisplay();
    if (own == null)
      return -1;
    return own.isValid() ? own.getDisplayId() : Display.DEFAULT_DISPLAY;
  }

  private void moveFailed(int displayId, int move, RuntimeException e)
  {
    Log.w(TAG, "[SecondDisplay] Cannot move to display " + displayId, e);
    nativeMoveFailed(move);
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
    boolean ownListed  = false;

    for (int i = 0; i < displays.length; i++)
    {
      Display d   = displays[i];
      int f       = 0;
      list.ids[i] = d.getDisplayId();
      if (list.ids[i] == ownId)
      {
        f |= FLAG_OWN;
        ownListed = true;
      }
      if (list.ids[i] == Display.DEFAULT_DISPLAY)
        f |= FLAG_DEFAULT;
      if (d.isValid())
        f |= FLAG_VALID;
      if (Build.VERSION.SDK_INT < 20 || d.getState() == Display.STATE_ON)
        f |= FLAG_ON;
      if ((d.getFlags() & Display.FLAG_PRIVATE) != 0)
        f |= FLAG_PRIVATE;
      list.models[i] = "";
      if (Build.VERSION.SDK_INT >= 31)
      {
        int connection = ProductInfo.connection(d);
        if (connection >= 0)
        {
          f |= FLAG_PHYSICAL | typeFlags(d);
          list.connections[i] = connection;
        }
        list.models[i] = ProductInfo.model(d);
      }
      list.flags[i] = f;
      /* From API 31 getRealSize() can report the Activity's window,
       * which follows RetroArch's own moves */
      if (Build.VERSION.SDK_INT >= 31)
        bare(displays, d).getRealSize(size);
      else
        d.getRealSize(size);
      list.widths[i]  = size.x;
      list.heights[i] = size.y;
      list.names[i]   = d.getName();
    }
    /* A removed display's Activity moves to the default display */
    if (!ownListed)
      for (int i = 0; i < displays.length; i++)
        if (list.ids[i] == Display.DEFAULT_DISPLAY)
          list.flags[i] |= FLAG_OWN;
    return list;
  }

  /* d from a context on another display: a Display made for its
   * context's own display reports that context's configuration, which
   * follows RetroArch's moves. d itself when it is the only display. */
  private Display bare(Display[] displays, Display d)
  {
    int id = d.getDisplayId();
    for (int s = 0; s < bareManagers.length; s++)
    {
      Display b;
      for (int i = 0; bareManagers[s] == null && i < displays.length; i++)
        if (displays[i].getDisplayId() != id)
        {
          bareIds[s]      = displays[i].getDisplayId();
          bareManagers[s] = (DisplayManager)activity.getApplicationContext()
              .createDisplayContext(displays[i])
              .getSystemService(Context.DISPLAY_SERVICE);
        }
      if (bareManagers[s] == null)
        break;
      if (bareIds[s] != id && (b = bareManagers[s].getDisplay(id)) != null)
        return b;
    }
    return d;
  }

  /* Android moved RetroArch off a removed display, or where it did not
   * ask; a move under way keeps its target */
  private void publishMarked(DisplayList list)
  {
    int own = published.ownId();
    int to  = -1;
    if (!list.has(own))
      fellBack = true;
    if (movedId >= 0 ? !list.has(movedId) : !list.has(own))
      to = Display.DEFAULT_DISPLAY;
    else if (list.has(own) && list.ownId() != own && list.ownId() != movedId)
      to = list.ownId();
    if (to >= 0)
    {
      if (refocusing || retrying)
        endRefocus();
      endTopGainWait();
      movedId = to;
      moves++;
      if (activity.hasWindowFocus())
        endMarkIfQuiet();
    }
    publish(list);
    if (lost)
      refocusAfterMove();
  }

  private void publish(DisplayList list)
  {
    int[] flags = list.flags.clone();
    for (int i = 0; i < flags.length; i++)
      if (failedIds.get(list.ids[i]))
        flags[i] |= FLAG_REFUSED;
    published = list;
    nativeDisplaysChanged(list.ids, list.connections, flags, list.widths,
        list.heights, list.names, list.models);
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
    /* From Android 12 a Presentation has a window context of its own */
    if (Build.VERSION.SDK_INT < 31)
      updateMetrics(p.getResources(), display);
    window = p.getWindow();
    /* Never focused: a tap would leave its display the pad, and the app
     * under it would get it once the Presentation goes */
    if (window != null)
      window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
          | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
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

  private void showFailed(int displayId, RuntimeException e)
  {
    Log.w(TAG, "[SecondDisplay] Cannot show on display " + displayId, e);
    failedIds.put(displayId, true);
    publishMarked(listDisplays());
  }

  private void dismiss()
  {
    Presentation p     = presentation;
    presentation       = null;
    presentationId     = -1;
    presentationScreen = null;
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
    final String[] models;

    DisplayList(int count)
    {
      ids         = new int[count];
      connections = new int[count];
      flags       = new int[count];
      widths      = new int[count];
      heights     = new int[count];
      names       = new String[count];
      models      = new String[count];
    }

    /* The display flagged FLAG_OWN, the default display if none */
    int ownId()
    {
      for (int i = 0; i < ids.length; i++)
        if ((flags[i] & FLAG_OWN) != 0)
          return ids[i];
      return Display.DEFAULT_DISPLAY;
    }

    boolean has(int id)
    {
      for (int i = 0; i < ids.length; i++)
        if (ids[i] == id)
          return true;
      return false;
    }

    boolean sameAs(DisplayList o)
    {
      return sameAsBut(o, 0);
    }

    boolean sameAsBut(DisplayList o, int ignoredFlags)
    {
      if (!Arrays.equals(ids, o.ids))
        return false;
      for (int i = 0; i < flags.length; i++)
        if (((flags[i] ^ o.flags[i]) & ~ignoredFlags) != 0)
          return false;
      return Arrays.equals(connections, o.connections)
          && Arrays.equals(widths, o.widths)
          && Arrays.equals(heights, o.heights)
          && Arrays.equals(names, o.names)
          && Arrays.equals(models, o.models);
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

  /* FLAG_EXTERNAL or FLAG_INTERNAL as the hidden Display.getType() says,
   * 0 where it cannot be read */
  private static int typeFlags(Display display)
  {
    int type;
    try
    {
      if (!getTypeLooked)
      {
        getTypeLooked = true;
        getType = Display.class.getMethod("getType");
      }
      if (getType == null)
        return 0;
      type = ((Integer)getType.invoke(display)).intValue();
    }
    catch (Throwable t)
    {
      if (!getTypeLogged)
        Log.i(TAG, "[SecondDisplay] Display.getType() not readable", t);
      getTypeLogged = true;
      return 0;
    }
    return type == DISPLAY_TYPE_EXTERNAL ? FLAG_EXTERNAL
        : type == DISPLAY_TYPE_INTERNAL ? FLAG_INTERNAL : 0;
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

    /* The product's name, "" without product info */
    static String model(Display display)
    {
      DeviceProductInfo info = display.getDeviceProductInfo();
      String name            = info != null ? info.getName() : null;
      return name != null ? name : "";
    }
  }

  static native void nativeDisplaysChanged(int[] ids, int[] connections,
      int[] flags, int[] widths, int[] heights, String[] names,
      String[] models);
  static native int nativeSurfaceChanged(Surface surface, int width,
      int height, int request);
  static native void nativeSurfaceDestroyed();
  static native void nativeTouch(int pointerId, int x, int y,
      boolean down, int generation);
  static native void nativeMoveFailed(int move);
}
