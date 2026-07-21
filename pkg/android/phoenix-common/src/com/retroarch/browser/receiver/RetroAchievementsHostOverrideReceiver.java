package com.retroarch.browser.receiver;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import com.retroarch.browser.preferences.util.ConfigFile;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;

import java.io.IOException;
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
   private static final String HOST_KEY = "cheevos_custom_host";
   private static final String HARDCORE_KEY = "cheevos_hardcore_mode_enable";
   private static final String HARDCORE_RESTORE_KEY = "raofflineproxy_hardcore_restore";

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
         if (!persistOverride(context, normalizedHost, shouldDisableHardcore))
         {
            setResultCode(Activity.RESULT_CANCELED);
            return;
         }

         applyOverrideIfRunning(normalizedHost, !shouldDisableHardcore);
         setResultCode(Activity.RESULT_OK);
         return;
      }

      if (clearAction.equals(action))
      {
         Boolean restoreHardcore = clearOverride(context);
         if (restoreHardcore == null)
         {
            setResultCode(Activity.RESULT_CANCELED);
            return;
         }

         applyOverrideIfRunning("", restoreHardcore.booleanValue());
         setResultCode(Activity.RESULT_OK);
      }
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

    private static boolean persistOverride(Context context, String host, boolean shouldDisableHardcore)
    {
       String path = UserPreferences.getDefaultConfigPath(context);
       ConfigFile config = new ConfigFile(path);

       try
       {
          if (shouldDisableHardcore)
          {
             String previousRestore = config.keyExists(HARDCORE_RESTORE_KEY)
                   ? config.getString(HARDCORE_RESTORE_KEY)
                  : "";
            if (previousRestore == null || previousRestore.length() == 0)
            {
               String currentValue = config.keyExists(HARDCORE_KEY)
                     ? Boolean.toString(config.getBoolean(HARDCORE_KEY))
                     : "false";
               config.setString(HARDCORE_RESTORE_KEY, currentValue);
            }
         }

          config.setString(HOST_KEY, host);
          if (shouldDisableHardcore)
             config.setBoolean(HARDCORE_KEY, false);
          config.write(path);
          return true;
      }
      catch (IOException e)
      {
         Log.e(TAG, "Failed to persist RetroAchievements host override", e);
         return false;
      }
   }

   private static Boolean clearOverride(Context context)
   {
      String path = UserPreferences.getDefaultConfigPath(context);
      ConfigFile config = new ConfigFile(path);

      try
      {
         String restoreValue = config.keyExists(HARDCORE_RESTORE_KEY)
               ? config.getString(HARDCORE_RESTORE_KEY)
               : "";
         boolean restoreHardcore = Boolean.parseBoolean(restoreValue);

         config.setString(HOST_KEY, "");
         config.setBoolean(HARDCORE_KEY, restoreHardcore);
         config.setString(HARDCORE_RESTORE_KEY, "");
         config.write(path);
         return Boolean.valueOf(restoreHardcore);
      }
      catch (IOException e)
      {
         Log.e(TAG, "Failed to clear RetroAchievements host override", e);
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
