package com.retroarch.browser.receiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class InstalledCoresReceiver extends BroadcastReceiver
{
   public static final String ACTION_QUERY = "com.retroarch.QUERY_INSTALLED_CORES";
   public static final String ACTION_RESULT = "com.retroarch.INSTALLED_CORES_RESULT";
   public static final String EXTRA_CORES = "CORES";

   @Override
   public void onReceive(Context context, Intent intent)
   {
      if (!ACTION_QUERY.equals(intent.getAction()))
         return;

      String coreDir = context.getApplicationInfo().dataDir + "/cores/";
      File dir = new File(coreDir);
      List<String> cores = new ArrayList<>();

      if (dir.exists() && dir.isDirectory())
      {
         File[] files = dir.listFiles();
         if (files != null)
         {
            for (File file : files)
            {
               String name = file.getName();
               if (name.endsWith("_libretro_android.so") || name.endsWith("_libretro.so"))
                  cores.add(name);
            }
         }
      }

      String[] result = cores.toArray(new String[0]);
      Log.i("RetroArch", "InstalledCoresReceiver: found " + result.length + " cores");

      Intent resultIntent = new Intent(ACTION_RESULT);
      resultIntent.putExtra(EXTRA_CORES, result);
      context.sendBroadcast(resultIntent);
   }
}
