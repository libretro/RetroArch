package org.andretro.emulator;

import java.util.*;

import android.content.*;
import org.w3c.dom.*;

public final class Doodads
{
	private static abstract class NamedInput
	{
		/**
		 * The short name of the input. Best used for setting keys.
		 */
		public final String name;
		
		/**
		 * The friendly name of the input. Best used for display.
		 */
		public final String fullName;
		
		/**
		 * Construct a new NamedInput.
		 * @param aShort The setting name of the input. May not be null.
		 * @param aFull The display name of the input. May not be null.
		 */
		NamedInput(String aShort, String aFull)
		{
			if(null == aShort || null == aFull)
			{
				throw new IllegalArgumentException("Neither name of a NamedInput may be null.");
			}
			
			name = aShort;
			fullName = aFull;
		}
	}

	private static abstract class GroupedInput <E extends NamedInput> extends NamedInput
	{
		/**
		 * List of input objects.
		 */
		protected final ArrayList<E> inputs;
		
		/**
		 * Create a new GroupedInput.
		 * @param aShort Setting name for the input. May not be null.
		 * @param aFull Display name for the input. May not be null.
		 * @param aCount Number of devices in this set.
		 */
		GroupedInput(String aShort, String aFull)
		{
			super(aShort, aFull);

			inputs = new ArrayList<E>();
		}

		public int getCount()
		{
			return inputs.size();
		}
		
		public ArrayList<E> getAll()
		{
			return inputs;
		}
		
		public E getItem(int aIndex)
		{
			return inputs.get(aIndex);
		}
		
		public E getItem(String aName)
		{
			for(E i: inputs)
			{
				if(aName.equals(i.name))
				{	
					return i;
				}
			}
			
			return null;
		}
	}
	
	public final static class Button extends NamedInput
	{
		public final String configKey;
		public final int bitOffset;
	    private int keyCode;

	    public static final String[] names = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "SELECT", "START", "L1", "R1", "L2", "R2", "L3", "R3"}; 
	    public static final int[] offsets = {4, 5, 6, 7, 8, 0, 9, 1, 2, 3, 10, 11, 12, 13, 14, 15};
	    
	    Button(SharedPreferences aPreferences, String aKeyBase, Element aData)
	    {
	    	super(aData.getAttribute("shortname"), aData.getAttribute("fullname"));

	    	// Get configKey
	    	configKey = aKeyBase + "_" + name; 
	    	
	    	// Get value
	    	keyCode = aPreferences.getInt(configKey, 0);
	    	
	    	// Grab native data
	    	final String keyName = aData.getAttribute("mappedkey");
	    	for(int i = 0; i != 16; i ++)
	    	{
	    		if(names[i].equals(keyName))
	    		{
	    			bitOffset = offsets[i];
	    			return;
	    		}
	    	}
	    	
    		throw new RuntimeException("Mapped key " + keyName + " not found");
	    }
	    
	    public int getKeyCode()
	    {
	    	return keyCode;
	    }
	    
	    public void setKeyCode(int aKeyCode)
	    {
	    	keyCode = aKeyCode;
	    }
	}
	
	public final static class Device extends GroupedInput<Button>
	{        
	    Device(SharedPreferences aPreferences, String aKeyBase, Element aData)
	    {
	    	super(aData.getAttribute("shortname"), aData.getAttribute("fullname"));

	    	final NodeList buttons = aData.getElementsByTagName("button");
	    	for(int i = 0; i != buttons.getLength(); i ++)
	    	{
	    		inputs.add(i, new Button(aPreferences, aKeyBase + "_" + name, (Element)buttons.item(i)));
	    	}
	    }    
	}
	
	public final static class Port extends GroupedInput<Device>
	{
	    public final String defaultDevice;
	    private final String currentDevice;
	    
	    Port(SharedPreferences aPreferences, String aKeyBase, Element aData)
	    {
	    	super(aData.getAttribute("shortname"), aData.getAttribute("fullname"));   
	    	defaultDevice = aData.getAttribute("defaultdevice");
	    	currentDevice = aPreferences.getString(aKeyBase + "_" + name, defaultDevice);
	    	
	    	final NodeList devices = aData.getElementsByTagName("device");
	    	for(int i = 0; i != devices.getLength(); i ++)
	    	{
	    		inputs.add(i, new Device(aPreferences, aKeyBase + "_" + name, (Element)devices.item(i)));
	    	}
	    }
	    
	    public String getCurrentDevice()
	    {
	    	return currentDevice;
	    }
	 }
	
	public final static class Set extends GroupedInput<Port>
	{
	    Set(SharedPreferences aPreferences, String aKeyBase, Element aData)
	    {
	    	super("root", "root");
	    	
	    	final NodeList ports = aData.getElementsByTagName("port");
	    	for(int i = 0; i != ports.getLength(); i ++)
	    	{
	    		inputs.add(new Port(aPreferences, aKeyBase, (Element)ports.item(i)));
	    	}
	    }
	        
		// Implement
	    public Doodads.Port getPort(int aPort)
	    {
	    	return getItem(aPort);
	    }
	    
	    public Doodads.Device getDevice(int aPort, int aDevice)
	    {
	    	return getItem(aPort).getItem(aDevice);    	
	    }
	    
	    public Doodads.Button getButton(int aPort, int aDevice, int aIndex)
	    {
	    	return getItem(aPort).getItem(aDevice).getItem(aIndex);
	    }
	}
}
