package com.retroarch.browser;

import android.graphics.drawable.Drawable;
import android.widget.ListView;

/**
 * Represents an item that is capable
 * of being within an {@link IconAdapter}.
 */
public interface IconAdapterItem
{
	/**
	 * Gets whether or not this item is
	 * enabled within the adapter.
	 * <p>
	 * This can be used for deciding whether or
	 * not to enable an item in a {@link ListView}
	 * if an IconAdapter is backing it.
	 * 
	 * @return true if this item is enabled; false otherwise.
	 */
	boolean isEnabled();

	/**
	 * Gets the title text of this IconAdapterItem.
	 * 
	 * @return the title text of this IconAdapterItem.
	 */
	String getText();

	/**
	 * Gets the subtitle text of this IconAdapterItem.
	 * 
	 * @return the subtitle text of this IconAdapterItem.
	 */
	String getSubText();

	/**
	 * Gets the resource ID of the icon to display
	 * alongside the text in this IconAdapterItem.
	 * <p>
	 * Returning zero means no icon is to be displayed.
	 * 
	 * @return the resource ID of this IconAdapterItem's icon.
	 */
	int getIconResourceId();

	/**
	 * Gets the actual {@link Drawable} object that represents
	 * the icon that is displayed with this IconAdapterItem.
	 * <p>
	 * Returning null means no icon is to be displayed.
	 * 
	 * @return the actual {@link Drawable} of this IconAdapterItem's icon.
	 */
	Drawable getIconDrawable();
}