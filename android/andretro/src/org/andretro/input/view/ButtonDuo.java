package org.andretro.input.view;
import org.andretro.*;
import org.w3c.dom.*;

import android.annotation.*;
import android.content.*;
import android.widget.*;

@SuppressLint("ViewConstructor")
public class ButtonDuo extends ImageView implements InputGroup.InputHandler
{
    int currentBits = 0;
    int bits[] = new int[2];

    public ButtonDuo(Context aContext, Element aElement)
    {
        super(aContext);
        
        setImageResource(R.drawable.buttonduo);

        bits[0] = InputGroup.getInt(aElement, "leftbits");
        bits[1] = InputGroup.getInt(aElement, "rightbits");
    }
        
    @Override public int getBits(int aX, int aY)
    {
        final int x = aX - getLeft();
        final int width = getWidth() / 3;
        
        int result = (x < width * 2) ? bits[0] : 0;
        result |= (x > width) ? bits[1] : 0;

        return result;
    }
}
