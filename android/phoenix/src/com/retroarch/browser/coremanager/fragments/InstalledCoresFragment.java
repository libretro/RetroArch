package com.retroarch.browser.coremanager.fragments;

import java.util.ArrayList;
import java.util.List;

import com.retroarch.R;
import com.retroarch.browser.coremanager.CoreManagerListItem;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

/**
 * {@link ListFragment} that displays all of the currently installed cores
 */
public final class InstalledCoresFragment extends ListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// List which will contain all of the items for the list.
		List<CoreManagerListItem> items = new ArrayList<CoreManagerListItem>();

		// TODO: Populate list adapter.

		// Set the list adapter.
		final InstalledCoresAdapter adapter = new InstalledCoresAdapter(getActivity(), R.layout.coremanager_list_item, items);
		setListAdapter(adapter);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		// Inflate the layout for this ListFragment.
		View parentView = inflater.inflate(R.layout.coremanager_listview_layout, container, false);

		return parentView.findViewById(android.R.id.list);
	}

	/**
	 * The {@link ArrayAdapter} that backs this InstalledCoresFragment.
	 */
	private final class InstalledCoresAdapter extends ArrayAdapter<CoreManagerListItem>
	{
		private final Context context;
		private final int resourceId;
		private final List<CoreManagerListItem> items;

		/**
		 * Constructor
		 * 
		 * @param context    The current {@link Context}.
		 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
		 * @param objects    The items to represent in the {@link ListView}.
		 */
		public InstalledCoresAdapter(Context context, int resourceId, List<CoreManagerListItem> items)
		{
			super(context, resourceId, items);

			this.context = context;
			this.resourceId = resourceId;
			this.items = items;
		}

		@Override
		public CoreManagerListItem getItem(int i)
		{
			return items.get(i);
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent)
		{
			if (convertView == null)
			{
				LayoutInflater vi = LayoutInflater.from(context);
				convertView = vi.inflate(resourceId, parent, false);
			}

			final CoreManagerListItem item = items.get(position);
			if (item != null)
			{
				TextView title    = (TextView) convertView.findViewById(R.id.CoreManagerListItemTitle);
				TextView subtitle = (TextView) convertView.findViewById(R.id.CoreManagerListItemSubTitle);
				ImageView icon    = (ImageView) convertView.findViewById(R.id.CoreManagerListItemIcon);

				if (title != null)
				{
					title.setText(item.getName());
				}

				if (subtitle != null)
				{
					subtitle.setText(item.getSubtitle());
				}

				if (icon != null)
				{
					// TODO: Set core icon.
				}
			}

			return convertView;
		}
	}
}
