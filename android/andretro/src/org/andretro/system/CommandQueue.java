package org.andretro.system;
import java.util.concurrent.*;

import android.app.*;

public class CommandQueue
{
    private final BlockingQueue<BaseCommand> eventQueue = new ArrayBlockingQueue<BaseCommand>(8);
	
    /**
     * Must be called before any other function
     * @param aThread
     */
            
    public void queueCommand(final BaseCommand aCommand)
    {
		// Put the event in the queue and notify any waiting clients that it's present. (This will wake the waiting emulator if needed.)
		eventQueue.add(aCommand);
    }

    public void pump()
    {
    	// Run all events
    	for(BaseCommand i = eventQueue.poll(); null != i; i = eventQueue.poll())
    	{
    		i.run();
    	}
    }	
	
    public static class Callback
    {
        private final Runnable callback;
        private final Activity activity;
        
        public Callback(Activity aActivity, Runnable aCallback)
        {
            callback = aCallback;
            activity = aActivity;
            
            if(null == callback || null == activity)
            {
                throw new RuntimeException("Neither aCallback nor aActivity may be null.");
            }
        }
        
        public void perform()
        {
            activity.runOnUiThread(callback);
        }
    }

	public static abstract class BaseCommand implements Runnable
	{
		private Callback finishedCallback;
				
		public BaseCommand setCallback(Callback aFinished)
		{
			finishedCallback = aFinished;
			return this;
		}
		
		@Override public final void run()
		{
			perform();
			
			if(null != finishedCallback)
			{
				finishedCallback.perform();
			}
		}
		
		abstract protected void perform();
	}
}
