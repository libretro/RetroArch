package com.retroarch.browser;

// Helper class which calls into JNI for various tasks.
public final class NativeInterface {

	static {
		System.loadLibrary("retroarch-jni");
	}

	public static native boolean extractArchiveTo(String archive,
			String subDirectory, String destinationFolder);
}
