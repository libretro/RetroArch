// Contributed by Connor McLaughlin (Stenzek)

package com.retroarch.browser.filehelper;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.ImageDecoder;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.os.storage.StorageManager;
import android.provider.DocumentsContract;
import android.provider.MediaStore;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.lang.reflect.Array;
import java.lang.reflect.Method;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * File helper class - used to bridge native code to Java storage access framework APIs.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class FileHelper {
    /**
     * Native filesystem flags.
     */
    public static final int FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY = 1;
    public static final int FILESYSTEM_FILE_ATTRIBUTE_READ_ONLY = 2;
    public static final int FILESYSTEM_FILE_ATTRIBUTE_COMPRESSED = 4;

    /**
     * Native filesystem find result flags.
     */
    public static final int FILESYSTEM_FIND_RECURSIVE = (1 << 0);
    public static final int FILESYSTEM_FIND_RELATIVE_PATHS = (1 << 1);
    public static final int FILESYSTEM_FIND_HIDDEN_FILES = (1 << 2);
    public static final int FILESYSTEM_FIND_FOLDERS = (1 << 3);
    public static final int FILESYSTEM_FIND_FILES = (1 << 4);
    public static final int FILESYSTEM_FIND_KEEP_ARRAY = (1 << 5);

    /**
     * Projection used when searching for files.
     */
    private static final String[] findProjection = new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED
    };

    /**
     * Projection used when statting files.
     */
    private static final String[] statProjection = new String[]{
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED
    };

    /**
     * Projection used when getting the display name for a file.
     */
    private static final String[] getDisplayNameProjection = new String[]{
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
    };

    /**
     * Projection used when getting a relative file for a file.
     */
    private static final String[] getRelativeFileProjection = new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
    };

    private final Context context;
    private final ContentResolver contentResolver;

    /**
     * File helper class - used to bridge native code to Java storage access framework APIs.
     *
     * @param context Context in which to perform file actions as.
     */
    public FileHelper(Context context) {
        this.context = context;
        this.contentResolver = context.getContentResolver();
    }

    /**
     * Reads the specified file as a string, under the specified context.
     *
     * @param context context to access file under
     * @param uri     uri to write data to
     * @param maxSize maximum file size to read
     * @return String containing the file data, otherwise null
     */
    public static String readStringFromUri(final Context context, final Uri uri, int maxSize) {
        InputStream stream = null;
        try {
            stream = context.getContentResolver().openInputStream(uri);
        } catch (FileNotFoundException e) {
            return null;
        }

        StringBuilder os = new StringBuilder();
        try {
            char[] buffer = new char[1024];
            InputStreamReader reader = new InputStreamReader(stream, Charset.forName(StandardCharsets.UTF_8.name()));
            int len;
            while ((len = reader.read(buffer)) > 0) {
                os.append(buffer, 0, len);
                if (os.length() > maxSize)
                    return null;
            }

            stream.close();
        } catch (IOException e) {
            return null;
        }

        if (os.length() == 0)
            return null;

        return os.toString();
    }

    /**
     * Reads the specified file as a byte array, under the specified context.
     *
     * @param context context to access file under
     * @param uri     uri to write data to
     * @param maxSize maximum file size to read
     * @return byte array containing the file data, otherwise null
     */
    public static byte[] readBytesFromUri(final Context context, final Uri uri, int maxSize) {
        InputStream stream = null;
        try {
            stream = context.getContentResolver().openInputStream(uri);
        } catch (FileNotFoundException e) {
            return null;
        }

        ByteArrayOutputStream os = new ByteArrayOutputStream();
        try {
            byte[] buffer = new byte[512 * 1024];
            int len;
            while ((len = stream.read(buffer)) > 0) {
                os.write(buffer, 0, len);
                if (maxSize > 0 && os.size() > maxSize) {
                    return null;
                }
            }

            stream.close();
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }

        if (os.size() == 0)
            return null;

        return os.toByteArray();
    }

    /**
     * Writes the specified data to a file referenced by the URI, as the specified context.
     *
     * @param context context to access file under
     * @param uri     uri to write data to
     * @param bytes   data to write file to
     * @return true if write was succesful, otherwise false
     */
    public static boolean writeBytesToUri(final Context context, final Uri uri, final byte[] bytes) {
        OutputStream stream = null;
        try {
            stream = context.getContentResolver().openOutputStream(uri);
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return false;
        }

        if (bytes != null && bytes.length > 0) {
            try {
                stream.write(bytes);
                stream.close();
            } catch (IOException e) {
                e.printStackTrace();
                return false;
            }
        }

        return true;
    }

    /**
     * Deletes the file referenced by the URI, under the specified context.
     *
     * @param context context to delete file under
     * @param uri     uri to delete
     * @return
     */
    public static boolean deleteFileAtUri(final Context context, final Uri uri) {
        try {
            if (uri.getScheme() == "file") {
                final File file = new File(uri.getPath());
                if (!file.isFile())
                    return false;

                return file.delete();
            }
            return (context.getContentResolver().delete(uri, null, null) > 0);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Returns the name of the file pointed at by a SAF URI.
     *
     * @param context context to access file under
     * @param uri     uri to retrieve file name for
     * @return the name of the file, or null
     */
    public static String getDocumentNameFromUri(final Context context, final Uri uri) {
        Cursor cursor = null;
        try {
            final String[] proj = {DocumentsContract.Document.COLUMN_DISPLAY_NAME};
            cursor = context.getContentResolver().query(uri, proj, null, null, null);
            final int columnIndex = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
            cursor.moveToFirst();
            return cursor.getString(columnIndex);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        } finally {
            if (cursor != null)
                cursor.close();
        }
    }

    /**
     * Loads a bitmap from the provided SAF URI.
     *
     * @param context context to access file under
     * @param uri     uri to retrieve file name for
     * @return a decoded bitmap for the file, or null
     */
    public static Bitmap loadBitmapFromUri(final Context context, final Uri uri) {
        InputStream stream = null;
        try {
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P) {
                final ImageDecoder.Source source = ImageDecoder.createSource(context.getContentResolver(), uri);
                return ImageDecoder.decodeBitmap(source);
            } else {
                return MediaStore.Images.Media.getBitmap(context.getContentResolver(), uri);
            }
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Returns the file name component of a path or URI.
     *
     * @param path Path/URI to examine.
     * @return File name component of path/URI.
     */
    public static String getFileNameForPath(String path) {
        if (path.startsWith("content:/") || path.startsWith("file:/")) {
            try {
                final Uri uri = Uri.parse(path);
                final String lastPathSegment = uri.getLastPathSegment();
                if (lastPathSegment != null)
                    path = lastPathSegment;
            } catch (Exception e) {
            }
        }

        int lastSlash = path.lastIndexOf('/');
        if (lastSlash > 0 && lastSlash < path.length() - 1)
            return path.substring(lastSlash + 1);
        else
            return path;
    }

    /**
     * Test if the given URI represents a {@link DocumentsContract.Document} tree.
     */
    public static boolean isTreeUri(Uri uri) {
        final List<String> paths = uri.getPathSegments();
        return (paths.size() >= 2 && paths.get(0).equals("tree"));
    }

    /**
     * Retrieves a file descriptor for a content URI string. Called by native code.
     *
     * @param uriString string of the URI to open
     * @param mode      Java open mode
     * @return file descriptor for URI, or -1
     */
    public int openURIAsFileDescriptor(String uriString, String mode) {
        try {
            final Uri uri = Uri.parse(uriString);
            final ParcelFileDescriptor fd = contentResolver.openFileDescriptor(uri, mode);
            if (fd == null)
                return -1;
            return fd.detachFd();
        } catch (Exception e) {
            return -1;
        }
    }

    /**
     * Recursively iterates documents in the specified tree, searching for files.
     *
     * @param treeUri    Root tree in which to search for documents.
     * @param documentId Document ID representing the directory to start searching.
     * @param flags      Native search flags.
     * @param results    Cumulative result array.
     */
    private void doFindFiles(Uri treeUri, String documentId, int flags, ArrayList<FindResult> results) {
        try {
            final Uri queryUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId);
            final Cursor cursor = contentResolver.query(queryUri, findProjection, null, null, null);
            final int count = cursor.getCount();

            while (cursor.moveToNext()) {
                try {
                    final String mimeType = cursor.getString(2);
                    final String childDocumentId = cursor.getString(0);
                    final Uri uri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocumentId);
                    final long size = cursor.getLong(3);
                    final long lastModified = cursor.getLong(4);

                    if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                        if ((flags & FILESYSTEM_FIND_FOLDERS) != 0) {
                            results.add(new FindResult(childDocumentId, uri.toString(), size, lastModified, FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY));
                        }

                        if ((flags & FILESYSTEM_FIND_RECURSIVE) != 0)
                            doFindFiles(treeUri, childDocumentId, flags, results);
                    } else {
                        if ((flags & FILESYSTEM_FIND_FILES) != 0) {
                            results.add(new FindResult(childDocumentId, uri.toString(), size, lastModified, 0));
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            cursor.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * Recursively iterates documents in the specified URI, searching for files.
     *
     * @param uriString URI containing directory to search.
     * @param flags     Native filter flags.
     * @return Array of find results.
     */
    public FindResult[] findFiles(String uriString, int flags) {
        try {
            final Uri fullUri = Uri.parse(uriString);
            final String documentId = DocumentsContract.getTreeDocumentId(fullUri);
            final ArrayList<FindResult> results = new ArrayList<>();
            doFindFiles(fullUri, documentId, flags, results);
            if (results.isEmpty())
                return null;

            final FindResult[] resultsArray = new FindResult[results.size()];
            results.toArray(resultsArray);
            return resultsArray;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Returns stat information for the given URI file.
     * @param uriString File to query.
     * @return Stat information, or null if the URI does not exist.
     */
    public StatResult statFile(String uriString) {
        try {
            final Uri uri = Uri.parse(uriString);
            final Cursor cursor = contentResolver.query(uri, statProjection, null, null, null);
            final int count = cursor.getCount();

            if (count > 0 && cursor.moveToNext()) {
                final String mimeType = cursor.getString(0);
                final long size = cursor.getLong(1);
                final long lastModified = cursor.getLong(2);
                int flags = 0;
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType))
                    flags |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;

                return new StatResult(size, lastModified, flags);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        return null;
    }

    /**
     * Returns the display name for the given URI.
     *
     * @param uriString URI to resolve display name for.
     * @return display name for the URI, or null.
     */
    public String getDisplayNameForURIPath(String uriString) {
        try {
            final Uri fullUri = Uri.parse(uriString);
            final Cursor cursor = contentResolver.query(fullUri, getDisplayNameProjection,
                    null, null, null);
            if (cursor.getCount() == 0 || !cursor.moveToNext())
                return null;

            return cursor.getString(0);
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Returns the path for a sibling file relative to another URI.
     *
     * @param uriString   URI to find the file relative to.
     * @param newFileName Sibling file name.
     * @return URI for the sibling file name, or null.
     */
    public String getRelativePathForURIPath(String uriString, String newFileName) {
        try {
            final Uri fullUri = Uri.parse(uriString);

            // if this is a document (expected)...
            Uri treeUri;
            String treeDocId;
            if (DocumentsContract.isDocumentUri(context, fullUri)) {
                // we need to remove the last part of the URI (the specific document ID) to get the parent
                final String lastPathSegment = fullUri.getLastPathSegment();
                int lastSeparatorIndex = lastPathSegment.lastIndexOf('/');
                if (lastSeparatorIndex < 0)
                    lastSeparatorIndex = lastPathSegment.lastIndexOf(':');
                if (lastSeparatorIndex < 0)
                    return null;

                // the parent becomes the document ID
                treeDocId = lastPathSegment.substring(0, lastSeparatorIndex);

                // but, we need to access it through the subtree if this was a tree URI (permissions...)
                if (isTreeUri(fullUri)) {
                    treeUri = DocumentsContract.buildTreeDocumentUri(fullUri.getAuthority(), DocumentsContract.getTreeDocumentId(fullUri));
                } else {
                    treeUri = DocumentsContract.buildTreeDocumentUri(fullUri.getAuthority(), treeDocId);
                }
            } else {
                treeDocId = DocumentsContract.getDocumentId(fullUri);
                treeUri = fullUri;
            }

            final Uri queryUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, treeDocId);
            final Cursor cursor = contentResolver.query(queryUri, getRelativeFileProjection, null, null, null);
            final int count = cursor.getCount();

            while (cursor.moveToNext()) {
                try {
                    final String displayName = cursor.getString(1);
                    if (!displayName.equalsIgnoreCase(newFileName))
                        continue;

                    final String childDocumentId = cursor.getString(0);
                    final Uri uri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childDocumentId);
                    cursor.close();
                    return uri.toString();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }

            cursor.close();
            return null;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    private static final String PRIMARY_VOLUME_NAME = "primary";

 /* @Nullable */
    public static String getFullPathFromTreeUri(/* @Nullable */ final Uri treeUri, Context con) {
        if (treeUri == null) return null;
        String volumePath = getVolumePath(getVolumeIdFromTreeUri(treeUri), con);
        if (volumePath == null) return File.separator;
        if (volumePath.endsWith(File.separator))
            volumePath = volumePath.substring(0, volumePath.length() - 1);

        String documentPath = getDocumentPathFromTreeUri(treeUri);
        if (documentPath.endsWith(File.separator))
            documentPath = documentPath.substring(0, documentPath.length() - 1);

        if (documentPath.length() > 0) {
            if (documentPath.startsWith(File.separator))
                return volumePath + documentPath;
            else
                return volumePath + File.separator + documentPath;
        } else return volumePath;
    }

    @SuppressLint("ObsoleteSdkInt")
    private static String getVolumePath(final String volumeId, Context context) {
        if (volumeId == null)
            return null;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return null;
        try {
            StorageManager mStorageManager =
                    (StorageManager) context.getSystemService(Context.STORAGE_SERVICE);
            Class<?> storageVolumeClazz = Class.forName("android.os.storage.StorageVolume");
            Method getVolumeList = mStorageManager.getClass().getMethod("getVolumeList");
            Method getUuid = storageVolumeClazz.getMethod("getUuid");
            Method getPath = storageVolumeClazz.getMethod("getPath");
            Method isPrimary = storageVolumeClazz.getMethod("isPrimary");
            Object result = getVolumeList.invoke(mStorageManager);

            final int length = Array.getLength(result);
            for (int i = 0; i < length; i++) {
                Object storageVolumeElement = Array.get(result, i);
                String uuid = (String) getUuid.invoke(storageVolumeElement);
                Boolean primary = (Boolean) isPrimary.invoke(storageVolumeElement);

                // primary volume?
                if (primary && PRIMARY_VOLUME_NAME.equals(volumeId))
                    return (String) getPath.invoke(storageVolumeElement);

                // other volumes?
                if (uuid != null && uuid.equals(volumeId))
                    return (String) getPath.invoke(storageVolumeElement);
            }
            // not found.
            return null;
        } catch (Exception ex) {
            return null;
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static String getVolumeIdFromTreeUri(final Uri treeUri) {
        final String docId = DocumentsContract.getTreeDocumentId(treeUri);
        final String[] split = docId.split(":");
        if (split.length > 0) return split[0];
        else return null;
    }


    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static String getDocumentPathFromTreeUri(final Uri treeUri) {
        final String docId = DocumentsContract.getTreeDocumentId(treeUri);
        final String[] split = docId.split(":");
        if ((split.length >= 2) && (split[1] != null)) return split[1];
        else return File.separator;
    }

 /* @Nullable */
    public static String getFullPathFromUri(/* @Nullable */ final Uri treeUri, Context con) {
        if (treeUri == null) return null;
        String volumePath = getVolumePath(getVolumeIdFromUri(treeUri), con);
        if (volumePath == null) return File.separator;
        if (volumePath.endsWith(File.separator))
            volumePath = volumePath.substring(0, volumePath.length() - 1);

        String documentPath = getDocumentPathFromUri(treeUri);
        if (documentPath.endsWith(File.separator))
            documentPath = documentPath.substring(0, documentPath.length() - 1);

        if (documentPath.length() > 0) {
            if (documentPath.startsWith(File.separator))
                return volumePath + documentPath;
            else
                return volumePath + File.separator + documentPath;
        } else return volumePath;
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static String getVolumeIdFromUri(final Uri treeUri) {
        try {
            final String docId = DocumentsContract.getDocumentId(treeUri);
            final String[] split = docId.split(":");
            if (split.length > 0) return split[0];
            else return null;
        } catch (Exception e) {
            return null;
        }
    }


    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static String getDocumentPathFromUri(final Uri treeUri) {
        try {
            final String docId = DocumentsContract.getDocumentId(treeUri);
            final String[] split = docId.split(":");
            if ((split.length >= 2) && (split[1] != null)) return split[1];
            else return File.separator;
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Java class containing the data for a file in a find operation.
     */
    public static class FindResult {
        public String name;
        public String relativeName;
        public long size;
        public long modifiedTime;
        public int flags;

        public FindResult(String relativeName, String name, long size, long modifiedTime, int flags) {
            this.relativeName = relativeName;
            this.name = name;
            this.size = size;
            this.modifiedTime = modifiedTime;
            this.flags = flags;
        }
    }

    /**
     * Java class containing the data in a stat operation.
     */
    public static class StatResult {
        public long size;
        public long modifiedTime;
        public int flags;

        public StatResult(long size, long modifiedTime, int flags) {
            this.size = size;
            this.modifiedTime = modifiedTime;
            this.flags = flags;
        }
    }
}
