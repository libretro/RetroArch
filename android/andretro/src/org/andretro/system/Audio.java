package org.andretro.system;
import android.media.*;

/**
 * A stateless audio stream.
 * 
 * Underlying audio object is created and managed transparently. User only needs to call write().
 * @author jason
 *
 */
public final class Audio
{
    private static AudioTrack audio;
    private static int rate;
    private static boolean failed;

    public synchronized static void open(int aRate)
    {
        close();

        rate = aRate;

        try
        {
        	final int bufferSize = AudioTrack.getMinBufferSize(aRate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
        	audio = new AudioTrack(AudioManager.STREAM_MUSIC, aRate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT, bufferSize, AudioTrack.MODE_STREAM);
        	audio.setStereoVolume(1, 1);
        	audio.play();
        	
        	failed = false;
        }
        catch(Exception e)
        {
        	audio = null;

        	failed = true;
        }
    }

    public synchronized static void close()
    {
        if(null != audio)
        {
            audio.stop();
            audio.release();
        }
        
        audio = null;
    }

    public synchronized static void write(int aRate, short aSamples[], int aCount)
    {
        // Check args
        if(null == aSamples || aCount < 0 || aCount >= aSamples.length)
        {
            throw new IllegalArgumentException("Invalid audio stream chunk.");
        }

        // Create audio if needed
        if((null == audio && !failed) || aRate != rate)
        {
            open(aRate);
        }
    
        // Write samples
        if(null != audio)
        {
        	audio.write(aSamples, 0, aCount);
        }
    }
}

