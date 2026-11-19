package android.database;

import java.io.Closeable;

public interface Cursor extends Closeable
{
   int getCount();
   boolean moveToNext();
   String getString(int column);
   long getLong(int column);
   boolean isNull(int column);
   void close();
}
