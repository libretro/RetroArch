package com.retroarch.browser.preferences.util;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

import android.util.Log;

/**
 * Represents a configuration file that works off of a key-value pair
 * in the form [key name] = "[value]".
 */
public final class ConfigFile
{
	// Map containing all of the key-value pairs.
	private final HashMap<String, String> map = new HashMap<String, String>();

	/**
	 * Constructor
	 */
	public ConfigFile()
	{
	}

	/**
	 * Constructor
	 * 
	 * @param filePath The path to the configuration file to open.
	 */
	public ConfigFile(String filePath)
	{
		try 
		{
			open(filePath);
		}
		catch (IOException ioe)
		{
			Log.e("ConfigFile", "Stream reading the configuration file was suddenly closed for an unknown reason.");
		}
	}

	/**
	 * Parses a configuration file from the given stream
	 * and appends the parsed values to the key-value map.
	 * 
	 * @param stream The {@link InputStream} containing the configuration file to parse.
	 */
	public void append(InputStream stream) throws IOException
	{
		BufferedReader br = new BufferedReader(new InputStreamReader(stream));

		String line;
		while ((line = br.readLine()) != null)
			parseLine(line);

		br.close();
	}

	/**
	 * Opens a configuration file given by configPath
	 * and parses all of its key-value pairs, adding
	 * them to the key-value map.
	 * 
	 * @param configPath Path to the configuration file to parse.
	 */
	public void open(String configPath) throws IOException
	{
		clear();
		append(new FileInputStream(configPath));
	}

	private void parseLine(String line)
	{
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

	/**
	 * Clears the key-value map of all currently set keys and values.
	 */
	public void clear()
	{
		map.clear();
	}

	/**
	 * Writes the currently set key-value pairs to 
	 * 
	 * @param path         The path to save the 
	 * 
	 * @throws IOException
	 */
	public void write(String path) throws IOException
	{
		PrintWriter writer = new PrintWriter(path);

		for (Map.Entry<String, String> entry : map.entrySet())
		{
			writer.println(entry.getKey() + " = \"" + entry.getValue() + "\"");
		}

		writer.close();
	}

	/**
	 * Checks if a key exists in the {@link HashMap}
	 * backing this ConfigFile instance.
	 * 
	 * @param key The key to check for.
	 * 
	 * @return true if the key exists in the HashMap backing
	 *         this ConfigFile; false if it doesn't.
	 */
	public boolean keyExists(String key)
	{
		return map.containsKey(key);
	}

	/**
	 * Sets a key to the given String value.
	 * 
	 * @param key   The key to set the String value to.
	 * @param value The String value to set to the key.
	 */
	public void setString(String key, String value)
	{
		map.put(key, value);
	}

	/**
	 * Sets a key to the given boolean value.
	 * 
	 * @param key   The key to set the boolean value to.
	 * @param value The boolean value to set to the key.
	 */
	public void setBoolean(String key, boolean value)
	{
		map.put(key, Boolean.toString(value));
	}

	/**
	 * Sets a key to the given Integer value.
	 * 
	 * @param key   The key to set the Integer value to.
	 * @param value The Integer value to set to the key.
	 */
	public void setInt(String key, int value)
	{
		map.put(key, Integer.toString(value));
	}

	/**
	 * Sets a key to the given double value.
	 * 
	 * @param key   The key to set the double value to.
	 * @param value The double value to set to the key.
	 */
	public void setDouble(String key, double value)
	{
		map.put(key, Double.toString(value));
	}
	
	/**
	 * Sets a key to the given float value.
	 * 
	 * @param key   The key to set the float value to.
	 * @param value The float value to set to the key.
	 */
	public void setFloat(String key, float value)
	{
		map.put(key, Float.toString(value));
	}

	/**
	 * Gets the String value associated with the given key.
	 * 
	 * @param key The key to get the String value from.
	 * 
	 * @return the String object associated with the given key.
	 */
	public String getString(String key)
	{
		String ret = map.get(key);

		if (ret != null)
			return ret;
		else
			return null;
	}

	/**
	 * Gets the Integer value associated with the given key.
	 * 
	 * @param key The key to get the Integer value from.
	 * 
	 * @return the Integer value associated with the given key.
	 */
	public int getInt(String key)
	{
		String str = getString(key);

		if (str != null)
			return Integer.parseInt(str);
		else
			throw new IllegalArgumentException("Config key '" + key + "' is invalid.");
	}

	/**
	 * Gets the double value associated with the given key.
	 * 
	 * @param key The key to get the double value from.
	 * 
	 * @return the double value associated with the given key.
	 */
	public double getDouble(String key)
	{
		String str = getString(key);

		if (str != null)
			return Double.parseDouble(str);
		else
			throw new IllegalArgumentException("Config key '" + key + "' is invalid.");
	}

	/**
	 * Gets the float value associated with the given key.
	 * 
	 * @param key The key to get the float value from.
	 * 
	 * @return the float value associated with the given key.
	 */
	public float getFloat(String key)
	{
		String str = getString(key);

		if (str != null)
			return Float.parseFloat(str);
		else
			throw new IllegalArgumentException("Config key '" + key + "' is invalid.");
	}

	/**
	 * Gets the boolean value associated with the given key.
	 * 
	 * @param key The key to get the boolean value from.
	 * 
	 * @return the boolean value associated with the given key.
	 */
	public boolean getBoolean(String key)
	{
		String str = getString(key);

		if (str != null)
			return Boolean.parseBoolean(str);
		else
			throw new IllegalArgumentException("Config key '" + key + "' is invalid.");
	}
}
