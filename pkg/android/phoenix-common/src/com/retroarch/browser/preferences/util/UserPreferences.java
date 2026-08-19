package com.retroarch.browser.preferences.util;

import java.io.File;
import java.io.IOException;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.preference.PreferenceManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.util.Log;

/**
 * Utility class for retrieving, saving, or loading preferences.
 */
public final class UserPreferences
{
	// Logging tag.
	private static final String TAG = "UserPreferences";

	// Disallow explicit instantiation.
	private UserPreferences()
	{
	}

	/**
	 * Retrieves the path to the default location of the libretro config.
	 * 
	 * @param ctx the current {@link Context}
	 * 
	 * @return the path to the default location of the libretro config.
	 */
	public static String getDefaultConfigPath(Context ctx)
	{
		// Internal/External storage dirs.
		final String internal = ctx.getFilesDir().getAbsolutePath();
		String external = null;

		// Get the App's external storage folder
		final String state = android.os.Environment.getExternalStorageState();
		if (android.os.Environment.MEDIA_MOUNTED.equals(state)) {
			File extsd = ctx.getExternalFilesDir(null);
			external = extsd.getAbsolutePath();
		}

		// Native library directory and data directory for this front-end.
		final String dataDir = ctx.getApplicationInfo().dataDir;
		final String coreDir = dataDir + "/cores/";

		// Get libretro name and path
		final SharedPreferences prefs = getPreferences(ctx);
		final String libretro_path = prefs.getString("libretro_path", coreDir);

		// Check if global config is being used. Return true upon failure.
		final boolean globalConfigEnabled = prefs.getBoolean("global_config_enable", true);

		String append_path;
		// If we aren't using the global config.
		if (!globalConfigEnabled && !libretro_path.equals(coreDir))
		{
			String sanitized_name = sanitizeLibretroPath(libretro_path);
			append_path = File.separator + sanitized_name + ".cfg";
		}
		else // Using global config.
		{
			append_path = File.separator + "retroarch.cfg";
		}

		if (external != null)
		{
			String confPath = external + append_path;
			if (new File(confPath).exists())
				return confPath;
		}
		else if (internal != null)
		{
			String confPath = internal + append_path;
			if (new File(confPath).exists())
				return confPath;
		}
		else
		{
			String confPath = "/mnt/extsd" + append_path;
			if (new File(confPath).exists())
				return confPath;
		}

		// Config file does not exist. Create empty one.

		// emergency fallback
		String new_path = "/mnt/sd" + append_path;

		if (external != null)
			new_path = external + append_path;
		else if (internal != null)
			new_path = internal + append_path;
		else if (dataDir != null)
			new_path = dataDir + append_path;

		try {
			new File(new_path).createNewFile();
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to create config file to: " + new_path);
		}
		return new_path;
	}

	// Cached parse of the config file, refreshed when the file on
	// disk changes.  Callbacks that fire often - window focus changes,
	// display mode updates - then pay a stat() instead of a full
	// re-read and re-parse on the UI thread.
	private static ConfigFile cachedConfig;
	private static String cachedConfigPath;
	private static long cachedConfigMtime;
	private static long cachedConfigSize;

	/**
	 * Returns a parse of the current config file, re-reading it only
	 * when the file on disk has changed since the last call.
	 *
	 * @param ctx the current {@link Context}.
	 *
	 * @return the parsed {@link ConfigFile}.
	 */
	public static synchronized ConfigFile getConfigFile(Context ctx)
	{
		String path = getDefaultConfigPath(ctx);
		File file = new File(path);
		long mtime = file.lastModified();
		long size = file.length();

		if (cachedConfig == null || !path.equals(cachedConfigPath)
				|| mtime != cachedConfigMtime || size != cachedConfigSize)
		{
			cachedConfig = new ConfigFile(path);
			cachedConfigPath = path;
			cachedConfigMtime = mtime;
			cachedConfigSize = size;
		}
		return cachedConfig;
	}

