package com.retroarch.browser.cores;

import com.retroarch.BuildConfig;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 *
 * Imports all cores from a custom directory to RetroArch's private core folder
 *
 */
public final class CoreBulkImportActivity extends Activity{
   public static final String EXTRA_CORES_DIR = "CORES_DIR";
   private static final int REQUEST_CODE_STARTUP_ALL_FILES   = 140;
   private static final int REQUEST_CODE_STARTUP_PERMISSIONS = 141;

   /* lock constant to prevent */
   private static final Object sImportLock = new Object();

   private String   mCoresDir;

   @Override
   protected void onCreate(Bundle savedInstanceState)
   {
      super.onCreate(savedInstanceState);

      mCoresDir = getIntent().getStringExtra(EXTRA_CORES_DIR);

      if (mCoresDir == null)
      {
         Log.e("CoreBulkImportActivity", "Missing \"" + EXTRA_CORES_DIR + "\"");
         finish();
         return;
      }

      resolveStartupPermissions();
   }

   /**
   *
   * Start of copied functions from pkg/android/phoenix-common/src/com/retroarch/browser/retroactivity/RetroActivityCommon.java
   *
   **/

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

   /**
   * Checks if this version of RetroArch is a Play Store build.
   *
   * @return true if this is a Play Store build, false otherwise
   */
   public boolean isPlayStoreBuild() {
      Log.i("RetroActivity", "isPlayStoreBuild: " + BuildConfig.PLAY_STORE_BUILD);

      return BuildConfig.PLAY_STORE_BUILD;
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


   /**
   *
   * Finish of copied functions from pkg/android/phoenix-common/src/com/retroarch/browser/retroactivity/RetroActivityCommon.java
   *
   **/

  @Override
   public void onActivityResult(int requestCode, int resultCode, Intent intent){
      if (requestCode == REQUEST_CODE_STARTUP_ALL_FILES)
      {
         /* The Settings return carries no intent; resolve from the live
         * state and proceed either way. */
         startupPermissionResolved(hasStoragePermission());
         return;
      }
   }


   /**
    * Start if we have permissions granted
    */
   private void startupPermissionResolved(boolean granted){
      if (granted)
         startImport();
      else
      {
         Log.e("CoreBulkImportActivity", "Storage permission denied");
         finish();
      }
   }

   /** Import the cores and show a pop up to the user when it finishes */
   private void startImport(){

      Log.i("CoreBulkImportActivity", "Importing cores from " + mCoresDir);

      new Thread(new Runnable() {
         @Override
         public void run() {
            final int[] result;
            /*
               result[0] -> imported
               result[1] -> failed
            */

            synchronized (sImportLock)
            {
               result = importCores(new File(mCoresDir));
            }

            runOnUiThread(new Runnable() {
               @Override
               public void run() {
                  if (result == null)
                  {
                     setResult(RESULT_CANCELED);
                     showFinishPopUp("Error while reading " + mCoresDir);
                     return;
                  }

                  String message = "Imported " + result[0] + " cores";

                  if (result[1] > 0)
                     message += ", " + result[1] + " failed";

                  setResult(result[1] == 0 ? RESULT_OK : RESULT_CANCELED);
                  showFinishPopUp(message);
               }
            });
         }
      }, "RetroArch-core-bulk-import").start();
   }

   /* Regular copy loop while checking the file extension */
   private int[] importCores(File dir){

      File[] cores = dir.listFiles();
      File destDir = new File(getApplicationInfo().dataDir, "cores");
      int imported = 0;
      int failed   = 0;

      if (cores == null)
         return null;

      if (!destDir.exists())
         destDir.mkdirs();


      for (int i = 0; i < cores.length; i++)
      {
         File src  = cores[i];
         File dest = new File(destDir, src.getName());

         if (!src.isFile() || !src.getName().endsWith(".so"))
            continue;

         if (copy(src, dest))
            imported++;
         else
            failed++;
      }

      return new int[] { imported, failed };
   }

   /** Copy one core into the cores directory. */
   private static boolean copy(File src, File dest) {
      try (FileInputStream in   = new FileInputStream(src);
           FileOutputStream out = new FileOutputStream(dest))
      {
         in.getChannel().transferTo(0, in.getChannel().size(), out.getChannel());
      }
      catch (IOException e)
      {
         Log.e("CoreBulkImportActivity", "Failed to import core " + src.getName() + ": " + e.getMessage());
         return false;
      }

      Log.i("CoreBulkImportActivity", "Imported core " + dest.getAbsolutePath());
      return true;
   }


   /** Tells the user how the import went */
   private void showFinishPopUp(String message){

      Log.i("CoreBulkImportActivity", message);

      new android.app.AlertDialog.Builder(this)
        .setMessage(message)
        .setPositiveButton("Go Back", new android.content.DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(android.content.DialogInterface dialog, int which)
          {
            finish();
          }
        })
        .setNegativeButton("Open RetroArch", new android.content.DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(android.content.DialogInterface dialog, int which)
          {
            Intent launchIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());

            if (launchIntent != null)
            {
              launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
              startActivity(launchIntent);
            }
            finish();
          }
        })
        .setOnCancelListener(new android.content.DialogInterface.OnCancelListener()
        {
          @Override
          public void onCancel(android.content.DialogInterface dialog)
          {
            finish();
          }
        })
        .show();
   }

}
