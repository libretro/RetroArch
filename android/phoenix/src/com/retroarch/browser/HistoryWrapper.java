package com.retroarch.browser;

import java.io.File;

import android.graphics.drawable.Drawable;

/**
 * Wraps a previously played game along with its core
 * for placement within the previously played history.
 */
public final class HistoryWrapper implements IconAdapterItem {

	private String gamePath;
	private String gamePathShort;
	private String corePath;
	private String coreName;

	/**
	 * Constructor
	 * 
	 * @param gamePath Path to the previously played game.
	 * @param corePath Path to the core the previously played game uses.
	 * @param coreName The actual name of the core.
	 */
	public HistoryWrapper(String gamePath, String corePath, String coreName) {
		this.gamePath = gamePath;
		this.corePath = corePath;
		this.coreName = coreName;
		
		File file = new File(gamePath);
		gamePathShort = file.getName();
		try {
			gamePathShort = gamePathShort.substring(0, gamePathShort.lastIndexOf('.'));
		} catch (IndexOutOfBoundsException e) {
		}
	}

	/**
	 * Gets the path to the previously played game.
	 * 
	 * @return the path to the previously played game.
	 */
	public String getGamePath() {
		return gamePath;
	}

	/**
	 * Gets the path to the core that the previously played game uses.
	 * 
	 * @return the path to the core that the previously played game uses.
	 */
	public String getCorePath() {
		return corePath;
	}

	/**
	 * Gets the name of the core used with the previously played game.
	 * 
	 * @return the name of the core used with the previously played game.
	 */
	public String getCoreName() {
		return coreName;
	}

	@Override
	public boolean isEnabled() {
		return true;
	}

	@Override
	public String getText() {
		return gamePathShort;
	}

	@Override
	public String getSubText() {
		return coreName;
	}

	@Override
	public int getIconResourceId() {
		return 0;
	}

	@Override
	public Drawable getIconDrawable() {
		return null;
	}
}
