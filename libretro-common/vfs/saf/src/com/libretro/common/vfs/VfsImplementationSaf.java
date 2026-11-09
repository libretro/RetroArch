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

   /**
    * Get the MIME type of a document, or null if it could not be queried.
    */
   private static String queryMimeType(ContentResolver content, Uri fileUri) {
      final Cursor cursor;
      try
      {
         cursor = content.query(fileUri, QUERY_ARGS_MIME_TYPE, null, null, null);
      }
      catch (IllegalArgumentException e)
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
    * Whether or not a document is named exactly name.
    * Every query is a Binder call into the document provider, so one is only made
    * if the name cannot be read from the end of a path-style document ID.
    */
   private static boolean hasName(ContentResolver content, Uri fileUri, String name) {
      final String id = DocumentsContract.getDocumentId(fileUri);
      final int separator = id.length() - name.length() - 1;

      if (separator >= 0 && id.endsWith(name) && (id.charAt(separator) == '/' || id.charAt(separator) == ':'))
         return true;

      return name.equals(getDisplayName(content, fileUri));
   }

   /**
    * Rename a document, returning its URI if it ended up named exactly name or null if not.
    * Some providers pick a different name (such as "name (1)") instead of failing when the name is taken,
    * so on a mismatch the document is renamed back to originalName.
    */
   private static Uri renameDocumentExact(ContentResolver content, Uri fileUri, String name, String originalName) {
      final Uri renamedUri;
      try
      {
         renamedUri = DocumentsContract.renameDocument(content, fileUri, name);
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return null;
      }

      // Returning null is how DocumentsContract reports a failure: a rename that keeps the ID returns fileUri
      if (renamedUri == null)
         return null;

      if (hasName(content, renamedUri, name))
         return renamedUri;

      try
      {
         DocumentsContract.renameDocument(content, renamedUri, originalName);
      }
      catch (FileNotFoundException | RuntimeException e)
      {}

      return null;
   }

   /**
    * Move a document to another directory, returning its URI if it ended up there named exactly name or null if not.
    * A null URI from DocumentsContract means the move failed.
    */
   private static Uri moveDocumentExact(ContentResolver content, Uri fileUri, Uri fromUri, Uri toUri, String name) {
      if (Build.VERSION.SDK_INT < 24)
         return null;

      final Uri movedUri;
      try
      {
         movedUri = DocumentsContract.moveDocument(content, fileUri, fromUri, toUri);
      }
      catch (FileNotFoundException | RuntimeException e)
      {
         return null;
      }

      if (movedUri == null)
         return null;

      if (hasName(content, movedUri, name))
         return movedUri;

      try
      {
         DocumentsContract.moveDocument(content, movedUri, toUri, fromUri);
      }
      catch (FileNotFoundException | RuntimeException e)
      {}

      return null;
   }

   /**
    * Move a document to another directory under a new name, returning whether or not it got there.
    * A provider can only move a document under the name it already has, so this takes a move and a rename.
    * The name is changed first, unless newName is taken in the source directory; on failure the document is put back.
    */
   private static boolean moveAndRenameDocument(ContentResolver content, Uri fileUri, Uri fromUri, Uri toUri, String oldName, String newName) {
      if (oldName.equals(newName))
         return moveDocumentExact(content, fileUri, fromUri, toUri, newName) != null;

      final Uri renamedUri = renameDocumentExact(content, fileUri, newName, oldName);
      if (renamedUri != null)
      {
         if (moveDocumentExact(content, renamedUri, fromUri, toUri, newName) != null)
            return true;

         renameDocumentExact(content, renamedUri, oldName, newName);
         return false;
      }

      final Uri movedUri = moveDocumentExact(content, fileUri, fromUri, toUri, oldName);
      if (movedUri == null)
         return false;

      if (renameDocumentExact(content, movedUri, newName, oldName) != null)
         return true;

      moveDocumentExact(content, movedUri, toUri, fromUri, oldName);
      return false;
   }

   /**
    * Rename a Storage Access Framework file, replacing the destination if it exists, returning whether or not the operation succeeded.
    * Moving between directories needs API level 24 and a provider that supports moves.
    * Storage Access Framework has no atomic replace, so an existing destination is first renamed to the destination path with ".old" appended,
    * and is only deleted once the file has been renamed into its place; if that fails, the destination is restored.
    * Each provider call below is a Binder transaction, by far the largest cost here: replacing a file takes four and creating one takes two, plus one for a move or two for a move under a new name in place of the rename.
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

      // Both paths are canonical now, so they can be taken apart without going back to the filesystem
      final int oldSlash = oldPath.lastIndexOf('/');
      final int newSlash = newPath.lastIndexOf('/');

      if (oldSlash + 1 == oldPath.length() || newSlash + 1 == newPath.length())
         return false;

      final boolean sameDirectory = oldSlash == newSlash && oldPath.regionMatches(0, newPath, 0, oldSlash);

      if (!sameDirectory && Build.VERSION.SDK_INT < 24)
         return false;

      final String oldName = oldPath.substring(oldSlash + 1);
      final String newName = newPath.substring(newSlash + 1);
      final Uri newUri = getDocumentUri(treeUri, newPath);
      String asideName = null;
      Uri asideUri = null;

      final String newMimeType = queryMimeType(content, newUri);
      if (newMimeType != null)
      {
         if (newMimeType.equals(Document.MIME_TYPE_DIR))
            return false;

         // The source may itself be called "<destination>.old", and must not be mistaken for a leftover
         asideName = newPath.concat(".old").equals(oldPath) ? newName.concat(".old2") : newName.concat(".old");

         asideUri = renameDocumentExact(content, newUri, asideName, newName);
         if (asideUri == null)
         {
            // Most likely a leftover from an earlier rename that could not clean up, which is rare enough to only look for now.
            // Deleting a directory would delete everything in it, so anything but a file stays where it is.
            final Uri leftoverUri = getDocumentUri(treeUri, newPath.substring(0, newSlash + 1).concat(asideName));
            final String leftoverMimeType = queryMimeType(content, leftoverUri);

            if (leftoverMimeType == null || leftoverMimeType.equals(Document.MIME_TYPE_DIR))
               return false;

            try
            {
               DocumentsContract.deleteDocument(content, leftoverUri);
            }
            catch (FileNotFoundException | RuntimeException e)
            {
               return false;
            }

            asideUri = renameDocumentExact(content, newUri, asideName, newName);
            if (asideUri == null)
               return false;
         }
      }

      final Uri oldUri = getDocumentUri(treeUri, oldPath);
      final boolean placed;

      if (sameDirectory)
         placed = renameDocumentExact(content, oldUri, newName, oldName) != null;
      else
         placed = moveAndRenameDocument(content, oldUri,
               getDocumentUri(treeUri, oldSlash == 0 ? "/" : oldPath.substring(0, oldSlash)),
               getDocumentUri(treeUri, newSlash == 0 ? "/" : newPath.substring(0, newSlash)),
               oldName, newName);

      if (placed)
      {
         if (asideUri != null)
         {
            // The file is in place; a failed cleanup only leaves a stale ".old" for the next rename to remove
            try
            {
               DocumentsContract.deleteDocument(content, asideUri);
            }
            catch (FileNotFoundException | RuntimeException e)
            {}
         }
         return true;
      }

      if (asideUri != null)
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
