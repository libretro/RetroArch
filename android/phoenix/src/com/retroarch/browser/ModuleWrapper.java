package com.retroarch.browser;

import java.io.File;

import com.retroarch.browser.preferences.util.ConfigFile;

import android.content.Context;
import android.graphics.drawable.Drawable;

final class ModuleWrapper implements IconAdapterItem {
	public final File file;
	private final ConfigFile config;

	public ModuleWrapper(Context context, File file, ConfigFile config) {
		this.file = file;
		this.config = config;
	}

	@Override
	public boolean isEnabled() {
		return true;
	}

	@Override
	public String getText() {
		String stripped = file.getName().replace(".so", "");
		if (config.keyExists(stripped)) {
			return config.getString(stripped);
		} else
			return stripped;
	}
	
	@Override
	public String getSubText() {
		String stripped = file.getName().replace(".so", "") + "_system";
		if (config.keyExists(stripped)) {
			return config.getString(stripped);
		} else
			return null;
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