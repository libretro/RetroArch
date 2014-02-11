//#define NO_UNALIGNED_MEM
//Uncomment the above if alignment is enforced.

#include "boolean.h"

//A compressing, lossy stack. Optimized for large, mostly similar, blocks of data; optimized for
// writing, less so for reading. Will discard old data if its capacity is exhausted.
struct rewindstack {
	//This is equivalent to deleting and recreating the structure, with the exception that
	// it won't reallocate the big block if the capacity is unchanged. It is safe to set the capacity
	// to 0, though this will make the structure rather useless.
	//The structure may hand out bigger blocks of data than requested. This is not detectable; just
	// ignore the extra bytes.
	//The structure may allocate a reasonable multiple of blocksize, in addition to capacity.
	//It is not possible to accurately predict how many blocks will fit in the structure; it varies
	// depending on how much the data changes. Emulator savestates are usually compressed to about
	// 0.5-2% of their original size. You can stick in some data and use capacity().
	void (*reset)(struct rewindstack * this, size_t blocksize, size_t capacity);
	
	//Asks where to put a new block. Size is same as blocksize. Don't read from it; contents are undefined.
	//push_end or push_cancel must be the first function called on the structure after this; not even free() is allowed.
	//This function cannot fail, though a pull() directly afterwards may fail.
	void * (*push_begin)(struct rewindstack * this);
	//Tells that the savestate has been written. Don't use the pointer from push_begin after this point.
	void (*push_end)(struct rewindstack * this);
	//Tells that things were not written to the pointer from push_begin. Equivalent
	// to push_end+pull, but faster, and may avoid discarding something. It is allowed to have written to the pointer.
	void (*push_cancel)(struct rewindstack * this);
	
	//Pulls off a block. Don't change it; it'll be used to generate the next one. The returned pointer is only
	// guaranteed valid until the first call to any function in this structure, with the exception that capacity()
	// will not invalidate anything. If the requested block has been discarded, or was never pushed, it returns NULL.
	const void * (*pull)(struct rewindstack * this);
	
	//Tells how many entries are in the structure, how many bytes are used, and whether the structure
	// is likely to discard something if a new item is appended. The full flag is guaranteed true if
	// it has discarded anything since the last pull() or reset(); however, it may be set even before
	// discarding, if the implementation feels like doing that.
	void (*capacity)(struct rewindstack * this, unsigned int * entries, size_t * bytes, bool * full);
	
	void (*free)(struct rewindstack * this);
};
struct rewindstack * rewindstack_create(size_t blocksize, size_t capacity);
