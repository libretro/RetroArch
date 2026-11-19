package android.os;

public final class ParcelFileDescriptor
{
   private final int fd;

   public ParcelFileDescriptor(int fd)
   {
      this.fd = fd;
   }

   public int detachFd()
   {
      return fd;
   }
}
