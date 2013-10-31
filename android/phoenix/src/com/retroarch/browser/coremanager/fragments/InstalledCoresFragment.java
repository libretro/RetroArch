package com.retroarch.browser.coremanager.fragments;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.retroarch.R;
import com.retroarch.browser.ModuleWrapper;
import com.retroarch.browser.preferences.util.UserPreferences;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.os.Bundle;
import android.support.v4.app.ListFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

/**
 * {@link ListFragment} that displays all of the currently installed cores
 */
public final class InstalledCoresFragment extends ListFragment
{
	// Adapter backing this ListFragment.
	private InstalledCoresAdapter adapter;

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// The list of items that will be added to the adapter backing this ListFragment.
		final List<ModuleWrapper> items = new ArrayList<ModuleWrapper>();

		// Check if the device supports NEON.
		final String cpuInfo = UserPreferences.readCPUInfo();
		final boolean supportsNeon = cpuInfo.contains("neon");

		// Populate the list
		final File[] libs = new File(getActivity().getApplicationInfo().dataDir, "/cores").listFiles();
		for (File lib : libs)
		{
			String libName = lib.getName();

			// Never append a NEON lib if we don't have NEON.
			if (libName.contains("neon") && !supportsNeon)
				continue;

			// If we have a NEON version with NEON capable CPU,
			// never append a non-NEON version.
			if (supportsNeon && !libName.contains("neon"))
			{
				boolean hasNeonVersion = false;
				for (File lib_ : libs)
				{
					String otherName = lib_.getName();
					String baseName = libName.replace(".so", "");

					if (otherName.contains("neon") && otherName.startsWith(baseName))
					{
						hasNeonVersion = true;
						break;
					}
				}

				if (hasNeonVersion)
					continue;
			}

			// Add it to the list.
			items.add(new ModuleWrapper(getActivity(), lib));
		}

		// Sort the list alphabetically
		Collections.sort(items);

		// Initialize and set the backing adapter for this ListFragment.
		adapter = new InstalledCoresAdapter(getActivity(), R.layout.coremanager_list_item, items);
		setListAdapter(adapter);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
	{
		// Inflate the layout for this ListFragment.
		View parentView = inflater.inflate(R.layout.coremanager_listview, container, false);
		
		// Set the long click listener.
		ListView mainList = (ListView) parentView.findViewById(android.R.id.list);
		mainList.setOnItemLongClickListener(itemLongClickListener);

		return mainList;
	}

	// This will be the handler for long clicks on individual list items in this ListFragment.
	private final OnItemLongClickListener itemLongClickListener = new OnItemLongClickListener()
	{
		@Override
		public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id)
		{
			// Begin building the AlertDialog
			final ModuleWrapper item = adapter.getItem(position);
			final AlertDialog.Builder alert = new AlertDialog.Builder(getActivity());
			alert.setTitle(R.string.uninstall_core);
			alert.setMessage(String.format(getString(R.string.uninstall_core_message), item.getText()));
			alert.setNegativeButton(R.string.no, null);
			alert.setPositiveButton(R.string.yes, new OnClickListener()
			{
				@Override
				public void onClick(DialogInterface dialog, int which)
				{
					// Attempt to uninstall the core item.
					if (item.getUnderlyingFile().delete())
					{
						Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_success), item.getText()), Toast.LENGTH_LONG).show();
						adapter.remove(item);
						adapter.notifyDataSetChanged();
					}
					else // Failed to uninstall.
					{
						Toast.makeText(getActivity(), String.format(getString(R.string.uninstall_failure), item.getText()), Toast.LENGTH_LONG).show();
					}
				}
			});
			alert.show();

			return true;
		}
	};

	/**
	 * The {@link ArrayAdapter} that backs this InstalledCoresFragment.
	 */
	private final class InstalledCoresAdapter extends ArrayAdapter<ModuleWrapper>
	{
		private final Context context;
		private final int resourceId;
		private final List<ModuleWrapper> items;

		/**
		 * Constructor
		 * 
		 * @param context    The current {@link Context}.
		 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
		 * @param items      The list of items to represent in this adapter.
		 */
		public InstalledCoresAdapter(Context context, int resourceId, List<ModuleWrapper> items)
		{
			super(context, resourceId, items);

			this.context = context;
			this.resourceId = resourceId;
			this.items = items;
		}

		@Override
		public ModuleWrapper getItem(int i)
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

			final ModuleWrapper item = items.get(position);
			if (item != null)
			{
				TextView title    = (TextView) convertView.findViewById(R.id.CoreManagerListItemTitle);
				TextView subtitle = (TextView) convertView.findViewById(R.id.CoreManagerListItemSubTitle);
				ImageView icon    = (ImageView) convertView.findViewById(R.id.CoreManagerListItemIcon);

				if (title != null)
				{
					title.setText(item.getText());
				}

				if (subtitle != null)
				{
					subtitle.setText(item.getSubText());
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
