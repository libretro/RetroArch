package org.retroarch.browser;

import android.os.Bundle;

public class ROMActivity extends DirectoryActivity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.addDisallowedExt(".state");
		super.addDisallowedExt(".srm");
		super.addDisallowedExt(".state.auto");
		super.addDisallowedExt(".rtc");
		super.onCreate(savedInstanceState);
	}
}
