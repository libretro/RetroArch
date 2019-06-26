package com.retroarch.browser.preferences.util;

import java.io.File;
import java.io.IOException;

import android.annotation.TargetApi;
import android.content.Context;
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

	/**
	 * Updates the libretro configuration file
	 * with new values if settings have changed.
	 * 
	 * @param ctx the current {@link Context}.
	 */
	public static void updateConfigFile(Context ctx)
	{
		String path = getDefaultConfigPath(ctx);
		ConfigFile config = new ConfigFile(path);

		Log.i(TAG, "Writing config to: " + path);

		final String dataDir = ctx.getApplicationInfo().dataDir;
		final String coreDir = dataDir + "/cores/";

		final SharedPreferences prefs = getPreferences(ctx);

		config.setString("libretro_directory", coreDir);
		config.setInt("audio_out_rate", getOptimalSamplingRate(ctx));

		try
		{
			int version							= ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionCode;
			final String dst_path			= dataDir;
			final String dst_path_subdir	= "assets";

			Log.i(TAG, "dst dir is: " + dst_path);
			Log.i(TAG, "dst subdir is: " + dst_path_subdir);

			config.setString("bundle_assets_src_path", ctx.getApplicationInfo().sourceDir);
			config.setString("bundle_assets_dst_path", dst_path);
			config.setString("bundle_assets_dst_path_subdir", dst_path_subdir);
			config.setInt("bundle_assets_extract_version_current", version);
		}
		catch (NameNotFoundException ignored)
		{
		}

		// Refactor this entire mess and make this usable for per-core config
		if (Build.VERSION.SDK_INT >= 17 && prefs.getBoolean("audio_latency_auto", true))
		{
			config.setInt("audio_block_frames", getLowLatencyBufferSize(ctx));
		}

		try
		{
			config.write(path);
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to save config file to: " + path);
		}
	}

	private static void readbackString(ConfigFile cfg, SharedPreferences.Editor edit, String key)
	{
		if (cfg.keyExists(key))
			edit.putString(key, cfg.getString(key));
		else
			edit.remove(key);
	}

	private static void readbackBool(ConfigFile cfg, SharedPreferences.Editor edit, String key)
	{
		if (cfg.keyExists(key))
			edit.putBoolean(key, cfg.getBoolean(key));
		else
			edit.remove(key);
	}

	private static void readbackDouble(ConfigFile cfg, SharedPreferences.Editor edit, String key)
	{
		if (cfg.keyExists(key))
			edit.putFloat(key, (float)cfg.getDouble(key));
		else
			edit.remove(key);
	}

	/*
	private static void readbackFloat(ConfigFile cfg, SharedPreferences.Editor edit, String key)
	{
		if (cfg.keyExists(key))
			edit.putFloat(key, cfg.getFloat(key));
		else
			edit.remove(key);
	}
	*/

	/**
	private static void readbackInt(ConfigFile cfg, SharedPreferences.Editor edit, String key)
	{
		if (cfg.keyExists(key))
			edit.putInt(key, cfg.getInt(key));
		else
			edit.remove(key);
	}
	*/

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

		return Integer.parseInt(manager
				.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
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
		int buffersize = Integer.parseInt(manager
				.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
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
