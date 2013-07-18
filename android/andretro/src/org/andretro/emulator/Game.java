package org.andretro.emulator;

import org.andretro.system.*;
import org.libretro.*;

import java.io.*;

import android.app.*;

public final class Game
{
    // Singleton
    static
    {
    	System.loadLibrary("retroiface");
    	
    	if(!LibRetro.nativeInit())
    	{
    		throw new RuntimeException("Failed to initialize JNI classes.");
    	}
    }
 
    // Thread
    private static final CommandQueue eventQueue = new CommandQueue();
    
    public static void queueCommand(final CommandQueue.BaseCommand aCommand)
    {
		eventQueue.queueCommand(aCommand);
    }

    // Game Info
    static int pauseDepth;
    
    // Fast forward
    static int fastForwardKey;
    static int fastForwardSpeed = 1;
    static boolean fastForwardDefault;
    static int rewindKey;
    static String screenShotName;

    // Functions to retrieve game data, careful as the data may be null, or outdated!            
    public static String getGameDataName(String aSubDirectory, String aExtension)
    {
    	final File dir = new File(moduleInfo.getDataPath() + "/" + aSubDirectory);
    	if(!dir.exists() && !dir.mkdirs())
    	{
    		throw new RuntimeException("Failed to make data directory");
    	}
    	
    	return moduleInfo.getDataPath() + "/" + aSubDirectory + "/" + dataName + "." + aExtension;
    }
            
    // LIBRARY
    static Activity loadingActivity;
    private static ModuleInfo moduleInfo;
    private static boolean gameLoaded;
    private static boolean gameClosed;
    private static LibRetro.SystemInfo systemInfo = new LibRetro.SystemInfo();
    private static LibRetro.AVInfo avInfo = new LibRetro.AVInfo();
    private static String dataName;

    static void loadGame(Activity aActivity, String aLibrary, File aFile)
    {	
    	if(!gameLoaded && !gameClosed && null != aFile && aFile.isFile())
    	{
    		moduleInfo = ModuleInfo.getInfoAbout(aActivity, new File(aLibrary));
    		
    		if(LibRetro.loadLibrary(aLibrary, moduleInfo.getDataPath()))
    		{
    			LibRetro.init();
    			
    			if(LibRetro.loadGame(aFile.getAbsolutePath()))
    			{
    				// System info
    				LibRetro.getSystemInfo(systemInfo);
    				LibRetro.getSystemAVInfo(avInfo);
    				
    				// Filesystem stuff    				
    	        	dataName = aFile.getName().split("\\.(?=[^\\.]+$)")[0];
    	        	LibRetro.readMemoryRegion(LibRetro.RETRO_MEMORY_SAVE_RAM, getGameDataName("SaveRAM", "srm"));
    	        	LibRetro.readMemoryRegion(LibRetro.RETRO_MEMORY_RTC, getGameDataName("SaveRAM", "rtc"));

    	        	// Load settings
    	        	loadingActivity = aActivity;
    				new Commands.RefreshSettings(aActivity.getSharedPreferences(moduleInfo.getDataName(), 0)).run();
    	            new Commands.RefreshInput().run();
    	            
    	            gameLoaded = true;
    			}
    		}
    	}
    	else
    	{
    		gameLoaded = true;
    		gameClosed = true;
    		throw new RuntimeException("Failed to load game");
    	}
    }
    
    static void closeGame()
    {
    	if(gameLoaded && !gameClosed)
    	{
        	LibRetro.writeMemoryRegion(LibRetro.RETRO_MEMORY_SAVE_RAM, getGameDataName("SaveRAM", "srm"));
        	LibRetro.writeMemoryRegion(LibRetro.RETRO_MEMORY_RTC, getGameDataName("SaveRAM", "rtc"));

   			LibRetro.unloadGame();
    		LibRetro.deinit();
    		LibRetro.unloadLibrary();
   			
    		// Shutdown any audio device
    		Audio.close();
    		
    		gameClosed = true;
    	}    	
    }
        
    public static boolean hasGame()
    {
        return gameLoaded && !gameClosed;
    }

    // LOOP
    public static boolean doFrame(LibRetro.VideoFrame aFrame)
    {
		eventQueue.pump();
		
		if(gameLoaded && !gameClosed && 0 == pauseDepth)
		{	
			// Fast forward
			aFrame.rewind = Input.isPressed(rewindKey);

			final boolean fastKeyPressed = Input.isPressed(fastForwardKey);
			final int frameToggle = fastForwardDefault ? 1 : fastForwardSpeed;
			final int frameTarget = fastForwardDefault ? fastForwardSpeed : 1;
			aFrame.framesToRun = (fastKeyPressed) ? frameToggle : frameTarget;
			
			// Write any pending screen shots
			if(null != screenShotName && !aFrame.restarted)
			{
				PngWriter.write(screenShotName);
				screenShotName = null;
			}

			// Get input data
			Input.poolKeyboard(aFrame.keyboard);
			aFrame.buttons[0] = Input.getBits(moduleInfo.inputData.getDevice(0,  0));
			aFrame.touchX = Input.getTouchX();
			aFrame.touchY = Input.getTouchY();
			aFrame.touched = Input.getTouched();
						
            //Emulate
			LibRetro.run(aFrame);
						
			// Present
			if(0 != aFrame.audioSamples)
			{
				Audio.write((int)avInfo.sampleRate, aFrame.audio, aFrame.audioSamples);
			}
			
			return true;
		}
		
		return false;
    }
}

