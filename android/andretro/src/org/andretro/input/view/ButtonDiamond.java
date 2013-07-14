package org.andretro.input.view;
import org.andretro.*;
import org.w3c.dom.*;

import android.annotation.*;
import android.content.*;
import android.widget.*;

@SuppressLint("ViewConstructor")
public class ButtonDiamond extends ImageView implements InputGroup.InputHandler
{
    private final int bits[] = new int[4];   

    public ButtonDiamond(Context aContext, Element aElement)
    {
    	super(aContext);
    	setImageResource(R.drawable.dpad);
    	
    	bits[0] = InputGroup.getInt(aElement, "upbits");
    	bits[1] = InputGroup.getInt(aElement, "downbits");
    	bits[2] = InputGroup.getInt(aElement, "leftbits");
    	bits[3] = InputGroup.getInt(aElement, "rightbits");
    }

    @Override public int getBits(int aX, int aY)
    {
    	final int w = getWidth() / 3;
    	final int h = getHeight() / 3;
    	
    	final int x = aX - getLeft();
    	final int y = aY - getTop();
    	
    	int result = 0;
    	result |= (x < w) ? bits[2] : 0;
    	result |= (x > w * 2) ? bits[3] : 0;
    	result |= (y < h) ? bits[0] : 0;
    	result |= (y > h * 2) ? bits[1] : 0;

    	return result;

    }
}
