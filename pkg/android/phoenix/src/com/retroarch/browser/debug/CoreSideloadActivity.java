package com.retroarch.browser.debug;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.util.Log;
import android.widget.TextView;

import com.retroarch.browser.mainmenu.MainMenuActivity;
import com.retroarch.browser.preferences.util.UserPreferences;
import com.retroarch.browser.retroactivity.RetroActivityFuture;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * This activity allows developers to sideload and run a core
 * from their PC through adb
 *
 * Usage : see Phoenix Gradle Build README.md
 */
public class CoreSideloadActivity extends Activity
{
    private static final String EXTRA_CORE = "LIBRETRO";
    private static final String EXTRA_CONTENT = "ROM";

    private TextView textView;
    private CoreSideloadWorkerTask workerThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // The most simple layout is no layout at all
        textView = new TextView(this);
        setContentView(textView);

        // Check that we have at least the core extra
        if (!getIntent().hasExtra(EXTRA_CORE))
        {
            textView.setText("Missing extra \"LIBRETRO\"");
            return;
        }

        // Start our worker thread
        workerThread = new CoreSideloadWorkerTask(this, textView, getIntent().getStringExtra(EXTRA_CORE), getIntent().getStringExtra(EXTRA_CONTENT));
        workerThread.execute();
    }

    @Override
    protected void onDestroy() {
        if (workerThread != null)
        {
            workerThread.cancel(true);
            workerThread = null;
        }
        super.onDestroy();
    }

    private static class CoreSideloadWorkerTask extends AsyncTask<Void, Integer, String>
    {
        private TextView progressTextView;
        private String core;
        private String content;
        private Activity ctx;
        private File destination;

        public CoreSideloadWorkerTask(Activity ctx, TextView progressTextView, String corePath, String contentPath)
        {
            this.progressTextView = progressTextView;
            this.core = corePath;
            this.ctx = ctx;
            this.content = contentPath;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
            progressTextView.setText("Sideloading...");
        }

        @Override
        protected String doInBackground(Void... voids) {
            File coreFile = new File(core);
            File corePath = new File(UserPreferences.getPreferences(ctx).getString("libretro_path", ctx.getApplicationInfo().dataDir + "/cores/"));

            // Check that both files exist
            if (!coreFile.exists())
                return "Input file doesn't exist (" + core + ")";

            if (!corePath.exists())
                return "Destination directory doesn't exist (" + corePath.getAbsolutePath() + ")";

            destination = new File(corePath, coreFile.getName());

            // Copy it
            Log.d("sideload", "Copying " + coreFile.getAbsolutePath() + " to " + destination.getAbsolutePath());
            long copied = 0;
            long max = coreFile.length();
            try
            {
                InputStream is = new FileInputStream(coreFile);
                OutputStream os = new FileOutputStream(destination);

                byte[] buf = new byte[1024];
                int length;

                while ((length = is.read(buf)) > 0)
                {
                    os.write(buf, 0, length);

                    copied += length;
                    publishProgress((int)(copied / max * 100));
                }

                is.close();
                os.close();
            }
            catch (IOException ex)
            {
                ex.printStackTrace();
                return ex.getMessage();
            }

            return null;
        }

        @Override
        protected void onProgressUpdate(Integer... values) {
            super.onProgressUpdate(values);

            if (values.length > 0)
                progressTextView.setText("Sideloading: " + values[0] + "%");
        }

        @Override
        protected void onPostExecute(String s) {
            super.onPostExecute(s);

            // Everything went as expected
            if (s == null)
            {
                progressTextView.setText("Done!");

                // Run RA with our newly sideloaded core (and content)
                Intent retro = new Intent(ctx, RetroActivityFuture.class);

                final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(ctx);

                retro.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);

                Log.d("sideload", "Running RetroArch with core " + destination.getAbsolutePath());

                MainMenuActivity.startRetroActivity(
                    retro,
                    content,
                    destination.getAbsolutePath(),
                    UserPreferences.getDefaultConfigPath(ctx),
                    Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD),
                    ctx.getApplicationInfo().dataDir,
                    ctx.getApplicationInfo().sourceDir);

                ctx.startActivity(retro);
                ctx.finish();
            }
            // An error occured
            else
            {
                progressTextView.setText("Error: " + s);
            }
        }
    }
}
