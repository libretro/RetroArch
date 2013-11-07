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
	private final List<String> authors;
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

		// So first, since the core name will have a platform-specific identifier at the end of its name, we trim this.
		// If it turns out we have an invalid core name, simply assign the core name as the full name of the file.
		final boolean isValidCoreName = (file.getName().lastIndexOf("_android.so") != -1); 
		final String coreName = (isValidCoreName) ? file.getName().substring(0, file.getName().lastIndexOf("_android.so"))
												  : file.getName();

		// Now get the directory where all of the info files are kept (dataDir/info)
		final String infoFileDir  = context.getApplicationInfo().dataDir + File.separator + "info";

		// Now, based off of the trimmed core name, we can get the core info file.
		// and attempt to read it as a config file (since it has the same key-value layout).
		final String infoFilePath = infoFileDir + File.separator + coreName + ".info";
		if (new File(infoFilePath).exists())
		{
			final ConfigFile infoFile = new ConfigFile(infoFilePath);
	
			// Now read info out of the info file. Make them an empty string if the key doesn't exist.
			this.displayName  = (infoFile.keyExists("display_name")) ? infoFile.getString("display_name") : "N/A";
			this.coreName     = (infoFile.keyExists("corename"))     ? infoFile.getString("corename")     : "N/A";
			this.systemName   = (infoFile.keyExists("systemname"))   ? infoFile.getString("systemname")   : "N/A";
			this.manufacturer = (infoFile.keyExists("manufacturer")) ? infoFile.getString("manufacturer") : "N/A";
			this.license      = (infoFile.keyExists("license"))      ? infoFile.getString("license")      : "N/A";
	
			// Getting supported extensions and authors is a little different.
			// We need to split at every '|' character, since it is
			// the delimiter for a new extension that the core supports.
			//
			// Cores that don't have multiple extensions supported
			// don't contain the '|' delimiter, so we just create a String list
			// and just directly assign the retrieved extensions to it.
			final String supportedExts = infoFile.getString("supported_extensions");
			if (supportedExts != null && supportedExts.contains("|"))
			{
				this.supportedExtensions = new ArrayList<String>(Arrays.asList(supportedExts.split("\\|")));
			}
			else
			{
				this.supportedExtensions = new ArrayList<String>();
				this.supportedExtensions.add(supportedExts);
			}

			final String emuAuthors = infoFile.getString("authors");
			if (emuAuthors != null && emuAuthors.contains("|"))
			{
				this.authors = new ArrayList<String>(Arrays.asList(emuAuthors.split("\\|")));
			}
			else
			{
				this.authors = new ArrayList<String>();
				this.authors.add(emuAuthors);
			}
		}
		else // No info file.
		{
			this.displayName = "N/A";
			this.systemName = "N/A";
			this.manufacturer = "N/A";
			this.license = "N/A";
			this.authors = new ArrayList<String>();
			this.supportedExtensions = new ArrayList<String>();
			this.coreName = coreName;
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
	 * Gets the internal core name for this wrapped core.
	 * 
	 * @return the internal core name for this wrapped core.
	 */
	public String getInternalName()
	{
		return coreName;
	}

	/**
	 * Gets the name of the system that is emulated by this wrapped core.
	 * 
	 * @return the name of the system that is emulated by this wrapped core.
	 */
	public String getEmulatedSystemName()
	{
		return systemName;
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
	 * Gets the list of authors of this emulator core.
	 * 
	 * @return the list of authors of this emulator core.
	 */
	public List<String> getEmulatorAuthors()
	{
		return authors;
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