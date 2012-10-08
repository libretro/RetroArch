package com.retroarch.fileio;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

import com.retroarch.R;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.SectionIndexer;
import android.widget.TextView;

public class FileArrayAdapter extends ArrayAdapter<Option> implements SectionIndexer 
{
     HashMap<String, Integer> alphaIndexer; 
     String[] sections;
     
     private Context c;
     private int id;
     private List<Option>items;
     
     public FileArrayAdapter(Context context, int textViewResourceId,
               List<Option> objects) {
          super(context, textViewResourceId, objects);
          c = context;
          id = textViewResourceId;
          items = objects;    
          
          initAlphaIndexer();
     }
     
     private void initAlphaIndexer()
     {
          alphaIndexer = new HashMap<String, Integer>();
          int size = items.size();

          for (int x = 0; x < size; x++) {
               Option o = items.get(x);

              String ch =  o.getName().substring(0, 1);
              
              ch = ch.toUpperCase();

              if (!alphaIndexer.containsKey(ch))
              {
                   alphaIndexer.put(ch, x);
              }
          }

          Set<String> sectionLetters = alphaIndexer.keySet();

          ArrayList<String> sectionList = new ArrayList<String>(sectionLetters); 

          Collections.sort(sectionList);

          sections = new String[sectionList.size()];

          sectionList.toArray(sections);          
     }

     public Option getItem(int i)
     {
          return items.get(i);
     }
     @Override
      public View getView(int position, View convertView, ViewGroup parent) {
              View v = convertView;
              if (v == null) {
                  LayoutInflater vi = (LayoutInflater)c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                  v = vi.inflate(id, null);
              }
              final Option o = items.get(position);
              if (o != null) {                   
                      TextView t1 = (TextView) v.findViewById(R.id.TextView01);
                      TextView t2 = (TextView) v.findViewById(R.id.TextView02);

                      if(t1!=null)
                      {
                        t1.setText(o.getName());
                      }
                      if(t2!=null)
                      {
                        t2.setText(o.getData());
                      }
                      
              }
              return v;
      }

     public int getPositionForSection(int section)
     {
    	 // FIXME
    	 if (section >= sections.length)
    	 {
    		 return 0;
    	 }
         return alphaIndexer.get(sections[section]);
     }

     public int getSectionForPosition(int position)
     {
          return 1;
     }

     public Object[] getSections()
     {          
          return sections;
     }
     
     
}
