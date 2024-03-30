package com.retroarch.browser.mainmenu;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.preference.PreferenceManager;
import android.provider.DocumentsContract;
import android.util.Log;
import android.util.Pair;

import com.retroarch.R;

import java.io.File;
import java.lang.ref.WeakReference;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;

@TargetApi(26)
public class MigrateRetroarchFolderActivity extends Activity
{
    final int REQUEST_CODE_GET_OLD_RETROARCH_FOLDER = 125;

    @Override
    public void onStart()
    {
        super.onStart();

        // Needs v26 for some of the file handling functions below.
        // Remove the TargetApi annotation to see which.
        // If we don't have it, then just skip migration.
        if (android.os.Build.VERSION.SDK_INT < 26) {
            finish();
        }
        if(true || needToMigrate()){
            askToMigrate();
        }else{
            finish();
        }
    }

    boolean needToMigrate()
    {
        // As the RetroArch folder has been moved from shared storage to app-specific storage,
        // people upgrading from older versions using the old location will need to migrate their data.
        // We identify these users by checking that the app has been updated from an older version,
        // and that the older version did not use the new location.
        final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        boolean isNewInstall;
        try{
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            isNewInstall = info.firstInstallTime == info.lastUpdateTime;
        }catch(PackageManager.NameNotFoundException ex) {
            isNewInstall = true;
        }

        // Avoid asking if new install
        if(isNewInstall && !prefs.contains("external_retroarch_folder_needs_migrate")){
            SharedPreferences.Editor editor = prefs.edit();
            editor.putBoolean("external_retroarch_folder_needs_migrate", false);
            editor.apply();
        }

        return prefs.getBoolean("external_retroarch_folder_needs_migrate", true);
    }

