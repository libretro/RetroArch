/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (VfsImplementationSaf.java).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

package com.libretro.common.vfs;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.Closeable;
import java.io.FileNotFoundException;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.nio.file.Paths;

public final class VfsImplementationSaf
{
   private static final String[] QUERY_ARGS_MIME_TYPE = {
      Document.COLUMN_MIME_TYPE,
   };

   /**
    * Open a Storage Access Framework file, returning its file descriptor if successful or -1 if not.
    * The file is not guaranteed to be seeked to any particular position, so it may be a good idea to seek it immediately after opening.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the file to open, relative to the root directory of the tree
    * @param read whether or not to open the file with read permissions
    * @param write whether or not to open the file with write permissions
    * @param truncate if the file is opened with write permissions, whether or not to delete the contents of the file after opening it
    */
   public static int openSafFile(ContentResolver content, String tree, String path, boolean read, boolean write, boolean truncate)
   {
      if (Build.VERSION.SDK_INT < 21)
         return -1;
      boolean createdFile = false;
      while (true)
      {
         final Uri treeUri = Uri.parse(tree);
         final Path filePath;
         try
         {
            filePath = Paths.get("/" + path).normalize();
         }
         catch (InvalidPathException e)
         {
            return -1;
         }
         final String filePathString = filePath.toString();
         final Uri fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, filePathString.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + filePathString);
         final String mode;
         if (!write)
            mode = "r";
         else if (!read)
         {
            if (truncate)
               mode = "wt";
            else
               mode = "wa";
         }
         else
         {
            if (truncate)
               mode = "rwt";
            else
               mode = "rw";
         }
         try
         {
            return content.openFileDescriptor(fileUri, mode).detachFd();
         }
         catch (FileNotFoundException | IllegalArgumentException e)
         {
            if (createdFile || !write)
               return -1;
            createdFile = true;
            final Path parentPath = filePath.getParent();
            if (parentPath == null)
               return -1;
            final String parentPathString = parentPath.toString();
            final Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentPathString.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + parentPathString);
            try
            {
               DocumentsContract.createDocument(content, parentUri, "application/octet-stream", filePath.getFileName().toString());
            }
            catch (FileNotFoundException | IllegalArgumentException f)
            {
               return -1;
            }
         }
      }
   }

   /**
    * Delete a Storage Access Framework file or directory, returning whether or not the operation succeeded.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the file or directory to delete, relative to the root directory of the tree
    */
   public static boolean removeSafFile(ContentResolver content, String tree, String path)
   {
      if (Build.VERSION.SDK_INT < 21)
         return false;
      final Uri treeUri = Uri.parse(tree);
      try
      {
         path = Paths.get("/" + path).normalize().toString();
      }
      catch (InvalidPathException e)
      {
         return false;
      }
      path = path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path;
      final Uri fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, path);
      try
      {
         DocumentsContract.deleteDocument(content, fileUri);
      }
      catch (FileNotFoundException | IllegalArgumentException e)
      {
         return false;
      }
      return true;
   }

   public static class SafStat
   {
      private static final String[] QUERY_ARGS_WITH_SIZE = {
         Document.COLUMN_MIME_TYPE,
         Document.COLUMN_SIZE,
      };

      private static final String[] QUERY_ARGS_WITHOUT_SIZE = {
         Document.COLUMN_MIME_TYPE,
      };

      private boolean isOpen = false;
      private boolean isDirectory = false;
      private long size = -1;

      /**
       * Retrieves metadata about a Storage Access Framework file or directory.
       * @param content the content resolver returned by getContentResolver()
       * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
       * @param path path of the file or directory to open, relative to the root directory of the tree
       * @param includeSize whether or not to query the size of the file or directory
       */
      public SafStat(ContentResolver content, String tree, String path, boolean includeSize)
      {
         if (Build.VERSION.SDK_INT < 21)
            return;
         final Uri treeUri = Uri.parse(tree);
         try
         {
            path = Paths.get("/" + path).normalize().toString();
         }
         catch (InvalidPathException e)
         {
            return;
         }
         path = path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path;
         final Uri fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, path);
         final Cursor cursor;
         try
         {
            cursor = content.query(fileUri, includeSize ? QUERY_ARGS_WITH_SIZE : QUERY_ARGS_WITHOUT_SIZE, null, null, null);
         }
         catch (IllegalArgumentException e)
         {
            return;
         }
         if (cursor == null)
            return;
         try
         {
            if (!cursor.moveToNext())
               return;
            isDirectory = cursor.getString(0).equals(Document.MIME_TYPE_DIR);
            size = includeSize ? cursor.getLong(1) : 0;
            isOpen = true;
         }
         finally
         {
            cursor.close();
         }
      }

      /**
       * Get whether or not the file or directory was able to be queried.
       */
      public boolean getIsOpen()
      {
         return isOpen;
      }

      /**
       * Get whether or not the file or directory is a directory, or false if there was an error opening the file or directory.
       */
      public boolean getIsDirectory()
      {
         return isOpen ? isDirectory : false;
      }

      /**
       * Get the size of the file or directory, or -1 if there was an error opening the file or directory.
       * If this object was initialized without includeSize, the size will be 0 if the file or directory was opened successfully or -1 if not.
       */
      public long getSize()
      {
         return isOpen ? size : -1;
      }
   }

   /**
    * Create a Storage Access Framework directory, returning 0 if it succeeded, -1 if it failed or -2 if the directory already exists.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param path path of the directory to create, relative to the root directory of the tree
    */
   public static int mkdirSaf(ContentResolver content, String tree, String path)
   {
      if (Build.VERSION.SDK_INT < 21)
         return -1;
      final Uri treeUri = Uri.parse(tree);
      final Path directoryPath;
      try
      {
         directoryPath = Paths.get("/" + path).normalize();
      }
      catch (InvalidPathException e)
      {
         return -1;
      }
      path = path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path;
      final Uri directoryUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, path);
      Cursor cursor = null;
      try
      {
         cursor = content.query(directoryUri, QUERY_ARGS_MIME_TYPE, null, null, null);
      }
      catch (IllegalArgumentException e)
      {}
      if (cursor != null)
      {
         try
         {
            if (cursor.moveToNext())
               return cursor.getString(0).equals(Document.MIME_TYPE_DIR) ? -2 : -1;
         }
         finally
         {
            cursor.close();
         }
      }
      final Path parentPath = directoryPath.getParent();
      if (parentPath == null)
         return -1;
      final String parentPathString = parentPath.toString();
      final Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentPathString.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + parentPathString);
      try
      {
         if (DocumentsContract.createDocument(content, parentUri, Document.MIME_TYPE_DIR, directoryPath.getFileName().toString()) == null)
            return -1;
      }
      catch (FileNotFoundException | IllegalArgumentException e)
      {
         return -1;
      }
      return 0;
   }

   public static class SafDirectory implements Closeable
   {
      private static final String[] QUERY_ARGS = {
         Document.COLUMN_DOCUMENT_ID,
         Document.COLUMN_MIME_TYPE,
      };

      private int prefixLength;
      private Cursor cursor = null;
      private String direntName = null;
      private boolean direntIsDirectory = false;

      /**
       * Open a Storage Access Framework directory to list its contents.
       * @param content the content resolver returned by getContentResolver()
       * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
       * @param path path of the directory to open, relative to the root directory of the tree
       */
      public SafDirectory(ContentResolver content, String tree, String path)
      {
         if (Build.VERSION.SDK_INT < 21)
            return;
         final Uri treeUri = Uri.parse(tree);
         try
         {
            path = Paths.get("/" + path).normalize().toString();
         }
         catch (InvalidPathException e)
         {
            return;
         }
         path = path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path;
         prefixLength = path.length();
         final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, path);
         try
         {
            cursor = content.query(childrenUri, QUERY_ARGS, null, null, null);
         }
         catch (IllegalArgumentException e)
         {}
      }

      @Override
      public void close()
      {
         if (cursor != null)
         {
            cursor.close();
            cursor = null;
         }
      }

      /**
       * Get the next child (could be a file or directory) of this directory, returning true if successful or false if there are no more children.
       */
      public boolean readdir()
      {
         if (Build.VERSION.SDK_INT < 21)
            return false;
         if (cursor == null)
            return false;
         if (cursor.moveToNext())
         {
            direntName = cursor.getString(0).substring(prefixLength);
            // Remove leading slashes
            int slashIndex = 0;
            while (slashIndex < direntName.length() && direntName.charAt(slashIndex) == '/')
               ++slashIndex;
            direntName = direntName.substring(slashIndex);
            // Remove trailing slashes
            slashIndex = direntName.length();
            while (slashIndex > 0 && direntName.charAt(slashIndex - 1) == '/')
               --slashIndex;
            direntName = direntName.substring(0, slashIndex);
            direntIsDirectory = cursor.getString(1).equals(Document.MIME_TYPE_DIR);
            return true;
         }
         else
         {
            direntName = null;
            direntIsDirectory = false;
            close();
            return false;
         }
      }

      /**
       * Return the name of the child that was most recently retrieved by readdir(), or null if readdir() hasn't been called yet or readdir() ran out of children.
       */
      public String getDirentName()
      {
         return direntName;
      }

      /**
       * Return whether or not the child that was most recently retrieved by readdir() is a directory, or false if readdir() hasn't been called yet or readdir() ran out of children.
       */
      public boolean getDirentIsDirectory()
      {
         return direntIsDirectory;
      }
   }
}
