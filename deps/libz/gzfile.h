
#ifndef _GZFILE_H
#define _GZFILE_H

struct gzFile_s
{
   unsigned have;
   unsigned char *next;
   z_off64_t pos;
};

#endif
