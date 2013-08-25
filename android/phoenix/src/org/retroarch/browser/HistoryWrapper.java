package org.retroarch.browser;

import java.io.File;

import android.graphics.drawable.Drawable;

public class HistoryWrapper implements IconAdapterItem {
	
	private String gamePath;
	private String gamePathShort;
	private String corePath;
	private String coreName;
	
	public HistoryWrapper(String gamePath, String corePath, String coreName) {
		this.gamePath = gamePath;
		this.corePath = corePath;
		this.coreName = coreName;
		
		File file = new File(gamePath);
		gamePathShort = file.getName();
		try {
			gamePathShort = gamePathShort.substring(0, gamePathShort.lastIndexOf('.'));
		} catch (Exception e) {
		}
	}
	
	public String getGamePath() {
		return gamePath;
	}
	
	public String getCorePath() {
		return corePath;
	}
	
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
