package org.andretro.emulator;

import android.content.*;
import android.os.*;

import java.io.*;
import java.util.*;

import javax.xml.parsers.*;
import javax.xml.xpath.*;
import org.w3c.dom.*;

public class ModuleInfo
{
	// TODO: Make them not public!
	public String name;
	public String shortName;
	public String libraryName;
	public String[] extensions;
	
	public String dataPath;
	
	private Document document;
	private Element onScreenInputData;
	public final Doodads.Set inputData;

	private static final Map<String, ModuleInfo> cache = new HashMap<String, ModuleInfo>(); 
	
	public static ModuleInfo getInfoAbout(final Context aContext, final File aFile)
	{
		final String key = aFile.getName() + ".xml";
		
		if(cache.containsKey(key))
		{
			return cache.get(key);
		}
		else
		{
			ModuleInfo newInfo = new ModuleInfo(aContext, aFile);
			cache.put(key, newInfo);
			return newInfo;
		}
	}
	
	private ModuleInfo(final Context aContext, final File aFile)
	{
        try
        {
        	// Why does xml have to be so god damned difficult?
    		document = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(aContext.getAssets().open(aFile.getName() + ".xml"));
    		final XPath xpath = XPathFactory.newInstance().newXPath();
    		
    		// Read system info
    		Element system = (Element)xpath.evaluate("/retro/system", document, XPathConstants.NODE);
    		name = system.getAttribute("fullname");
    		shortName = system.getAttribute("shortname");
    		
    		// Read module info
    		Element module = (Element)xpath.evaluate("/retro/module", document, XPathConstants.NODE);
    		libraryName = module.getAttribute("libraryname");
    		extensions = module.getAttribute("extensions").split("\\|");
    		Arrays.sort(extensions);

    		// Read input element
    		onScreenInputData = (Element)xpath.evaluate("/retro/onscreeninput", document, XPathConstants.NODE);
    		inputData = new Doodads.Set(aContext.getSharedPreferences(libraryName, 0), "input", (Element)xpath.evaluate("/retro/input", document, XPathConstants.NODE));
    		
        	// Quick check hack
        	if(null == name || null == shortName || null == libraryName || null == extensions)
        	{
        		throw new Exception("Not all elements present in xml");
        	}
        	
        	// Build Directories
        	dataPath = Environment.getExternalStorageDirectory().getPath() + "/andretro/" + libraryName;
        	new File(dataPath + "/Games").mkdirs();
        }
        catch(final Exception e)
        {
        	throw new RuntimeException(e);
        }
	}
	
	public String getDataName()
	{
		return libraryName;
	}
	
	public String getDataPath()
	{
		return dataPath;
	}
	
	public boolean isFileValid(File aFile)
	{
    	final String path = aFile.getAbsolutePath(); 
        final int dot = path.lastIndexOf(".");
        final String extension = (dot < 0) ? null : path.substring(dot + 1).toLowerCase();

    	return (null == extension) ? false : (0 <= Arrays.binarySearch(extensions, extension));
	}
	
	public Element getOnScreenInputDefinition()
	{
		return onScreenInputData;
	}
}
