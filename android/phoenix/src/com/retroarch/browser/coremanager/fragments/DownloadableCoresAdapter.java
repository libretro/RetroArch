package com.retroarch.browser.coremanager.fragments;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

/**
 * {@link ArrayAdapter} that handles the display of downloadable cores.
 */
final class DownloadableCoresAdapter extends ArrayAdapter<DownloadableCore>
{
	private final int layoutId;

	/**
	 * Constructor
	 *
	 * @param context  The current {@link Context}.
	 * @param layoutID The resource ID for a layout file containing a layout to use when instantiating views
	 */
	public DownloadableCoresAdapter(Context context, int layoutId)
	{
		super(context, layoutId);

		this.layoutId = layoutId;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		if (convertView == null)
		{
			final LayoutInflater li = LayoutInflater.from(getContext());
			convertView = li.inflate(layoutId, parent, false);
		}

		final DownloadableCore core = getItem(position);
		if (core != null)
		{
			TextView title    = (TextView) convertView.findViewById(android.R.id.text1);
			TextView subtitle = (TextView) convertView.findViewById(android.R.id.text2);

			if (title != null)
				title.setText(core.getCoreName());

			if (subtitle != null)
				subtitle.setText(core.getCoreURL());
		}

		return convertView;
	}
}