package org.retroarch.browser;

import org.retroarch.R;

import android.app.*;
import android.content.*;
import android.graphics.drawable.*;
import android.view.*;
import android.widget.*;

interface IconAdapterItem {
	public abstract boolean isEnabled();
	public abstract String getText();
	public abstract String getSubText();
	public abstract int getIconResourceId();
	public abstract Drawable getIconDrawable();
}

class IconAdapter<T extends IconAdapterItem> extends ArrayAdapter<T> {
	private final int layout;

	public IconAdapter(Activity aContext, int aLayout) {
		super(aContext, aLayout);
		layout = aLayout;
	}

	@Override
	public View getView(int aPosition, View aConvertView, ViewGroup aParent) {
		// Build the view
		if (aConvertView == null) {
			LayoutInflater inflater = (LayoutInflater) aParent.getContext()
					.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			aConvertView = inflater.inflate(layout, aParent, false);
		}

		// Fill the view
		IconAdapterItem item = getItem(aPosition);
		final boolean enabled = item.isEnabled();

		TextView textView = (TextView) aConvertView.findViewById(R.id.name);
		if (null != textView) {
			textView.setText(item.getText());
			textView.setEnabled(enabled);
		}
		
		textView = (TextView) aConvertView.findViewById(R.id.sub_name);
		if (null != textView) {
			String subText = item.getSubText();
			if (null != subText) {
				textView.setVisibility(View.VISIBLE);
				textView.setEnabled(item.isEnabled());
				textView.setText(subText);
			}
		}

		ImageView imageView = (ImageView) aConvertView.findViewById(R.id.icon);
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

		return aConvertView;
	}

	@Override
	public boolean isEnabled(int aPosition) {
		return getItem(aPosition).isEnabled();
	}

}
