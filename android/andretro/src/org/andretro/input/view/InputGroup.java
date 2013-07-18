package org.andretro.input.view;
import org.andretro.emulator.*;

import android.util.*;
import android.view.*;
import android.content.*;
import android.widget.*;
import android.app.*;
import android.graphics.*;

import java.io.*;
import java.util.*;

import javax.xml.parsers.*;
import org.w3c.dom.*;

public class InputGroup extends RelativeLayout
{
	public static interface InputHandler
	{
	    int getBits(int aX, int aY);
	}

	private static class Handler
	{
		final Rect area = new Rect();
		final View view;
		
		public Handler(View aHandler)
		{
			if(!(aHandler instanceof InputHandler))
			{
				throw new RuntimeException("aHandler must be an object that implements the InputHandler interface.");
			}

			view = aHandler;
		}
		
		public int getBits(int aX, int aY)
		{
			final InputHandler handler = (InputHandler)view;
			
			area.left = view.getLeft();
			area.right = view.getRight();
			area.top = view.getTop();
			area.bottom = view.getBottom();
			return area.contains(aX, aY) ? handler.getBits(aX, aY) : 0;
		}
	}
	private ArrayList<Handler> handlers = new ArrayList<Handler>();
	
	private int currentBits;
	
    public InputGroup(Context aContext, AttributeSet aAttributes)
    {
        super(aContext, aAttributes);
        setFocusableInTouchMode(true);
    }

    @Override public boolean onTouchEvent(MotionEvent aEvent)
    {
    	currentBits = 0;
    	
        final int count = aEvent.getPointerCount();
        for(int i = 0; i != count; i ++)
        {
        	if(aEvent.getActionMasked() != MotionEvent.ACTION_UP || i != aEvent.getActionIndex())
        	{
        		for(Handler j: handlers)
	        	{
	        		currentBits |= j.getBits((int)aEvent.getX(i), (int)aEvent.getY(i));
	        	}
        	}
        }

        return true;
    }    
    
    public int getBits()
    {    
    	return currentBits;
    }
    
    public void removeChildren()
    {
    	final List<View> toRemove = new ArrayList<View>();
    	
    	for(int i = 0; i != getChildCount(); i ++)
    	{
    		final View thisOne = getChildAt(i);
    		if(thisOne instanceof InputHandler)
    		{
    			toRemove.add(thisOne);
    		}
    	}
    	
    	for(final View view: toRemove)
    	{
    		removeView(view);
    	}
    	
    	handlers.clear();
    }
    
    public void loadInputLayout(final Activity aContext, final ModuleInfo aModule, final InputStream aFile)
    {
    	try
    	{
	    	removeChildren();
	    	
	    	// Get the default from the module, if it's null read the default from aFile
	    	Element inputElement = aModule.getOnScreenInputDefinition();
	    	if(null == inputElement)
	    	{
	    		inputElement = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(aFile).getDocumentElement();
	    	}
	    	
	    	// Build the views
	    	NodeList inputs = inputElement.getChildNodes();
	    	for(int i = 0; i != inputs.getLength(); i ++)
	    	{
	    		if(Node.ELEMENT_NODE == inputs.item(i).getNodeType())
	    		{
		    		Element input = (Element)inputs.item(i);
		    							
					final Class<? extends View> clazz = Class.forName("org.andretro.input.view." + input.getNodeName()).asSubclass(View.class);
					View inputView = clazz.getConstructor(Context.class, Element.class).newInstance(aContext, input);
					addView(inputView, extractAnchorData(input));

					handlers.add(new Handler(inputView));
	    		}
	    	}
        }
        catch(final Exception e)
        {
        	aContext.runOnUiThread(new Runnable()
        	{
        		@Override public void run()
        		{
        			String message = e.getMessage();
        			message = (message == null) ? "No Message" : message;
                    Toast.makeText(aContext, "Failed to load controls from " + aFile + "\n" + e.getClass().toString() + "\n" + message, Toast.LENGTH_LONG).show();
        		}
        	});
        }    	
    }
    
    // Anchor mapping
	private RelativeLayout.LayoutParams extractAnchorData(Element aElement)
	{
		RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(getInt(aElement, "width"), getInt(aElement, "height"));
		
		final int horizontalAnchorMode = horizontalAnchor.get(aElement.getAttribute("horzanchor"));
		final int horizontalMargin = getInt(aElement, "horzmargin");		
		params.leftMargin = (horizontalAnchorMode == RelativeLayout.ALIGN_PARENT_LEFT) ? horizontalMargin : 0;;
		params.rightMargin = (horizontalAnchorMode == RelativeLayout.ALIGN_PARENT_RIGHT) ? horizontalMargin : 0;;
		params.addRule(horizontalAnchorMode);

		final int verticalAnchorMode = verticalAnchor.get(aElement.getAttribute("vertanchor"));
		final int verticalMargin = getInt(aElement, "vertmargin");
		params.topMargin = (verticalAnchorMode == RelativeLayout.ALIGN_PARENT_TOP) ? verticalMargin : 0;
		params.bottomMargin = (verticalAnchorMode == RelativeLayout.ALIGN_PARENT_BOTTOM) ? verticalMargin : 0;
		params.addRule(verticalAnchorMode);
		
		return params;
	}
	
    private static final Map<String, Integer> horizontalAnchor = new HashMap<String, Integer>();
    private static final Map<String, Integer> verticalAnchor = new HashMap<String, Integer>();
    
    static
    {
    	horizontalAnchor.put("left", RelativeLayout.ALIGN_PARENT_LEFT);
    	horizontalAnchor.put("center", RelativeLayout.CENTER_HORIZONTAL);
    	horizontalAnchor.put("right", RelativeLayout.ALIGN_PARENT_RIGHT);
    	verticalAnchor.put("top", RelativeLayout.ALIGN_PARENT_TOP);
    	verticalAnchor.put("center", RelativeLayout.CENTER_VERTICAL);
    	verticalAnchor.put("bottom", RelativeLayout.ALIGN_PARENT_BOTTOM);
    }

	public static int getInt(Element aElement, String aAttribute)
	{
		final String value = aElement.getAttribute(aAttribute);
		return ("".equals(value)) ? 0 : Integer.parseInt(value);        			
	}
}
