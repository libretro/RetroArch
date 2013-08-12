package org.retroarch.browser;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;

@TargetApi(Build.VERSION_CODES.HONEYCOMB)
class HoneycombPopupMenu extends LazyPopupMenu {
	private PopupMenu instance;
	HoneycombPopupMenu.OnMenuItemClickListener listen;
	
	public HoneycombPopupMenu(Context context, View anchor)
	{
		instance = new PopupMenu(context, anchor);
	}

	@Override
	public void setOnMenuItemClickListener(HoneycombPopupMenu.OnMenuItemClickListener listener)
	{
		listen = listener;
		instance.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
			@Override
			public boolean onMenuItemClick(MenuItem item) {
				return listen.onMenuItemClick(item);
			}
			
		});
	}

	@Override
	public Menu getMenu() {
		return instance.getMenu();
	}

	@Override
	public MenuInflater getMenuInflater() {
		return instance.getMenuInflater();
	}

	@Override
	public void show() {
		instance.show();
	}
}
