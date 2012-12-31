package org.retroarch.browser;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileInputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

public class ConfigFile {
	private HashMap<String, String> map = new HashMap<String, String>();

	public void append(File file) throws IOException {
		BufferedReader br = new BufferedReader(new InputStreamReader(
				new FileInputStream(file.getAbsolutePath())));
		
		String line;
		while ((line = br.readLine()) != null)
			parseLine(line);
		
		br.close();
	}
	
	public void open(File file) throws IOException {
		clear();
		append(file);
	}
	
	private void parseLine(String line) {
		String[] tokens = line.split("=", 2);
		if (tokens.length < 2) {
			System.err.println("Didn't find two tokens in config line ...");
			return;
		}
		
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
		
		System.out.println("Parsed: \"" + key + "\" => \"" + value + "\"");
	}

	public void clear() {
		map.clear();
	}

	public void write(File file) throws IOException {
		PrintWriter writer = new PrintWriter(file.getAbsolutePath());
		for (Map.Entry<String, String> entry : map.entrySet()) {
			System.out.println("Key: " + entry.getKey() + " Value: "
					+ entry.getValue());
			writer.println(entry.getKey() + " = \"" + entry.getValue() + "\"");
		}
		writer.close();
	}

	public void setString(String key, String value) {
		map.put(key, value);
	}

	public void setInt(String key, int value) {
		map.put(key, Integer.toString(value));
	}

	public void setDouble(String key, double value) {
		map.put(key, Double.toString(value));
	}
	
	public String getString(String key) {
		Object ret = map.get(key);
		if (ret != null)
			return (String)ret;
		else
			return null;
	}
	
	public boolean keyExists(String key) {
		return map.containsKey(key);
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
}
