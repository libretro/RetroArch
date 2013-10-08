package org.retroarch.browser;

import java.io.File;

import org.retroarch.R;

import android.graphics.drawable.Drawable;

public final class FileWrapper implements IconAdapterItem {
	public final File file;
	public final boolean parentItem;
	public final boolean dirSelectItem;
	
	protected final boolean enabled;
	
	public static final int DIRSELECT = 0;
	public static final int PARENT = 1;
	public static final int FILE = 2;
	
	protected final int typeIndex;

	public FileWrapper(File aFile, int type, boolean aIsEnabled) {
		file = aFile;
		
		parentItem = type == PARENT;
		dirSelectItem = type == DIRSELECT;		
		typeIndex = type == FILE ? (FILE + (file.isDirectory() ? 0 : 1)) : type;	
		
		enabled = parentItem || dirSelectItem || aIsEnabled;
	}

	@Override
	public boolean isEnabled() {
		return enabled;
	}

	@Override
	public String getText() {
		if (dirSelectItem)
			return "[[Use this directory]]";
		else if (parentItem)
			return "[Parent Directory]";
		else
			return file.getName();
	}
	
	@Override
	public String getSubText() {
		return null;
	}

	@Override
	public int getIconResourceId() {
		if (!parentItem && !dirSelectItem) {
			return file.isFile() ? R.drawable.ic_file : R.drawable.ic_dir;
		} else {
			return R.drawable.ic_dir;
		}
	}

	@Override
	public Drawable getIconDrawable() {
		return null;
	}

	public int compareTo(FileWrapper aOther) {
		if (aOther != null) {
			// Who says ternary is hard to follow
			if (isEnabled() == aOther.isEnabled()) {
				return (typeIndex == aOther.typeIndex) ? file
						.compareTo(aOther.file)
						: ((typeIndex < aOther.typeIndex) ? -1 : 1);
			} else {
				return isEnabled() ? -1 : 1;
			}
		}

		return -1;
	}
}
