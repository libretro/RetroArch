package com.retroarch.browser;

import com.retroarch.browser.preferences.util.ConfigFile;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.content.Context;
import android.graphics.drawable.Drawable;

/**
 * Wrapper class that encapsulates a libretro core 
 * along with information about said core.
 */
public final class ModuleWrapper implements IconAdapterItem, Comparable<ModuleWrapper>
{
	private final File file;
	private final String displayName;
	private final String coreName;
	private final String manufacturer;
	private final String systemName;
	private final String license;
	private final List<String> supportedExtensions;

	/**
	 * Constructor
	 * 
	 * @param context The current {@link Context}.
	 * @param file    The {@link File} instance of the core being wrapped.
	 */
	public ModuleWrapper(Context context, File file)
	{
		this.file = file;

		// Attempt to get the core's info file.
		// Basically this is dataDir/info/[core name].info

		// So first, since the core name will contain platform specific strings at the end of name, we trim this.
		final String coreName = file.getName().substring(0, file.getName().lastIndexOf("_android.so"));

		// Now get the directory where all of the info files are kept (dataDir/info)
		final String infoFileDir  = context.getApplicationInfo().dataDir + File.separator + "info";

		// Now, based off of the trimmed core name, we can get the core info file.
		// and attempt to read it as a config file (since it has the same key-value layout).
		final String infoFilePath = infoFileDir + File.separator + coreName + ".info";
		final ConfigFile infoFile = new ConfigFile(infoFilePath);

		// Now read info out of the info file. Make them an empty string if the key doesn't exist.
		this.displayName  = (infoFile.keyExists("display_name")) ? infoFile.getString("display_name") : "";
		this.coreName     = (infoFile.keyExists("corename"))     ? infoFile.getString("corename")     : "";
		this.systemName   = (infoFile.keyExists("systemname"))   ? infoFile.getString("systemname")   : "";
		this.manufacturer = (infoFile.keyExists("manufacturer")) ? infoFile.getString("manufacturer") : "";
		this.license      = (infoFile.keyExists("license"))      ? infoFile.getString("license")      : "";

		// Getting supported extensions is a little different.
		// We need to split at every '|' character, since it is
		// the delimiter for a new extension that the core supports.
		//
		// Cores that don't have multiple extensions supported
		// don't contain the '|' delimiter, so we just create a String array with
		// a size of 1, and just directly assign the retrieved extensions to it.
		final String supportedExts = infoFile.getString("supported_extensions");
		if (supportedExts != null && supportedExts.contains("|"))
		{
			this.supportedExtensions = new ArrayList<String>(Arrays.asList(supportedExts.split("|")));
		}
		else
		{
			this.supportedExtensions = new ArrayList<String>(); 
			this.supportedExtensions.add(supportedExts);
		}
	}

	/**
	 * Gets the underlying {@link File} instance for this ModuleWrapper.
	 * 
	 * @return the underlying {@link File} instance for this ModuleWrapper.
	 */
	public File getUnderlyingFile()
	{
		return file;
	}

	/**
	 * Gets the display name for this wrapped core.
	 * 
	 * @return the display name for this wrapped core.
	 */
	public String getDisplayName()
	{
		return displayName;
	}

	/**
	 * Gets the license that this core is protected under.
	 * 
	 * @return the license that this core is protected under.
	 */
	public String getCoreLicense()
	{
		return license;
	}

	/**
	 * Gets the name of the manufacturer of the console that
	 * this core emulates.
	 * 
	 * @return the name of the manufacturer of the console that
	 *         this core emulates.
	 */
	public String getManufacturer()
	{
		return manufacturer;
	}

	/**
	 * Gets the List of supported extensions for this core.
	 * 
	 * @return the List of supported extensions for this core.
	 */
	public List<String> getSupportedExtensions()
	{
		return supportedExtensions;
	}

	@Override
	public boolean isEnabled()
	{
		return true;
	}

	@Override
	public String getText()
	{
		return coreName;
	}
	
	@Override
	public String getSubText()
	{
		return systemName;
	}

	@Override
	public int getIconResourceId()
	{
		return 0;
	}

	@Override
	public Drawable getIconDrawable()
	{
		return null;
	}

	@Override
	public int compareTo(ModuleWrapper other)
	{
		if(coreName != null)
			return coreName.toLowerCase().compareTo(other.coreName.toLowerCase()); 
		else 
			throw new NullPointerException("The name of this ModuleWrapper is null");
	}
}