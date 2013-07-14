package org.andretro.input.view;

import org.andretro.*;

import android.annotation.SuppressLint;
import android.content.*;
import android.widget.*;

import org.w3c.dom.*;

@SuppressLint("ViewConstructor")
public class Button extends ImageView implements InputGroup.InputHandler
{
    int touchCount = 0;
    int bits;

    public Button(Context aContext, Element aElement)
    {
        super(aContext);
    	setImageResource(R.drawable.button);
    	
        bits = InputGroup.getInt(aElement, "bits");
    }

    @Override public int getBits(int aX, int aY)
    {
        return bits;
    }
}
