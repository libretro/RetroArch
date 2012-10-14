package com.retroarch.fileio;

public class Option implements Comparable<Option>
{
     public String name;
     public String data;
     public String path;
     
     public Option(String n,String d,String p)
     {
         name = n;
         data = d;
         path = p;
     }
     
     public int compareTo(Option o)
     {
         if(this.name != null)
             return this.name.toLowerCase().compareTo(o.name.toLowerCase()); 
         else 
             throw new IllegalArgumentException();
     }
}
