package com.retroarch.browser.coremanager.fragments;

/**
 * Represents a core that can be downloaded.
 */
final class DownloadableCore
{
	private final String coreName;
	private final String coreURL;
	private final String shortURL;

	/**
	 * Constructor
	 *
	 * @param coreName Name of the core.
	 * @param coreURL  URL to this core.
	 */
	public DownloadableCore(String coreName, String coreURL)
	{
		this.coreName = coreName;
		this.coreURL  = coreURL;
		this.shortURL = coreURL.substring(coreURL.lastIndexOf('/') + 1);
	}

	/**
	 * Gets the name of this core.
	 *
	 * @return The name of this core.
	 */
	public String getCoreName()
	{
		return coreName;
	}

	/**
	 * Gets the URL to download this core.
	 *
	 * @return The URL to download this core.
	 */
	public String getCoreURL()
	{
		return coreURL;
	}

	/**
	 * Gets the short URL name of this core.
	 * <p>
	 * e.g. Consider the url: www.somesite/somecore.zip.
	 *      This would return "somecore.zip"
	 *
	 * @return the short URL name of this core.
	 */
	public String getShortURLName()
	{
		return shortURL;
	}
}
