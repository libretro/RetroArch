package com.retroarch.browser.receiver;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Locale;

public class RetroAchievementsHostOverrideReceiver extends BroadcastReceiver
{
   private static final String TAG = "RetroArch";
   private static final String ACTION_SET_SUFFIX = ".action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE";
   private static final String ACTION_CLEAR_SUFFIX = ".action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE";
   private static final String EXTRA_HOST = "host";
   private static final String EXTRA_DISABLE_HARDCORE = "disableHardcore";

   private static final String PREF_ACTIVE = "cheevos_host_override_active";
   private static final String PREF_HOST = "cheevos_host_override_host";
   private static final String PREF_DISABLE_HARDCORE = "cheevos_host_override_disable_hardcore";
   private static final String PREF_USER_HOST = "cheevos_host_override_user_host";
   private static final String PREF_USER_HARDCORE = "cheevos_host_override_user_hardcore";

   /* Matches the config default, so a restore that predates the first
    * recorded settings does not silently turn hardcore off. */
   private static final boolean DEFAULT_HARDCORE = true;

   @Override
   public void onReceive(Context context, Intent intent)
   {
      String packageName = context.getPackageName();
      String action = intent.getAction();
      String setAction = packageName + ACTION_SET_SUFFIX;
      String clearAction = packageName + ACTION_CLEAR_SUFFIX;

      if (setAction.equals(action))
      {
         String normalizedHost = normalizeHost(intent.getStringExtra(EXTRA_HOST));
         if (normalizedHost == null)
         {
            Log.w(TAG, "Rejected invalid RetroAchievements host override");
            setResultCode(Activity.RESULT_CANCELED);
            return;
         }

         boolean shouldDisableHardcore = intent.getBooleanExtra(EXTRA_DISABLE_HARDCORE, true);
         SharedPreferences prefs = UserPreferences.getPreferences(context);
         boolean hardcoreEnabled = !shouldDisableHardcore
               && prefs.getBoolean(PREF_USER_HARDCORE, DEFAULT_HARDCORE);

         prefs.edit()
               .putBoolean(PREF_ACTIVE, true)
               .putString(PREF_HOST, normalizedHost)
               .putBoolean(PREF_DISABLE_HARDCORE, shouldDisableHardcore)
               .apply();

         applyOverrideIfRunning(normalizedHost, hardcoreEnabled);
         setResultCode(Activity.RESULT_OK);
         return;
      }

      if (clearAction.equals(action))
      {
         SharedPreferences prefs = UserPreferences.getPreferences(context);

         prefs.edit().putBoolean(PREF_ACTIVE, false).apply();

         applyOverrideIfRunning(prefs.getString(PREF_USER_HOST, ""),
               prefs.getBoolean(PREF_USER_HARDCORE, DEFAULT_HARDCORE));
         setResultCode(Activity.RESULT_OK);
      }
   }

   /**
    * Records the achievement settings the native side just loaded, so an
    * override sent while RetroArch is not running knows what to restore.
    * Ignored while an override is active, where the live values are the
    * override's own.
    */
   public static void recordSettings(Context context, String host, boolean hardcoreEnabled)
   {
      SharedPreferences prefs = UserPreferences.getPreferences(context);

      if (prefs.getBoolean(PREF_ACTIVE, false))
         return;

      prefs.edit()
            .putString(PREF_USER_HOST, host != null ? host : "")
            .putBoolean(PREF_USER_HARDCORE, hardcoreEnabled)
            .apply();
   }

   /**
    * Returns the achievement settings the native side should apply on top of
    * the loaded config, as "host\t0|1", or null when the config is to be left
    * alone. The restore values handed out after a clear are consumed, so a
    * later config change is not overridden by them.
    */
   public static String consumeOverride(Context context)
   {
      SharedPreferences prefs = UserPreferences.getPreferences(context);

      if (prefs.getBoolean(PREF_ACTIVE, false))
      {
         boolean hardcoreEnabled = !prefs.getBoolean(PREF_DISABLE_HARDCORE, true)
               && prefs.getBoolean(PREF_USER_HARDCORE, DEFAULT_HARDCORE);

         return prefs.getString(PREF_HOST, "") + '\t' + (hardcoreEnabled ? '1' : '0');
      }

      /* PREF_HOST outliving a cleared override means the config still
       * carries it, so the restore runs even where the settings were
       * never recorded. */
      if (!prefs.contains(PREF_USER_HOST) && !prefs.contains(PREF_HOST))
         return null;

      String host = prefs.getString(PREF_USER_HOST, "");
      boolean hardcoreEnabled = prefs.getBoolean(PREF_USER_HARDCORE, DEFAULT_HARDCORE);

      prefs.edit()
            .remove(PREF_USER_HOST)
            .remove(PREF_USER_HARDCORE)
            .remove(PREF_HOST)
            .remove(PREF_DISABLE_HARDCORE)
            .apply();

      return host + '\t' + (hardcoreEnabled ? '1' : '0');
   }

   private static String normalizeHost(String host)
   {
      String trimmed = host != null ? host.trim() : "";
      if (trimmed.isEmpty())
         return null;

      String candidate = trimmed.contains("://") ? trimmed : "http://" + trimmed;

      try
      {
         URI uri = new URI(candidate);
         String scheme = uri.getScheme();
         String normalizedScheme = scheme != null ? scheme.toLowerCase(Locale.US) : null;
         String normalizedHost = uri.getHost();
         normalizedHost = normalizedHost != null ? normalizedHost.toLowerCase(Locale.US) : null;
         String path = uri.getRawPath();

         if (!"http".equals(normalizedScheme))
            return null;

         if (!"127.0.0.1".equals(normalizedHost) && !"localhost".equals(normalizedHost))
            return null;

         if (uri.getPort() < 1 || uri.getPort() > 65535)
            return null;

         if (uri.getRawQuery() != null || uri.getRawFragment() != null || uri.getUserInfo() != null)
            return null;

         if (path != null && path.length() > 0 && !"/".equals(path) && !"/dorequest.php".equals(path))
            return null;

         return normalizedHost + ":" + uri.getPort();
      }
      catch (URISyntaxException ignored)
      {
         return null;
      }
   }

   private static void applyOverrideIfRunning(String host, boolean hardcoreEnabled)
   {
      if (!RetroActivityFuture.isRunning)
         return;

      try
      {
         RetroActivityFuture.applyRetroAchievementsHostOverride(host, hardcoreEnabled);
      }
      catch (UnsatisfiedLinkError e)
      {
         Log.w(TAG, "Could not apply RetroAchievements host override live", e);
      }
   }
}