    void askToMigrate()
    {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.migrate_retroarch_folder_dialog_title);
        builder.setMessage(R.string.migrate_retroarch_folder_dialog_message);
        builder.setNegativeButton(R.string.migrate_retroarch_folder_dialog_negative, new DialogInterface.OnClickListener() {
            @Override public void onClick(DialogInterface dialogInterface, int i) {
                SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(MigrateRetroarchFolderActivity.this).edit();
                editor.putBoolean("external_retroarch_folder_needs_migrate", false);
                editor.apply();
                MigrateRetroarchFolderActivity.this.finish();
            }
        });
        builder.setNeutralButton(R.string.migrate_retroarch_folder_dialog_neutral, new DialogInterface.OnClickListener() {
            @Override public void onClick(DialogInterface dialogInterface, int i) {
                SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(MigrateRetroarchFolderActivity.this).edit();
                editor.putBoolean("external_retroarch_folder_needs_migrate", true);
                editor.apply();
                MigrateRetroarchFolderActivity.this.finish();
            }
        });
        builder.setPositiveButton(R.string.migrate_retroarch_folder_dialog_positive, new DialogInterface.OnClickListener() {
            @Override public void onClick(DialogInterface dialogInterface, int i) {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, Uri.fromFile(new File(
                        Environment.getExternalStorageDirectory().getAbsolutePath() + "/RetroArch"
                )));
                startActivityForResult(intent, REQUEST_CODE_GET_OLD_RETROARCH_FOLDER);
            }
        });
        AlertDialog dialog = builder.create();
        dialog.show();
    }

    public void onActivityResult(int requestCode, int resultCode, Intent resultData)
    {
        super.onActivityResult(requestCode, resultCode, resultData);
        if(requestCode == REQUEST_CODE_GET_OLD_RETROARCH_FOLDER){
            if(resultCode == Activity.RESULT_OK && resultData != null){
                copyFiles(resultData.getData());
            }else{
                //User cancelled or otherwise failed. Go back to the picker screen.
                askToMigrate();
            }
        }
    }

    void copyFiles(Uri sourceDir)
    {
        final ProgressDialog pd = new ProgressDialog(this);
        pd.setMax(100);
        pd.setTitle(R.string.migrate_retroarch_folder_inprogress);
        pd.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        pd.setCancelable(false);

        CopyThread thread = new CopyThread()
        {
            @Override
            protected void onPreExecute(){
                super.onPreExecute();
                pd.show();
            }
            @Override
            protected void onProgressUpdate(Pair<Integer, String>... params)
            {
                super.onProgressUpdate(params);
                pd.setProgress(params[0].first);
                pd.setMessage(params[0].second);
            }
            @Override
            protected void onPostExecute(Boolean ok)
            {
                super.onPostExecute(ok);
                pd.dismiss();
                postMigrate(ok);
            }
        };

        thread.execute(sourceDir);
    }

    void postMigrate(boolean ok)
    {
        SharedPreferences.Editor editor = PreferenceManager.getDefaultSharedPreferences(this).edit();
        editor.putBoolean("external_retroarch_folder_needs_migrate", false);
        editor.commit();

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(ok ?
                R.string.migrate_retroarch_folder_confirm :
                R.string.migrate_retroarch_folder_confirm_witherror
        );
        builder.setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
            @Override public void onClick(DialogInterface dialogInterface, int i) {
                MigrateRetroarchFolderActivity.this.finish();
            }
        });
        builder.create().show();
    }

    class CopyThread extends AsyncTask<Uri, Pair<Integer, String>, Boolean>
    {
        String PACKAGE_NAME;
        ContentResolver resolver;
        Uri sourceRoot;
        boolean error;
        ArrayList<int[]> progress;
        public CopyThread()
        {
            PACKAGE_NAME = MigrateRetroarchFolderActivity.this.getPackageName();
            resolver = MigrateRetroarchFolderActivity.this.getContentResolver();
        }
        @Override
        protected Boolean doInBackground(Uri... params)
        {
            sourceRoot = params[0];
            error = false;
            progress = new ArrayList<>();

            String destination = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/data/" + PACKAGE_NAME + "/files/RetroArch";
            copyFolder(sourceRoot, new File(destination));
            return !error;
        }
        void copyFolder(Uri sourceUri, File dest)
        {
            //create destination folder
            if(!(dest.isDirectory() || dest.mkdirs())) {
                Log.e("MigrateRetroarchFolder", "Couldn't make new destination folder " + dest.getPath());
                error = true;
                return;
            }

            Uri sourceChildrenResolver;
            try{ //for subfolders
                sourceChildrenResolver = DocumentsContract.buildChildDocumentsUriUsingTree(sourceUri, DocumentsContract.getDocumentId(sourceUri));
            }catch(IllegalArgumentException ex){ //for root selected by document picker
                sourceChildrenResolver = DocumentsContract.buildChildDocumentsUriUsingTree(sourceUri, DocumentsContract.getTreeDocumentId(sourceUri));
            }
            progress.add(new int[]{0, 1});
            try(
                    Cursor c = resolver.query(sourceChildrenResolver, new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)
            ) {
                if(c == null) {
                    Log.e("MigrateRetroarchFolder", "Could not list files in source folder " + sourceUri.toString());
                    error = true;
                    return;
                }
                progress.get(progress.size() - 1)[1] = c.getCount();
                while(c.moveToNext()){ //loop through children returned
                    String childFilename = c.getString(1);
                    Uri childUri = DocumentsContract.buildDocumentUriUsingTree(sourceUri, c.getString(0));
                    String childDocumentId = DocumentsContract.getDocumentId(childUri);
                    File destFile = new File(dest, childFilename);

                    if(c.getString(2).equals(DocumentsContract.Document.MIME_TYPE_DIR)){ //is a folder, recurse
                        copyFolder(childUri, destFile);
                    }else{ //is a file, copy it
                        try(
                                ParcelFileDescriptor pfd = resolver.openFileDescriptor(childUri, "r");
                                ParcelFileDescriptor.AutoCloseInputStream sourceStream = new ParcelFileDescriptor.AutoCloseInputStream(pfd);
                        ) {
                            Files.copy(sourceStream, destFile.toPath(), StandardCopyOption.REPLACE_EXISTING);
                        }catch(Exception ex){
                            Log.e("MigrateRetroarchFolder", "Error copying file " + childDocumentId, ex);
                            error = true;
                        }
                    }
                    progress.get(progress.size() - 1)[0]++;
                    publishProgress(new Pair<Integer, String>(getProgressPercentage(), destFile.getPath()));
                }
            }catch(Exception ex){
                Log.e("MigrateRetroarchFolder", "Error while copying", ex);
                error = true;
            }
            progress.remove(progress.size() - 1);
        }
        int getProgressPercentage()
        {
            float sum = 0;
            int lastDenominator = 1;
            for(int[] frac : progress){
                sum += ((float) frac[0]) / frac[1] / lastDenominator;
                lastDenominator *= frac[1];
            }
            return (int) (sum * 100);
        }
    }
}
