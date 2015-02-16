#include <stddef.h>
#include <boolean.h>

enum nbio_mode_t
{
   /* The comments tell which mode in fopen() it corresponds to. */
   NBIO_READ = 0,/* rb */
   NBIO_WRITE,   /* wb */
   NBIO_UPDATE,  /* r+b */
};

struct nbio_t;

/*
 * Creates an nbio structure for performing the given operation on the given file.
 */
struct nbio_t* nbio_open(const char * filename, enum nbio_mode_t mode);

/*
 * Starts reading the given file. When done, it will be available in nbio_get_ptr.
 * Can not be done if the structure was created with nbio_write.
 */
void nbio_begin_read(struct nbio_t* handle);

/*
 * Starts writing to the given file. Before this, you should've copied the data to nbio_get_ptr.
 * Can not be done if the structure was created with nbio_read.
 */
void nbio_begin_write(struct nbio_t* handle);

/*
 * Performs part of the requested operation, or checks how it's going.
 * When it returns true, it's done.
 */
bool nbio_iterate(struct nbio_t* handle, size_t* progress, size_t* len);

/*
 * Resizes the file up to the given size; cannot shrink.
 * Can not be done if the structure was created with nbio_read.
 */
void nbio_resize(struct nbio_t* handle, size_t len);

/*
 * Returns a pointer to the file data. Writable only if structure was not created with nbio_read.
 * If any operation is in progress, the pointer will be NULL, but len will still be correct.
 */
void* nbio_get_ptr(struct nbio_t* handle, size_t* len);

/*
 * Deletes the nbio structure and its associated pointer.
 */
void nbio_free(struct nbio_t* handle);
