package com.retroarch.browser.coremanager.fragments;

import java.util.List;

import android.text.TextUtils;

/**
 * Represents a single list item within the InstalledCoreInfoFragment.
 */
public final class InstalledCoreInfoItem
{
	private final String title;
	private final String subtitle;

	/**
	 * Constructor
	 * 
	 * @param title    Title of the item within the core info list.
	 * @param subtitle Subtitle of the item within the core info list.
	 */
	public InstalledCoreInfoItem(String title, String subtitle)
	{
		this.title = title;
		this.subtitle = subtitle;
	}

	/**
	 * Constructor.
	 * <p>
	 * Allows for creating a subtitle out of multiple strings.
	 * 
	 * @param title    Title of the item within the core info list.
	 * @param subtitle List of strings to add to the subtitle section of this item.
	 */
	public InstalledCoreInfoItem(String title, List<String> subtitle)
	{
		this.title = title;
		this.subtitle = TextUtils.join(", ", subtitle);
	}

	/**
	 * Gets the title of this InstalledCoreInfoItem.
	 * 
	 * @return the title of this InstalledCoreInfoItem.
	 */
	public String getTitle()
	{
		return title;
	}

	/**
	 * Gets the subtitle of this InstalledCoreInfoItem.
	 * 
	 * @return the subtitle of this InstalledCoreInfoItem.
	 */
	public String getSubtitle()
	{
		return subtitle;
	}
}
