package com.retroarch.browser;

import java.util.List;

import com.retroarch.R;

import android.content.*;
import android.view.*;
import android.widget.*;

/**
 * An {@link ArrayAdapter} derivative that can back a {@link View}
 * that accepts ArrayAdapters. Items within this ArrayAdapter derivative
 * must implement the {@link IconAdapterItem} interface.
 *
 * @param <T> The type of the item that will be within this IconAdapter.
 *            This type must implement the {@link IconAdapterItem} interface.
 */
public final class IconAdapter<T extends IconAdapterItem> extends ArrayAdapter<T>
{
	private final int resourceId;
	private final Context context;

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 */
	public IconAdapter(Context context, int resourceId)
	{
		super(context, resourceId);
		this.context = context;
		this.resourceId = resourceId;
	}

	/**
	 * Constructor
	 * 
	 * @param context    The current {@link Context}.
	 * @param resourceId The resource ID for a layout file containing a layout to use when instantiating views.
	 * @param items      The list of items to store within this IconAdapter.
	 */
	public IconAdapter(Context context, int resourceId, List<T> items)
	{
		super(context, resourceId, items);
		this.context = context;
		this.resourceId = resourceId;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
		// Build the view
		if (convertView == null)
		{
			LayoutInflater inflater = LayoutInflater.from(context);
			convertView = inflater.inflate(resourceId, parent, false);
		}

		// Fill the view
		IconAdapterItem item = getItem(position);
		final boolean enabled = item.isEnabled();

		TextView title = (TextView) convertView.findViewById(R.id.name);
		if (title != null)
		{
			title.setText(item.getText());
			title.setEnabled(enabled);
		}
		
		TextView subtitle  = (TextView) convertView.findViewById(R.id.sub_name);
		if (subtitle != null)
		{
			String subText = item.getSubText();
			if (subText != null)
			{
				subtitle.setVisibility(View.VISIBLE);
				subtitle.setEnabled(item.isEnabled());
				subtitle.setText(subText);
			}
		}

		ImageView imageView = (ImageView) convertView.findViewById(R.id.icon);
		if (imageView != null)
		{
			if (enabled)
			{
				final int id = item.getIconResourceId();
				if (id != 0)
				{
					imageView.setImageResource(id);
				}
				else
				{
					imageView.setImageDrawable(item.getIconDrawable());
				}
			}
			else
			{
				imageView.setImageDrawable(null);
			}
		}

		return convertView;
	}

	@Override
	public boolean isEnabled(int position)
	{
		return getItem(position).isEnabled();
	}
}
