package com.retroarch.fileio;

import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Stack;

import com.retroarch.R;
import com.retroarch.main;
import com.retroarch.R.layout;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Toast;

public class FileChooser extends Activity
{
     private static final String LOG_TAG = "FileChooser";
     
     public static final String EXTRA_START_DIR = "StartDir";
     public static final String EXTRA_EXTENSIONS = "Extensions";
     public static final String EXTRA_TEMP_DIR = "TempDir";
     public static final String EXTRA_SELECT_DIR = "SelectDir";
     public static final String PAYLOAD_FILENAME = "Filename";
     public static final String PAYLOAD_SELECTED_DIR = "DirSelected";
     
     private File currentDir;
     private FileArrayAdapter adapter;
     
     private String _startDir;
     private String _extensions;
     private String _tempDir;
     private String _selectDir;
     
     private Stack<String> _dirStack;
     
     private ListView _view;
     
     @Override
     public void onCreate(Bundle savedInstanceState)
     {
         super.onCreate(savedInstanceState);
                           
         setContentView(R.layout.file_choose);
         _view = (ListView)findViewById(R.id.fileChooserView);
         
         _startDir = getIntent().getStringExtra(EXTRA_START_DIR);
         _extensions = getIntent().getStringExtra(EXTRA_EXTENSIONS);
         _tempDir = getIntent().getStringExtra(EXTRA_TEMP_DIR);
         _selectDir = getIntent().getStringExtra(EXTRA_SELECT_DIR);
         
         // clamp start dir and remove the /
         if (_startDir == null)
         {
              _startDir = Environment.getExternalStorageDirectory().getAbsolutePath();
         }
         else if (_startDir.endsWith("/"))
         {
              _startDir = _startDir.substring(0, _startDir.length() - 1);
         }
         
         // push the start dir onto the stack
         _dirStack = new Stack<String>();
         _dirStack.push(_startDir);
         
         // clamp extentions to all or extra specified
         if (_extensions == null)
         {
              _extensions = ".*";
         }
         
         // clamp temp dir
         if (_tempDir == null)
         {
              _tempDir = Environment.getExternalStorageDirectory().getAbsolutePath();
         }
         
         // regular filesystem dir
         currentDir = new File(_startDir);
         fill(currentDir);    
         
         // are we dir selecting?, add button
         if (_selectDir == null)
         {
              // hide button
              Button b = (Button)findViewById(R.id.fileChooserSubmit);
              b.setVisibility(Button.GONE);
         }
         else
         {
              // add on click submit
              Button b = (Button)findViewById(R.id.fileChooserSubmit);
              b.setOnClickListener(new OnClickListener()
              {
                    
                   public void onClick(View v)
                   {
                        String s = _dirStack.peek();
                        Toast.makeText(FileChooser.this, "Selected: " + s, Toast.LENGTH_SHORT).show();
                        
                        Intent intent=new Intent();   
                        intent.putExtra(FileChooser.PAYLOAD_SELECTED_DIR, s);
                        setResult(RESULT_OK, intent);
                        
                        finish();                       
                   }
              });
         }
     }
     
     public boolean onCreateOptionsMenu(Menu menu)
     {
     	MenuInflater inflater = getMenuInflater();
     	inflater.inflate(R.menu.main_menu, menu);
     	return true;
     }
     
     public boolean onOptionsItemSelected(MenuItem item)
     {
     	switch (item.getItemId())
     	{
     	    case R.id.main:
     	    	this.finish();
     		    break;
     	    case R.id.open:
     	    	//TODO: Return to root dir
     		    break;
     		default:
     	    	Toast.makeText(this, "MenuItem " + item.getTitle() + " selected.", Toast.LENGTH_SHORT).show();
     	    	break;
     	}
     	
     	return true;
     }
     
     
     private void fill(File f)
     {
         File[]dirs = f.listFiles();
          this.setTitle("Path: "+f.getAbsolutePath());
          List<Option>dir = new ArrayList<Option>();
          List<Option>fls = new ArrayList<Option>();
          try
          {
              for(File ff: dirs)
              {
                 if(ff.isDirectory())
                 {
                     dir.add(new Option(ff.getName() + "/","Folder",ff.getAbsolutePath()));
                 }
                 else
                 {
                      int dotIndex = ff.getName().lastIndexOf('.');
                      if (dotIndex > 0)
                      {
                           String extension = ff.getName().substring(dotIndex+1).toLowerCase();
                           if (extension.matches(_extensions))
                           {
                                fls.add(new Option(ff.getName(),"File Size: "+ff.length(),ff.getAbsolutePath()));
                           }
                      }
                 }
              }
          }
          catch(Exception e)
          {
              
          }
          
          Collections.sort(dir);
          Collections.sort(fls);
          dir.addAll(fls);
          
	  dir.add(0,new Option("..", "Parent Directory", f.getParent()));

          adapter = new FileArrayAdapter(FileChooser.this,R.layout.file_view,dir);
          _view.setAdapter(adapter);
          
          _view.setOnItemClickListener(new OnItemClickListener()
          {

               public void onItemClick(AdapterView<?> arg0, View arg1,
                         int arg2, long arg3)
               {
                    int pos = arg0.getPositionForView(arg1);
                    Option o = adapter.getItem(pos);
                    if(o.getData().equalsIgnoreCase("folder")||o.getData().equalsIgnoreCase("parent directory"))
                    {
                         if (o.getPath() != null)
                         {
                            currentDir = new File(o.getPath());
                            fill(currentDir);
                            _dirStack.push(o.getPath());
                         }
                    }
                    else
                    {
                         String path = o.getPath();
                         int dotIndex = path.lastIndexOf('.');
                         String ext = null;
                         if (dotIndex >= 0)
                         {
                              ext = path.substring(dotIndex+1).toLowerCase();
                         }
                         
			 onFileClick(o);
                    }
               }
               
          });
     }
     
     
     private void onFileClick(Option o)
     {
         Toast.makeText(this, "Loading: "+o.getName() + "...", Toast.LENGTH_SHORT).show();
         
         Intent intent=new Intent();   
         intent.putExtra(FileChooser.PAYLOAD_FILENAME, o.getPath());
         setResult(RESULT_OK, intent);
         
         finish();         
     }
     
     @Override
     public boolean onKeyDown(int keyCode, KeyEvent event) {
         if (keyCode == KeyEvent.KEYCODE_BACK)
         {
              if (_dirStack.size() > 1)
              {
            	  _dirStack.pop();
            	  File file = new File(_dirStack.peek());
            	  
                  currentDir = file;
                  fill(currentDir);                   
              }
              else
              {
                   Intent intent = new Intent();
                   setResult(RESULT_OK, intent);
                   
                   finish();
              }
              
             return true;
         }
         return super.onKeyDown(keyCode, event);
     }
 }
