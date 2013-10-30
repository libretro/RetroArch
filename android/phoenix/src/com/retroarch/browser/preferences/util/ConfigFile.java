package com.retroarch.browser.preferences.util;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.FileInputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

public final class ConfigFile {
	private HashMap<String, String> map = new HashMap<String, String>();

	public void append(InputStream stream) throws IOException {
		BufferedReader br = new BufferedReader(new InputStreamReader(stream));

		String line;
		while ((line = br.readLine()) != null)
			parseLine(line);

		br.close();
	}

	public void open(File file) throws IOException {
		clear();
		append(new FileInputStream(file));
	}
	
	public ConfigFile(File file) throws IOException {
		open(file);
	}
	
	public ConfigFile() {}

	private void parseLine(String line) {
		String[] tokens = line.split("=", 2);
		if (tokens.length < 2)
			return;

		for (int i = 0; i < tokens.length; i++)
			tokens[i] = tokens[i].trim();

		String key = tokens[0];
		String value = tokens[1];

		if (value.startsWith("\""))
			value = value.substring(1, value.lastIndexOf('\"'));
		else
			value = value.split(" ")[0];

		if (value.length() > 0)
			map.put(key, value);
	}

	public void clear() {
		map.clear();
	}

	public void write(File file) throws IOException {
		PrintWriter writer = new PrintWriter(file.getAbsolutePath());
		for (Map.Entry<String, String> entry : map.entrySet())
			writer.println(entry.getKey() + " = \"" + entry.getValue() + "\"");
		writer.close();
	}

	public void setString(String key, String value) {
		map.put(key, value);
	}

	public void setBoolean(String key, boolean value) {
		map.put(key, Boolean.toString(value));
	}

	public void setInt(String key, int value) {
		map.put(key, Integer.toString(value));
	}

	public void setDouble(String key, double value) {
		map.put(key, Double.toString(value));
	}
	
	public void setFloat(String key, float value) {
		map.put(key, Float.toString(value));
	}

	public boolean keyExists(String key) {
		return map.containsKey(key);
	}

	public String getString(String key) {
		String ret = map.get(key);
		if (ret != null)
			return ret;
		else
			return null;
	}

	public int getInt(String key) throws NumberFormatException {
		String str = getString(key);
		if (str != null)
			return Integer.parseInt(str);
		else
			throw new NumberFormatException();
	}

	public double getDouble(String key) throws NumberFormatException {
		String str = getString(key);
		if (str != null)
			return Double.parseDouble(str);
		else
			throw new NumberFormatException();
	}
	
	public float getFloat(String key) throws NumberFormatException {
		String str = getString(key);
		if (str != null)
			return Float.parseFloat(str);
		else
			throw new NumberFormatException();
	}

	public boolean getBoolean(String key) {
		String str = getString(key);
		return Boolean.parseBoolean(str);
	}
}