	/**
	 * Adds device-derived parameters to the intent that launches the
	 * native activity: the app version code that gates bundled asset
	 * extraction, and the device-optimal audio output parameters.
	 * The native side reads these as the VERSIONCODE, AUDIO_RATE and
	 * AUDIO_FRAMES extras.
	 *
	 * @param ctx   the current {@link Context}.
	 * @param retro the launch {@link Intent} for the native activity.
	 */
	public static void putDeviceIntentExtras(Context ctx, Intent retro)
	{
		final SharedPreferences prefs = getPreferences(ctx);

		try
		{
			int version = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionCode;
			retro.putExtra("VERSIONCODE", Integer.toString(version));
		}
		catch (NameNotFoundException ignored)
		{
		}

		int samplingRate = getOptimalSamplingRate(ctx);
		if (samplingRate > 0)
			retro.putExtra("AUDIO_RATE", Integer.toString(samplingRate));

		if (Build.VERSION.SDK_INT >= 17 && prefs.getBoolean("audio_latency_auto", true))
		{
			int bufferSize = getLowLatencyBufferSize(ctx);
			if (bufferSize > 0)
				retro.putExtra("AUDIO_FRAMES", Integer.toString(bufferSize));
		}
	}

	/**
	 * Sanitizes a libretro core path.
	 * 
	 * @param path The path to the libretro core.
	 * 
	 * @return the sanitized libretro path.
	 */
	private static String sanitizeLibretroPath(String path)
	{
		String sanitized_name = path.substring(
				path.lastIndexOf('/') + 1,
				path.lastIndexOf('.'));
		sanitized_name = sanitized_name.replace("neon", "");
		sanitized_name = sanitized_name.replace("libretro_", "");

		return sanitized_name;
	}

	/**
	 * Gets a {@link SharedPreferences} instance containing current settings.
	 * 
	 * @param ctx the current {@link Context}.
	 * 
	 * @return A SharedPreference instance containing current settings.
	 */
	public static SharedPreferences getPreferences(Context ctx)
	{
		return PreferenceManager.getDefaultSharedPreferences(ctx);
	}

	/**
	 * Gets the optimal sampling rate for low-latency audio playback.
	 * 
	 * @param ctx the current {@link Context}.
	 * 
	 * @return the optimal sampling rate for low-latency audio playback in Hz.
	 */
	@TargetApi(17)
	private static int getLowLatencyOptimalSamplingRate(Context ctx)
	{
		AudioManager manager = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
		String value = manager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);

		if(value == null || value.isEmpty()) {
			return -1;
		}

		return Integer.parseInt(value);
	}

	/**
	 * Gets the optimal buffer size for low-latency audio playback.
	 * 
	 * @param ctx the current {@link Context}.
	 * 
	 * @return the optimal output buffer size in decimal PCM frames.
	 */
	@TargetApi(17)
	private static int getLowLatencyBufferSize(Context ctx)
	{
		AudioManager manager = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
		String value = manager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);

		if(value == null || value.isEmpty()) {
			return -1;
		}

		int buffersize = Integer.parseInt(value);
		Log.i(TAG, "Queried ideal buffer size (frames): " + buffersize);
		return buffersize;
	}

	/**
	 * Gets the optimal audio sampling rate.
	 * <p>
	 * On Android 4.2+ devices this will retrieve the optimal low-latency sampling rate,
	 * since Android 4.2 adds support for low latency audio in general.
	 * <p>
	 * On other devices, it simply returns the regular optimal sampling rate
	 * as returned by the hardware.
	 * 
	 * @param ctx The current {@link Context}.
	 * 
	 * @return the optimal audio sampling rate in Hz.
	 */
	private static int getOptimalSamplingRate(Context ctx)
	{
		int ret;
		if (Build.VERSION.SDK_INT >= 17)
			ret = getLowLatencyOptimalSamplingRate(ctx);
		else
			ret = AudioTrack.getNativeOutputSampleRate(AudioManager.STREAM_MUSIC);

		Log.i(TAG, "Using sampling rate: " + ret + " Hz");
		return ret;
	}
}
