package org.retroarch.browser;

public final class AssetExtractor {
	final private String archive;

	public AssetExtractor(String archive) {
		this.archive = archive;
	}

	static {
		System.loadLibrary("apk-extract");
	}

	public boolean extractTo(String subDirectory, String destinationFolder) {
		return extractArchiveTo(archive, subDirectory, destinationFolder);
	}

	private static native boolean extractArchiveTo(String archive,
			String subDirectory, String destinationFolder);
}
