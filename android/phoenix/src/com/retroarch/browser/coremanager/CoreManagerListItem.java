package com.retroarch.browser.coremanager;

import java.io.File;

/**
 * Represents a list item within the CoreManager fragments.
 */
public final class CoreManagerListItem
{
	private final String name;
	private final String subtitle;
	private final String path;
	private final File underlyingFile;

	/**
	 * Constructor
	 * 
	 * @param name     The name of the core represented by this CoreManagerListItem.
	 * @param subtitle The subtitle description of the core represented by this CoreManagerListItem.
	 * @param path     The path to the core represented by this CoreManagerListItem.
	 */
	public CoreManagerListItem(String name, String subtitle, String path)
	{
		this.name = name;
		this.subtitle = subtitle;
		this.path = path;
		this.underlyingFile = new File(path);
	}

	/**
	 * Gets the name of the core represented by this CoreManagerListItem.
	 * 
	 * @return the name of the core represented by this CoreManagerListItem.
	 */
	public String getName()
	{
		return name;
	}

	/**
	 * Gets the subtitle description of the core represented by this CoreManagerListItem.
	 * 
	 * @return the subtitle description of the core represented by this CoreManagerListItem.
	 */
	public String getSubtitle()
	{
		return subtitle;
	}

	/**
	 * Gets the path to the core represented by this CoreManagerListItem.
	 * 
	 * @return the path to the core represented by this CoreManagerListItem.
	 */
	public String getPath()
	{
		return path;
	}

	/**
	 * Gets the underlying {@link File} instance to the core represented by this CoreManagerListItem.
	 * 
	 * @return the underlying {@link File} instance to the core represented by this CoreManagerListItem.
	 */
	public File getUnderlyingFile()
	{
		return underlyingFile;
	}
}
