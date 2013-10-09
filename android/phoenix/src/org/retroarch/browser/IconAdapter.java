package org.retroarch.browser;

import org.retroarch.R;

import android.content.*;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;

interface IconAdapterItem {
	public boolean isEnabled();
	public String getText();
	public String getSubText();
	public int getIconResourceId();
	public Drawable getIconDrawable();
}

public final class IconAdapter<T extends IconAdapterItem> extends ArrayAdapter<T> {
	private final int resourceId;
	private final Context context;

	public IconAdapter(Context context, int resourceId) {
		super(context, resourceId);
		this.context = context;
		this.resourceId = resourceId;
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		// Build the view
		if (convertView == null) {
			LayoutInflater inflater = LayoutInflater.from(context);
			convertView = inflater.inflate(resourceId, parent, false);
		}

		// Fill the view
		IconAdapterItem item = getItem(position);
		final boolean enabled = item.isEnabled();

		TextView textView = (TextView) convertView.findViewById(R.id.name);
		if (null != textView) {
			textView.setText(item.getText());
			textView.setEnabled(enabled);
		}
		
		textView = (TextView) convertView.findViewById(R.id.sub_name);
		if (null != textView) {
			String subText = item.getSubText();
			if (null != subText) {
				textView.setVisibility(View.VISIBLE);
				textView.setEnabled(item.isEnabled());
				textView.setText(subText);
			}
		}

		ImageView imageView = (ImageView) convertView.findViewById(R.id.icon);
		if (null != imageView) {
			if (enabled) {
				final int id = item.getIconResourceId();
				if (0 != id) {
					imageView.setImageResource(id);
				} else {
					imageView.setImageDrawable(item.getIconDrawable());
				}
			} else {
				imageView.setImageDrawable(null);
			}
		}

		return convertView;
	}

	@Override
	public boolean isEnabled(int aPosition) {
		return getItem(aPosition).isEnabled();
	}
}
