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
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;

public final class VfsImplementationSaf
{
   private static final String[] QUERY_ARGS_MIME_TYPE = {
      Document.COLUMN_MIME_TYPE,
   };

   private static final String[] QUERY_ARGS_DISPLAY_NAME = {
      Document.COLUMN_DISPLAY_NAME,
   };

   private static String normalizePath(String path) {
      try
      {
         return new File("/" + path).getCanonicalPath();
      }
      catch (IOException e)
      {
         return "/";
      }
   }

   private static String getPathParent(String path) {
      try
      {
         return new File("/" + path).getCanonicalFile().getParent();
      }
      catch (IOException e)
      {
         return null;
      }
   }

   private static String getPathFileName(String path) {
      try
      {
         return new File("/" + path).getCanonicalFile().getName();
      }
      catch (IOException e)
      {
         return null;
      }
   }

   private static boolean isDocument(Uri treeUri) {
      final List<String> segments = treeUri.getPathSegments();
      return (
         (
            segments.size() >= 2
               && segments.get(0).equals("document")
         ) || (
            segments.size() >= 4
               && segments.get(0).equals("tree")
               && segments.get(2).equals("document")
         )
      );
   }

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
      path = normalizePath(path);
      boolean createdFile = false;
      while (true)
      {
         final Uri treeUri = Uri.parse(tree);
         if (isDocument(treeUri) && !path.equals("/"))
            return -1;
         final Uri fileUri = isDocument(treeUri)
            ? treeUri
            : DocumentsContract.buildDocumentUriUsingTree(treeUri, path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path);
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
            if (createdFile || !write || !truncate || isDocument(treeUri))
               return -1;
            createdFile = true;
            final String parentPath = getPathParent(path);
            if (parentPath == null)
               return -1;
            final Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentPath.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + parentPath);
            try
            {
               DocumentsContract.createDocument(content, parentUri, "application/octet-stream", getPathFileName(path));
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
      path = normalizePath(path);
      if (isDocument(treeUri) && !path.equals("/"))
         return false;
      final Uri fileUri = isDocument(treeUri)
         ? treeUri
         : DocumentsContract.buildDocumentUriUsingTree(treeUri, path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path);
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

   private static Uri getDocumentUri(Uri treeUri, String path) {
      return DocumentsContract.buildDocumentUriUsingTree(treeUri, path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path);
   }

   private static String getDisplayName(ContentResolver content, Uri fileUri) {
      final Cursor cursor;
      try
      {
         cursor = content.query(fileUri, QUERY_ARGS_DISPLAY_NAME, null, null, null);
      }
      catch (RuntimeException e)
      {
         return null;
      }

      if (cursor == null)
         return null;
      
      try
      {
         return cursor.moveToNext() ? cursor.getString(0) : null;
      }
      finally
      {
         cursor.close();
      }
   }

   /**
    * Rename a document, returning whether or not it ended up named exactly name.
    * Some providers pick a different name (such as "name (1)") instead of failing when the name is taken,
    * so on a mismatch the document is renamed back to originalName.
    */
   private static boolean renameDocumentExact(ContentResolver content, Uri fileUri, String name, String originalName) {
      Uri renamedUri;
      try
      {
         renamedUri = DocumentsContract.renameDocument(content, fileUri, name);
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return false;
      }

      // null means the document was renamed without its ID changing
      if (renamedUri == null)
         renamedUri = fileUri;

      if (name.equals(getDisplayName(content, renamedUri)))
         return true;

      try
      {
         DocumentsContract.renameDocument(content, renamedUri, originalName);
      }
      catch (FileNotFoundException | RuntimeException e)
      {}
      
      return false;
   }

   /**
    * Rename a Storage Access Framework file, replacing the destination if it exists, returning whether or not the operation succeeded.
    * Both paths must be in the same directory.
    * Storage Access Framework has no atomic replace, so an existing destination is first renamed to the destination path with ".old" appended,
    * and is only deleted once the file has been renamed into its place; if that fails, the destination is restored.
    * @param content the content resolver returned by getContentResolver()
    * @param tree the URI returned by the ACTION_OPEN_DOCUMENT_TREE intent action
    * @param oldPath path of the file to rename, relative to the root directory of the tree
    * @param newPath path to rename the file to, relative to the root directory of the tree
    */
   public static boolean renameSafFile(ContentResolver content, String tree, String oldPath, String newPath)
   {
      if (Build.VERSION.SDK_INT < 21)
         return false;
      
      final Uri treeUri = Uri.parse(tree);

      if (isDocument(treeUri))
         return false;
      
      oldPath = normalizePath(oldPath);
      newPath = normalizePath(newPath);
      
      if (oldPath.equals(newPath))
         return new SafStat(content, tree, oldPath, false).getIsOpen();

      final String parentPath = getPathParent(oldPath);
      final String oldName = getPathFileName(oldPath);
      final String newName = getPathFileName(newPath);

      if (parentPath == null || oldName == null || newName == null || !parentPath.equals(getPathParent(newPath)))
         return false;

      final String asidePath = newPath + ".old";
      final String asideName = newName + ".old";
      final Uri newUri = getDocumentUri(treeUri, newPath);
      final Uri asideUri = getDocumentUri(treeUri, asidePath);
      boolean movedAside = false;

      final SafStat newStat = new SafStat(content, tree, newPath, false);
      if (newStat.getIsOpen())
      {
         if (newStat.getIsDirectory())
            return false;
         // A leftover from an earlier rename that could not clean up
         if (new SafStat(content, tree, asidePath, false).getIsOpen())
            removeSafFile(content, tree, asidePath);

         if (!renameDocumentExact(content, newUri, asideName, newName))
            return false;
         
         movedAside = true;
      }

      if (renameDocumentExact(content, getDocumentUri(treeUri, oldPath), newName, oldName))
      {
         if (movedAside)
         {
            // The file is in place; a failed cleanup only leaves a stale ".old" for the next rename to remove
            try
            {
               removeSafFile(content, tree, asidePath);
            }
            catch (RuntimeException e)
            {}
         }
         return true;
      }

      if (movedAside)
         renameDocumentExact(content, asideUri, newName, asideName);

      return false;
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
         path = normalizePath(path);
         if (isDocument(treeUri) && !path.equals("/"))
            return;
         final Uri fileUri = isDocument(treeUri)
            ? treeUri
            : DocumentsContract.buildDocumentUriUsingTree(treeUri, path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path);
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
      path = normalizePath(path);
      if (isDocument(treeUri))
         return -1;
      final Uri directoryUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, path.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + path);
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
      final String parentPath = getPathParent(path);
      if (parentPath == null)
         return -1;
      final Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentPath.length() == 1 ? DocumentsContract.getTreeDocumentId(treeUri) : DocumentsContract.getTreeDocumentId(treeUri) + parentPath);
      try
      {
         if (DocumentsContract.createDocument(content, parentUri, Document.MIME_TYPE_DIR, getPathFileName(path)) == null)
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
      private boolean[] batchIsDir = new boolean[0];

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
         path = normalizePath(path);
         if (isDocument(treeUri))
            return;
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
       * Drain up to max children into arrays, so the native side can walk a
       * whole batch without a JNI round trip per entry. Returns the names;
       * getBatchIsDirectory() returns the parallel directory flags for the
       * same batch. A zero-length result means the directory is exhausted.
       *
       * readdir()/getDirentName()/getDirentIsDirectory() remain for callers
       * that step one entry at a time.
       */
      public String[] readdirBatch(int max)
      {
         int n = 0;
         String[] names;

         if (max <= 0)
            max = 1;

         names        = new String[max];
         batchIsDir   = new boolean[max];

         while (n < max && readdir())
         {
            names[n]      = direntName;
            batchIsDir[n] = direntIsDirectory;
            ++n;
         }

         if (n < max)
         {
            String[] shrunkNames  = new String[n];
            boolean[] shrunkFlags = new boolean[n];
            System.arraycopy(names, 0, shrunkNames, 0, n);
            System.arraycopy(batchIsDir, 0, shrunkFlags, 0, n);
            names      = shrunkNames;
            batchIsDir = shrunkFlags;
         }

         return names;
      }

      /**
       * Directory flags for the batch returned by the last readdirBatch().
       */
      public boolean[] getBatchIsDirectory()
      {
         return batchIsDir;
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
