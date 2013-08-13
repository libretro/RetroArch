package org.retroarch.browser;

import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

abstract class LazyPopupMenu {
	public abstract Menu getMenu();
	public abstract MenuInflater getMenuInflater();
	public abstract void setOnMenuItemClickListener(LazyPopupMenu.OnMenuItemClickListener listener);
	public abstract void show();
	public interface OnMenuItemClickListener {
		public abstract boolean onMenuItemClick(MenuItem item);
	}
}