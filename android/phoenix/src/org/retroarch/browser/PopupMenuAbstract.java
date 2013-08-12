package org.retroarch.browser;

import android.content.Context;
import android.os.Build;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;

class PopupMenuAbstract extends LazyPopupMenu
{
	private LazyPopupMenu lazy;
	
	public PopupMenuAbstract(Context context, View anchor)
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy = new HoneycombPopupMenu(context, anchor);
		}
	}

	@Override
	public Menu getMenu() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			return lazy.getMenu();
		}
		else
		{
			return null;
		}
	}

	@Override
	public MenuInflater getMenuInflater() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			return lazy.getMenuInflater();
		}
		else
		{
			return null;
		}
	}

	@Override
	public void setOnMenuItemClickListener(PopupMenuAbstract.OnMenuItemClickListener listener) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy.setOnMenuItemClickListener(listener);
		}
	}

	@Override
	public void show() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
		{
			lazy.show();
		}
	}
}
