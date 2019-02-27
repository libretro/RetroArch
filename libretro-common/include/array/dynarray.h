/*
 * A header-only typesafe dynamic array implementation for plain C,
 * kinda like C++ std::vector. This code is compatible with C++, but should
 * only be used with POD (plain old data) types, as it uses memcpy() etc
 * instead of copy/move construction/assignment.
 * It requires a new type (created with the DA_TYPEDEF(ELEMENT_TYPE, ARRAY_TYPE_NAME)
 * macro) for each kind of element you want to put in a dynamic array; however
 * the "functions" to manipulate the array are actually macros and the same
 * for all element types.
 * The array elements are accessed via dynArr.p[i] or da_get(dynArr, i)
 * - the latter checks whether i is a valid index and asserts if not.
 *
 * One thing to keep in mind is that, because of using macros, the arguments to
 * the "functions" are usually evaluated more than once, so you should avoid
 * putting things with side effect (like function-calls with side effects or i++)
 * into them. Notable exceptions are the value arguments (v) of da_push()
 * and da_insert(), so it's still ok to do da_push(arr, fun_with_sideffects());
 * or da_insert(a, 3, x++);
 *
 * The function-like da_* macros are short aliases of dg_dynarr_* macros.
 * If the short names clash with anything in your code or other headers
 * you are using, you can, before #including this header, do
 *  #define DG_DYNARR_NO_SHORTNAMES
 * and use the long dg_dynarr_* forms of the macros instead.
 *
 * Using this library in your project:
 *   Put this file somewhere in your project.
 *   In *one* of your .c/.cpp files, do
 *     #define DG_DYNARR_IMPLEMENTATION
 *     #include "DG_dynarr.h"
 *   to create the implementation of this library in that file.
 *   You can just #include "DG_dynarr.h" (without the #define) in other source
 *   files to use it there.
 *
 * See below this comment block for a usage example.
 *
 * You can #define your own allocators, assertion and the amount of runtime
 * checking of indexes, see CONFIGURATION section in the code for more information.
 *
 *
 * This is heavily inspired by Sean Barrett's stretchy_buffer.h
 * ( see: https://github.com/nothings/stb/blob/master/stretchy_buffer.h )
 * However I wanted to have a struct that holds the array pointer and the length
 * and capacity, so that struct always remains at the same address while the
 * array memory might be reallocated.
 * I can live with arr.p[i] instead of arr[i], but I like how he managed to use
 * macros to create an API that doesn't force the user to specify the stored
 * type over and over again, so I stole some of his tricks :-)
 *
 * This has been tested with GCC 4.8 and clang 3.8 (-std=gnu89, -std=c99 and as C++;
 * -std=c89 works if you convert the C++-style comments to C comments) and
 * Microsoft Visual Studio 6 and 2010 (32bit) and 2013 (32bit and 64bit).
 * I guess it works with all (recentish) C++ compilers and C compilers supporting
 * C99 or even C89 + C++ comments (otherwise converting the comments should help).
 *
 * (C) 2016 Daniel Gibson
 *
 * LICENSE
 *   This software is dual-licensed to the public domain and under the following
 *   license: you are granted a perpetual, irrevocable license to copy, modify,
 *   publish, and distribute this file as you see fit.
 *   No warranty implied; use at your own risk.
 *
 * So you can do whatever you want with this code, including copying it
 * (or parts of it) into your own source.
 * No need to mention me or this "license" in your code or docs, even though
 * it would be appreciated, of course.
 */
#if 0 /* Usage Example: */
 #define DG_DYNARR_IMPLEMENTATION /* this define is only needed in *one* .c/.cpp file! */
 #include "DG_dynarr.h"

 DA_TYPEDEF(int, MyIntArrType); /* creates MyIntArrType - a dynamic array for ints */

 void printIntArr(MyIntArrType* arr, const char* name)
 {
     /* note that arr is a pointer here, so use *arr in the da_*() functions. */
     printf("%s = {", name);
     if(da_count(*arr) > 0)
         printf(" %d", arr->p[0]);
     for(int i=1; i<da_count(*arr); ++i)
         printf(", %d", arr->p[i]);
     printf(" }\n");
 }

 void myFunction()
 {
     MyIntArrType a1 = {0};
     /* make sure to zero out the struct
      * instead of = {0}; you could also call da_init(a1);
      */

     da_push(a1, 42);
     assert(da_count(a1) == 1 && a1.p[0] == 42);

     int* addedElements = da_addn_uninit(a1, 3);
     assert(da_count(a1) == 4);
     for(size_t i=0; i<3; ++i)
         addedElements[i] = i+5;

     printIntArr(&a1, "a1"); /* "a1 = { 42, 5, 6, 7 }" */

     MyIntArrType a2;
     da_init(a2);

     da_addn(a2, a1.p, da_count(a1)); /* copy all elements from a1 to a2 */
     assert(da_count(a2) == 4);

     da_insert(a2, 1, 11);
     printIntArr(&a2, "a2"); /* "a2 = { 42, 11, 5, 6, 7 }" */

     da_delete(a2, 2);
     printIntArr(&a2, "a2"); /* "a2 = { 42, 11, 6, 7 }" */

     da_deletefast(a2, 0);
     printIntArr(&a2, "a2"); /* "a2 = { 7, 11, 6 }" */

     da_push(a1, 3);
     printIntArr(&a1, "a1"); /* "a1 = { 42, 5, 6, 7, 3 }" */

     int x=da_pop(a1);
     printf("x = %d\n", x);  /* "x = 3" */
     printIntArr(&a1, "a1"); /* "a1 = { 42, 5, 6, 7 }" */

     da_free(a1); /* make sure not to leak memory! */
     da_free(a2);
 }
#endif /* 0 (usage example) */

#ifndef DG__DYNARR_H
#define DG__DYNARR_H

/* ######### CONFIGURATION #########
 *
 * following: some #defines that you can tweak to your liking
 *
 * you can reduce some overhead by defining DG_DYNARR_INDEX_CHECK_LEVEL to 2, 1 or 0
 */
#ifndef DG_DYNARR_INDEX_CHECK_LEVEL

	/* 0: (almost) no index checking
	 * 1: macros "returning" something return a.p[0] or NULL if the index was invalid
	 * 2: assertions in all macros taking indexes that make sure they're valid
	 * 3: 1 and 2
         */
	#define DG_DYNARR_INDEX_CHECK_LEVEL 3

#endif /* DG_DYNARR_INDEX_CHECK_LEVEL */

/* you can #define your own DG_DYNARR_ASSERT(condition, msgstring)
 * that will be used for all assertions in this code.
 */
#ifndef DG_DYNARR_ASSERT
	#include <assert.h>
	#define DG_DYNARR_ASSERT(cond, msg)  assert((cond) && msg)
#endif

/* you can #define DG_DYNARR_OUT_OF_MEMORY to some code that will be executed
 * if allocating memory fails
 * it's needed only before the #define DG_DYNARR_IMPLEMENTATION #include of
 * this header, so the following is here only for reference and commented out
 */
/*
 #ifndef DG_DYNARR_OUT_OF_MEMORY
	#define DG_DYNARR_OUT_OF_MEMORY  DG_DYNARR_ASSERT(0, "Out of Memory!");
 #endif
*/

/* By default, C's malloc(), realloc() and free() is used to allocate/free heap memory
 * (see beginning of "#ifdef DG_DYNARR_IMPLEMENTATION" block below).
 * You can #define DG_DYNARR_MALLOC, DG_DYNARR_REALLOC and DG_DYNARR_FREE yourself
 * to provide alternative implementations like Win32 Heap(Re)Alloc/HeapFree
 * it's needed only before the #define DG_DYNARR_IMPLEMENTATION #include of
 * this header, so the following is here only for reference and commented out
 */
/*
 #define DG_DYNARR_MALLOC(elemSize, numElems)  malloc(elemSize*numElems)

 // oldNumElems is not used for C's realloc, but maybe you need it for
 // your allocator to copy the old elements over
 #define DG_DYNARR_REALLOC(ptr, elemSize, oldNumElems, newCapacity) \
 	realloc(ptr, elemSize*newCapacity);

 #define DG_DYNARR_FREE(ptr)  free(ptr)
*/

/* if you want to prepend something to the non inline (DG_DYNARR_INLINE) functions,
 * like "__declspec(dllexport)" or whatever, #define DG_DYNARR_DEF
 */
#ifndef DG_DYNARR_DEF
	/* by defaults it's empty. */
	#define DG_DYNARR_DEF
#endif

/* some functions are inline, in case your compiler doesn't like "static inline"
 * but wants "__inline__" or something instead, #define DG_DYNARR_INLINE accordingly.
 */
#ifndef DG_DYNARR_INLINE
	#define DG_DYNARR_INLINE static INLINE
#endif

/* ############### Short da_* aliases for the long names ############### */

#ifndef DG_DYNARR_NO_SHORTNAMES

/* this macro is used to create an array type (struct) for elements of TYPE
 * use like DA_TYPEDEF(int, MyIntArrType); MyIntArrType ia = {0}; da_push(ia, 42); ...
 */
#define DA_TYPEDEF(TYPE, NewArrayTypeName) \
	DG_DYNARR_TYPEDEF(TYPE, NewArrayTypeName)

/* makes sure the array is initialized and can be used.
 * either do YourArray arr = {0}; or YourArray arr; da_init(arr);
 */
#define da_init(a) \
	dg_dynarr_init(a)

/* This allows you to provide an external buffer that'll be used as long as it's big enough
 * once you add more elements than buf can hold, fresh memory will be allocated on the heap
 * Use like:
 * DA_TYPEDEF(double, MyDoubleArrType);
 * MyDoubleArrType arr;
 * double buf[8];
 * dg_dynarr_init_external(arr, buf, 8);
 * dg_dynarr_push(arr, 1.23);
 * ...
 */
#define da_init_external(a, buf, buf_cap) \
	dg_dynarr_init_external(a, buf, buf_cap)

/* use this to free the memory allocated by dg_dynarr once you don't need the array anymore
 * Note: it is safe to add new elements to the array after da_free()
 *       it will allocate new memory, just like it would directly after da_init()
 */
#define da_free(a) \
	dg_dynarr_free(a)

/* add an element to the array (appended at the end) */
#define da_push(a, v) \
	dg_dynarr_push(a, v)

/* add an element to the array (appended at the end)
 * does the same as push, just for consistency with addn (like insert and insertn)
 */
#define da_add(a, v) \
	dg_dynarr_add(a, v)

/* append n elements to a and initialize them from array vals, doesn't return anything
 * ! vals (and all other args) are evaluated multiple times !
 */
#define da_addn(a, vals, n) \
	dg_dynarr_addn(a, vals, n)

/* add n elements to the end of the array and zeroes them with memset()
 * returns pointer to first added element, NULL if out of memory (array is empty then)
 */
#define da_addn_zeroed(a, n) \
	dg_dynarr_addn_zeroed(a, n)

/* add n elements to the end of the array, will remain uninitialized
 * returns pointer to first added element, NULL if out of memory (array is empty then)
 */
#define da_addn_uninit(a, n) \
	dg_dynarr_addn_uninit(a, n)

/* insert a single value v at index idx */
#define da_insert(a, idx, v) \
	dg_dynarr_insert(a, idx, v)

/* insert n elements into a at idx, initialize them from array vals
 * doesn't return anything
 * ! vals (and all other args) is evaluated multiple times !
 */
#define da_insertn(a, idx, vals, n) \
	dg_dynarr_insertn(a, idx, vals, n)

/* insert n elements into a at idx and zeroe them with memset()
 * returns pointer to first inserted element or NULL if out of memory
 */
#define da_insertn_zeroed(a, idx, n) \
	dg_dynarr_insertn_zeroed(a, idx, n)

/* insert n uninitialized elements into a at idx;
 * returns pointer to first inserted element or NULL if out of memory
 */
#define da_insertn_uninit(a, idx, n) \
	dg_dynarr_insertn_uninit(a, idx, n)

/* set a single value v at index idx - like "a.p[idx] = v;" but with checks (unless disabled) */
#define da_set(a, idx, v) \
	dg_dynarr_set(a, idx, v)

/* overwrite n elements of a, starting at idx, with values from array vals
 * doesn't return anything
 * ! vals (and all other args) is evaluated multiple times !
 */
#define da_setn(a, idx, vals, n) \
	dg_dynarr_setn(a, idx, vals, n)

/* delete the element at idx, moving all following elements (=> keeps order) */
#define da_delete(a, idx) \
	dg_dynarr_delete(a, idx)

/* delete n elements starting at idx, moving all following elements (=> keeps order) */
#define da_deleten(a, idx, n) \
	dg_dynarr_deleten(a, idx, n)

/* delete the element at idx, move the last element there (=> doesn't keep order) */
#define da_deletefast(a, idx) \
	dg_dynarr_deletefast(a, idx)

/* delete n elements starting at idx, move the last n elements there (=> doesn't keep order) */
#define da_deletenfast(a, idx, n) \
	dg_dynarr_deletenfast(a, idx, n)

/* removes all elements from the array, but does not free the buffer
 * (if you want to free the buffer too, just use da_free())
 */
#define da_clear(a) \
	dg_dynarr_clear(a)

/* sets the logical number of elements in the array
 * if cnt > dg_dynarr_count(a), the logical count will be increased accordingly
 * and the new elements will be uninitialized
 */
#define da_setcount(a, cnt) \
	dg_dynarr_setcount(a, cnt)

/* make sure the array can store cap elements without reallocating
 * logical count remains unchanged
 */
#define da_reserve(a, cap) \
	dg_dynarr_reserve(a, cap)

/* this makes sure a only uses as much memory as for its elements
 * => maybe useful if a used to contain a huge amount of elements,
 *    but you deleted most of them and want to free some memory
 * Note however that this implies an allocation and copying the remaining
 * elements, so only do this if it frees enough memory to be worthwhile!
 */
#define da_shrink_to_fit(a) \
	dg_dynarr_shrink_to_fit(a)

/* removes and returns the last element of the array */
#define da_pop(a) \
	dg_dynarr_pop(a)

/* returns the last element of the array */
#define da_last(a) \
	dg_dynarr_last(a)

/* returns the pointer *to* the last element of the array
 * (in contrast to dg_dynarr_end() which returns a pointer *after* the last element)
 * returns NULL if array is empty
 */
#define da_lastptr(a) \
	dg_dynarr_lastptr(a)

/* get element at index idx (like a.p[idx]), but with checks
 * (unless you disabled them with #define DG_DYNARR_INDEX_CHECK_LEVEL 0)
 */
#define da_get(a, idx) \
	dg_dynarr_get(a,idx)

/* get pointer to element at index idx (like &a.p[idx]), but with checks
 * and it returns NULL if idx is invalid
 */
#define da_getptr(a, idx) \
	dg_dynarr_getptr(a, idx)

/* returns a pointer to the first element of the array
 * (together with dg_dynarr_end() you can do C++-style iterating)
 */
#define da_begin(a) \
	dg_dynarr_begin(a)

/* returns a pointer to the past-the-end element of the array
 * Allows C++-style iterating, in case you're into that kind of thing:
 * for(T *it=da_begin(a), *end=da_end(a); it!=end; ++it) foo(*it);
 * (see da_lastptr() to get a pointer *to* the last element)
 */
#define da_end(a) \
	dg_dynarr_end(a)

/* returns (logical) number of elements currently in the array */
#define da_count(a) \
	dg_dynarr_count(a)

/* get the current reserved capacity of the array */
#define da_capacity(a) \
	dg_dynarr_capacity(a)

/* returns 1 if the array is empty, else 0 */
#define da_empty(a) \
	dg_dynarr_empty(a)

/* returns 1 if the last (re)allocation when inserting failed (Out Of Memory)
 *   or if the array has never allocated any memory yet, else 0
 * deleting the contents when growing fails instead of keeping old may seem
 * a bit uncool, but it's simple and OOM should rarely happen on modern systems
 * anyway - after all you need to deplete both RAM and swap/pagefile.sys
 */
#define da_oom(a) \
	dg_dynarr_oom(a)

/* sort a using the given qsort()-comparator cmp
 * (just a slim wrapper around qsort())
 */
#define da_sort(a, cmp) \
	dg_dynarr_sort(a, cmp)

#endif /* DG_DYNARR_NO_SHORTNAMES */

/* ######### Implementation of the actual macros (using the long names) ##########
 *
 * use like DG_DYNARR_TYPEDEF(int, MyIntArrType); MyIntArrType ia = {0}; dg_dynarr_push(ia, 42); ...
 */
#define DG_DYNARR_TYPEDEF(TYPE, NewArrayTypeName) \
	typedef struct { TYPE* p; dg__dynarr_md md; } NewArrayTypeName;

/* makes sure the array is initialized and can be used.
 * either do YourArray arr = {0}; or YourArray arr; dg_dynarr_init(arr);
 */
#define dg_dynarr_init(a) \
	dg__dynarr_init((void**)&(a).p, &(a).md, NULL, 0)

/* this allows you to provide an external buffer that'll be used as long as it's big enough
 * once you add more elements than buf can hold, fresh memory will be allocated on the heap
 */
#define dg_dynarr_init_external(a, buf, buf_cap) \
	dg__dynarr_init((void**)&(a).p, &(a).md, (buf), (buf_cap))

/* use this to free the memory allocated by dg_dynarr
 * Note: it is safe to add new elements to the array after dg_dynarr_free()
 *       it will allocate new memory, just like it would directly after dg_dynarr_init()
 */
#define dg_dynarr_free(a) \
	dg__dynarr_free((void**)&(a).p, &(a).md)

/* add an element to the array (appended at the end) */
#define dg_dynarr_push(a, v) \
	(dg__dynarr_maybegrowadd(dg__dynarr_unp(a), 1) ? (((a).p[(a).md.cnt++] = (v)),0) : 0)

/* add an element to the array (appended at the end)
 * does the same as push, just for consistency with addn (like insert and insertn)
 */
#define dg_dynarr_add(a, v) \
	dg_dynarr_push((a), (v))

/* append n elements to a and initialize them from array vals, doesn't return anything
 * ! vals (and all other args) are evaluated multiple times !
 */
#define dg_dynarr_addn(a, vals, n) do { \
	DG_DYNARR_ASSERT((vals)!=NULL, "Don't pass NULL als vals to dg_dynarr_addn!"); \
	if((vals)!=NULL && dg__dynarr_add(dg__dynarr_unp(a), n, 0)) { \
	  size_t i_=(a).md.cnt-(n), v_=0; \
	  while(i_<(a).md.cnt)  (a).p[i_++]=(vals)[v_++]; \
	} } DG__DYNARR_WHILE0

/* add n elements to the end of the array and zeroe them with memset()
 * returns pointer to first added element, NULL if out of memory (array is empty then)
 */
#define dg_dynarr_addn_zeroed(a, n) \
	(dg__dynarr_add(dg__dynarr_unp(a), (n), 1) ? &(a).p[(a).md.cnt-(size_t)(n)] : NULL)

/* add n elements to the end of the array, which are uninitialized
 * returns pointer to first added element, NULL if out of memory (array is empty then)
 */
#define dg_dynarr_addn_uninit(a, n) \
	(dg__dynarr_add(dg__dynarr_unp(a), (n), 0) ? &(a).p[(a).md.cnt-(size_t)(n)] : NULL)

/* insert a single value v at index idx */
#define dg_dynarr_insert(a, idx, v) \
	(dg__dynarr_checkidxle((a),(idx)), \
	 dg__dynarr_insert(dg__dynarr_unp(a), (idx), 1, 0), \
	 (a).p[dg__dynarr_idx((a).md, (idx))] = (v))

/* insert n elements into a at idx, initialize them from array vals
 * doesn't return anything
 * ! vals (and all other args) is evaluated multiple times !
 */
#define dg_dynarr_insertn(a, idx, vals, n) do { \
	DG_DYNARR_ASSERT((vals)!=NULL, "Don't pass NULL as vals to dg_dynarr_insertn!"); \
	dg__dynarr_checkidxle((a),(idx)); \
	if((vals)!=NULL && dg__dynarr_insert(dg__dynarr_unp(a), (idx), (n), 0)){ \
		size_t i_=(idx), v_=0, e_=(idx)+(n); \
		while(i_ < e_)  (a).p[i_++] = (vals)[v_++]; \
	}} DG__DYNARR_WHILE0

/* insert n elements into a at idx and zeroe them with memset()
 * returns pointer to first inserted element or NULL if out of memory
 */
#define dg_dynarr_insertn_zeroed(a, idx, n) \
	(dg__dynarr_checkidxle((a),(idx)), \
	 dg__dynarr_insert(dg__dynarr_unp(a), (idx), (n), 1) \
	  ? &(a).p[dg__dynarr_idx((a).md, (idx))] : NULL)

/* insert n uninitialized elements into a at idx;
 * returns pointer to first inserted element or NULL if out of memory
 */
#define dg_dynarr_insertn_uninit(a, idx, n) \
	(dg__dynarr_checkidxle((a),(idx)), \
	 dg__dynarr_insert(dg__dynarr_unp(a), idx, n, 0) \
	  ? &(a).p[dg__dynarr_idx((a).md, (idx))] : NULL)

/* set a single value v at index idx - like "a.p[idx] = v;" but with checks (unless disabled) */
#define dg_dynarr_set(a, idx, v) \
	(dg__dynarr_checkidx((a),(idx)), \
	 (a).p[dg__dynarr_idx((a).md, (idx))] = (v))

/* overwrite n elements of a, starting at idx, with values from array vals
 * doesn't return anything
 * ! vals (and all other args) is evaluated multiple times !
 */
#define dg_dynarr_setn(a, idx, vals, n) do { \
	DG_DYNARR_ASSERT((vals)!=NULL, "Don't pass NULL as vals to dg_dynarr_setn!"); \
	size_t idx_=(idx); size_t end_=idx_+(size_t)n; \
	dg__dynarr_checkidx((a),idx_); dg__dynarr_checkidx((a),end_-1); \
	if((vals)!=NULL && idx_ < (a).md.cnt && end_ <= (a).md.cnt) { \
		size_t v_=0; \
		while(idx_ < end_)  (a).p[idx_++] = (vals)[v_++]; \
	}} DG__DYNARR_WHILE0

/* delete the element at idx, moving all following elements (=> keeps order) */
#define dg_dynarr_delete(a, idx) \
	(dg__dynarr_checkidx((a),(idx)), dg__dynarr_delete(dg__dynarr_unp(a), (idx), 1))

/* delete n elements starting at idx, moving all following elements (=> keeps order) */
#define dg_dynarr_deleten(a, idx, n) \
	(dg__dynarr_checkidx((a),(idx)), dg__dynarr_delete(dg__dynarr_unp(a), (idx), (n)))
	/* TODO: check whether idx+n < count? */

/* delete the element at idx, move the last element there (=> doesn't keep order) */
#define dg_dynarr_deletefast(a, idx) \
	(dg__dynarr_checkidx((a),(idx)), dg__dynarr_deletefast(dg__dynarr_unp(a), (idx), 1))

/* delete n elements starting at idx, move the last n elements there (=> doesn't keep order) */
#define dg_dynarr_deletenfast(a, idx, n) \
	(dg__dynarr_checkidx((a),(idx)), dg__dynarr_deletefast(dg__dynarr_unp(a), idx, n))
	/* TODO: check whether idx+n < count? */

/* removes all elements from the array, but does not free the buffer
 * (if you want to free the buffer too, just use dg_dynarr_free())
 */
#define dg_dynarr_clear(a) \
	((a).md.cnt=0)

/* sets the logical number of elements in the array
 * if cnt > dg_dynarr_count(a), the logical count will be increased accordingly
 * and the new elements will be uninitialized
 */
#define dg_dynarr_setcount(a, n) \
	(dg__dynarr_maybegrow(dg__dynarr_unp(a), (n)) ? ((a).md.cnt = (n)) : 0)

/* make sure the array can store cap elements without reallocating
 * logical count remains unchanged
 */
#define dg_dynarr_reserve(a, cap) \
	dg__dynarr_maybegrow(dg__dynarr_unp(a), (cap))

/* this makes sure a only uses as much memory as for its elements
 * => maybe useful if a used to contain a huge amount of elements,
 *    but you deleted most of them and want to free some memory
 * Note however that this implies an allocation and copying the remaining
 * elements, so only do this if it frees enough memory to be worthwhile!
 */
#define dg_dynarr_shrink_to_fit(a) \
	dg__dynarr_shrink_to_fit(dg__dynarr_unp(a))

#if (DG_DYNARR_INDEX_CHECK_LEVEL == 1) || (DG_DYNARR_INDEX_CHECK_LEVEL == 3)

	/* removes and returns the last element of the array */
	#define dg_dynarr_pop(a) \
		(dg__dynarr_check_notempty((a), "Don't pop an empty array!"), \
		 (a).p[((a).md.cnt > 0) ? (--(a).md.cnt) : 0])

	/* returns the last element of the array */
	#define dg_dynarr_last(a) \
		(dg__dynarr_check_notempty((a), "Don't call da_last() on an empty array!"), \
		 (a).p[((a).md.cnt > 0) ? ((a).md.cnt-1) : 0])

#elif (DG_DYNARR_INDEX_CHECK_LEVEL == 0) || (DG_DYNARR_INDEX_CHECK_LEVEL == 2)

	/* removes and returns the last element of the array */
	#define dg_dynarr_pop(a) \
		(dg__dynarr_check_notempty((a), "Don't pop an empty array!"), \
		 (a).p[--(a).md.cnt])

	/* returns the last element of the array */
	#define dg_dynarr_last(a) \
		(dg__dynarr_check_notempty((a), "Don't call da_last() on an empty array!"), \
		 (a).p[(a).md.cnt-1])

#else /* invalid DG_DYNARR_INDEX_CHECK_LEVEL */
	#error Invalid index check level DG_DYNARR_INDEX_CHECK_LEVEL (must be 0-3) !
#endif /* DG_DYNARR_INDEX_CHECK_LEVEL */

/* returns the pointer *to* the last element of the array
 * (in contrast to dg_dynarr_end() which returns a pointer *after* the last element)
 * returns NULL if array is empty
 */
#define dg_dynarr_lastptr(a) \
	(((a).md.cnt > 0) ? ((a).p + (a).md.cnt - 1) : NULL)

/* get element at index idx (like a.p[idx]), but with checks
 * (unless you disabled them with #define DG_DYNARR_INDEX_CHECK_LEVEL 0)
 */
#define dg_dynarr_get(a, idx) \
	(dg__dynarr_checkidx((a),(idx)), (a).p[dg__dynarr_idx((a).md, (idx))])

/* get pointer to element at index idx (like &a.p[idx]), but with checks
 * (unless you disabled them with #define DG_DYNARR_INDEX_CHECK_LEVEL 0)
 * if index-checks are disabled, it returns NULL on invalid index (else it asserts() before returning)
 */
#define dg_dynarr_getptr(a, idx) \
	(dg__dynarr_checkidx((a),(idx)), \
	 ((size_t)(idx) < (a).md.cnt) ? ((a).p+(size_t)(idx)) : NULL)

/* returns a pointer to the first element of the array
 * (together with dg_dynarr_end() you can do C++-style iterating)
 */
#define dg_dynarr_begin(a) \
	((a).p)

/* returns a pointer to the past-the-end element of the array
 * Allows C++-style iterating, in case you're into that kind of thing:
 * for(T *it=dg_dynarr_begin(a), *end=dg_dynarr_end(a); it!=end; ++it) foo(*it);
 * (see dg_dynarr_lastptr() to get a pointer *to* the last element)
 */
#define dg_dynarr_end(a) \
	((a).p + (a).md.cnt)

/* returns (logical) number of elements currently in the array */
#define dg_dynarr_count(a) \
	((a).md.cnt)

/* get the current reserved capacity of the array */
#define dg_dynarr_capacity(a) \
	((a).md.cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB)

/* returns 1 if the array is empty, else 0 */
#define dg_dynarr_empty(a) \
	((a).md.cnt == 0)

/* returns 1 if the last (re)allocation when inserting failed (Out Of Memory)
 *   or if the array has never allocated any memory yet, else 0
 * deleting the contents when growing fails instead of keeping old may seem
 * a bit uncool, but it's simple and OOM should rarely happen on modern systems
 * anyway - after all you need to deplete both RAM and swap/pagefile.sys
 * or deplete the address space, which /might/ happen with 32bit applications
 * but probably not with 64bit (at least in the foreseeable future)
 */
#define dg_dynarr_oom(a) \
	((a).md.cap == 0)

/* sort a using the given qsort()-comparator cmp
 * (just a slim wrapper around qsort())
 */
#define dg_dynarr_sort(a, cmp) \
	qsort((a).p, (a).md.cnt, sizeof((a).p[0]), (cmp))

/* ######### Implementation-Details that are not part of the API ########## */

#include <stdlib.h> /* size_t, malloc(), free(), realloc() */
#include <string.h> /* memset(), memcpy(), memmove() */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	size_t cnt; /* logical number of elements */
	size_t cap; /* cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB is actual capacity (in elements, *not* bytes!) */
		/* if(cap & DG__DYNARR_SIZE_T_MSB) the current memory is not allocated by dg_dynarr,
		 * but was set with dg_dynarr_init_external()
		 * that's handy to give an array a base-element storage on the stack, for example
		 * TODO: alternatively, we could introduce a flag field to this struct and use that,
		 *       so we don't have to calculate & everytime cap is needed
		 */
} dg__dynarr_md;

/* I used to have the following in an enum, but MSVC assumes enums are always 32bit ints */
static const size_t DG__DYNARR_SIZE_T_MSB = ((size_t)1) << (sizeof(size_t)*8 - 1);
static const size_t DG__DYNARR_SIZE_T_ALL_BUT_MSB = (((size_t)1) << (sizeof(size_t)*8 - 1))-1;

/* "unpack" the elements of an array struct for use with helper functions
 * (to void** arr, dg__dynarr_md* md, size_t itemsize)
 */
#define dg__dynarr_unp(a) \
	(void**)&(a).p, &(a).md, sizeof((a).p[0])

/* MSVC warns about "conditional expression is constant" when using the
 * do { ... } while(0) idiom in macros..
 */
#ifdef _MSC_VER
  #if _MSC_VER >= 1400 // MSVC 2005 and newer
    /* people claim MSVC 2005 and newer support __pragma, even though it's only documented
     * for 2008+ (https://msdn.microsoft.com/en-us/library/d9x1s805%28v=vs.90%29.aspx)
     * the following workaround is based on
     * http://cnicholson.net/2009/03/stupid-c-tricks-dowhile0-and-c4127/
     */
    #define DG__DYNARR_WHILE0 \
      __pragma(warning(push)) \
      __pragma(warning(disable:4127)) \
      while(0) \
      __pragma(warning(pop))
  #else /* older MSVC versions don't support __pragma - I heard this helps for them */
    #define DG__DYNARR_WHILE0  while(0,0)
  #endif

#else /* other compilers */

	#define DG__DYNARR_WHILE0  while(0)

#endif /* _MSC_VER */

#if (DG_DYNARR_INDEX_CHECK_LEVEL == 2) || (DG_DYNARR_INDEX_CHECK_LEVEL == 3)

	#define dg__dynarr_checkidx(a,i) \
		DG_DYNARR_ASSERT((size_t)i < a.md.cnt, "index out of bounds!")

	/* special case for insert operations: == cnt is also ok, insert will append then */
	#define dg__dynarr_checkidxle(a,i) \
		DG_DYNARR_ASSERT((size_t)i <= a.md.cnt, "index out of bounds!")

	#define dg__dynarr_check_notempty(a, msg) \
		DG_DYNARR_ASSERT(a.md.cnt > 0, msg)

#elif (DG_DYNARR_INDEX_CHECK_LEVEL == 0) || (DG_DYNARR_INDEX_CHECK_LEVEL == 1)

	/* no assertions that check if index is valid */
	#define dg__dynarr_checkidx(a,i) (void)0
	#define dg__dynarr_checkidxle(a,i) (void)0

	#define dg__dynarr_check_notempty(a, msg) (void)0

#else /* invalid DG_DYNARR_INDEX_CHECK_LEVEL */
	#error Invalid index check level DG_DYNARR_INDEX_CHECK_LEVEL (must be 0-3) !
#endif /* DG_DYNARR_INDEX_CHECK_LEVEL */

#if (DG_DYNARR_INDEX_CHECK_LEVEL == 1) || (DG_DYNARR_INDEX_CHECK_LEVEL == 3)

	/* the given index, if valid, else 0 */
	#define dg__dynarr_idx(md,i) \
		(((size_t)(i) < md.cnt) ? (size_t)(i) : 0)

#elif (DG_DYNARR_INDEX_CHECK_LEVEL == 0) || (DG_DYNARR_INDEX_CHECK_LEVEL == 2)

	/* don't check and default to 0 if invalid, but just use the given value */
	#define dg__dynarr_idx(md,i) (size_t)(i)

#else /* invalid DG_DYNARR_INDEX_CHECK_LEVEL */
	#error Invalid index check level DG_DYNARR_INDEX_CHECK_LEVEL (must be 0-3) !
#endif /* DG_DYNARR_INDEX_CHECK_LEVEL */

/* the functions allocating/freeing memory are not implemented inline, but
 * in the #ifdef DG_DYNARR_IMPLEMENTATION section
 * one reason is that dg__dynarr_grow has the most code in it, the other is
 * that windows has weird per-dll heaps so free() or realloc() should be
 * called from code in the same dll that allocated the memory - these kind
 * of wrapper functions that end up compiled into the exe or *one* dll
 * (instead of inline functions compiled into everything) should ensure that.
 */

DG_DYNARR_DEF void
dg__dynarr_free(void** p, dg__dynarr_md* md);

DG_DYNARR_DEF void
dg__dynarr_shrink_to_fit(void** arr, dg__dynarr_md* md, size_t itemsize);

/* grow array to have enough space for at least min_needed elements
 * if it fails (OOM), the array will be deleted, a.p will be NULL, a.md.cap and a.md.cnt will be 0
 * and the functions returns 0; else (on success) it returns 1
 */
DG_DYNARR_DEF int
dg__dynarr_grow(void** arr, dg__dynarr_md* md, size_t itemsize, size_t min_needed);

/* the following functions are implemented inline, because they're quite short
 * and mosty implemented in functions so the macros don't get too ugly
 */

DG_DYNARR_INLINE void
dg__dynarr_init(void** p, dg__dynarr_md* md, void* buf, size_t buf_cap)
{
	*p = buf;
	md->cnt = 0;
	if(buf == NULL)  md->cap = 0;
	else md->cap = (DG__DYNARR_SIZE_T_MSB | buf_cap);
}

DG_DYNARR_INLINE int
dg__dynarr_maybegrow(void** arr, dg__dynarr_md* md, size_t itemsize, size_t min_needed)
{
	if((md->cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB) >= min_needed)  return 1;
	else return dg__dynarr_grow(arr, md, itemsize, min_needed);
}

DG_DYNARR_INLINE int
dg__dynarr_maybegrowadd(void** arr, dg__dynarr_md* md, size_t itemsize, size_t num_add)
{
	size_t min_needed = md->cnt+num_add;
	if((md->cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB) >= min_needed)  return 1;
	else return dg__dynarr_grow(arr, md, itemsize, min_needed);
}

DG_DYNARR_INLINE int
dg__dynarr_insert(void** arr, dg__dynarr_md* md, size_t itemsize, size_t idx, size_t n, int init0)
{
	/* allow idx == md->cnt to append */
	size_t oldCount = md->cnt;
	size_t newCount = oldCount+n;
	if(idx <= oldCount && dg__dynarr_maybegrow(arr, md, itemsize, newCount))
	{
		unsigned char* p = (unsigned char*)*arr; /* *arr might have changed in dg__dynarr_grow()! */
		/* move all existing items after a[idx] to a[idx+n] */
		if(idx < oldCount)  memmove(p+(idx+n)*itemsize, p+idx*itemsize, itemsize*(oldCount - idx));

		/* if the memory is supposed to be zeroed, do that */
		if(init0)  memset(p+idx*itemsize, 0, n*itemsize);

		md->cnt = newCount;
		return 1;
	}
	return 0;
}

DG_DYNARR_INLINE int
dg__dynarr_add(void** arr, dg__dynarr_md* md, size_t itemsize, size_t n, int init0)
{
	size_t cnt = md->cnt;
	if(dg__dynarr_maybegrow(arr, md, itemsize, cnt+n))
	{
		unsigned char* p = (unsigned char*)*arr; /* *arr might have changed in dg__dynarr_grow()! */
		/* if the memory is supposed to be zeroed, do that */
		if(init0)  memset(p+cnt*itemsize, 0, n*itemsize);

		md->cnt += n;
		return 1;
	}
	return 0;
}

DG_DYNARR_INLINE void
dg__dynarr_delete(void** arr, dg__dynarr_md* md, size_t itemsize, size_t idx, size_t n)
{
	size_t cnt = md->cnt;
	if(idx < cnt)
	{
		if(idx+n >= cnt)  md->cnt = idx; /* removing last element(s) => just reduce count */
		else
		{
			unsigned char* p = (unsigned char*)*arr;
			/* move all items following a[idx+n] to a[idx] */
			memmove(p+itemsize*idx, p+itemsize*(idx+n), itemsize*(cnt - (idx+n)));
			md->cnt -= n;
		}
	}
}

DG_DYNARR_INLINE void
dg__dynarr_deletefast(void** arr, dg__dynarr_md* md, size_t itemsize, size_t idx, size_t n)
{
	size_t cnt = md->cnt;
	if(idx < cnt)
	{
		if(idx+n >= cnt)  md->cnt = idx; /* removing last element(s) => just reduce count */
		else
		{
			unsigned char* p = (unsigned char*)*arr;
			/* copy the last n items to a[idx] - but handle the case that
			 * the array has less than n elements left after the deleted elements
			 */
			size_t numItemsAfterDeleted = cnt - (idx+n);
			size_t m = (n < numItemsAfterDeleted) ? n : numItemsAfterDeleted;
			memcpy(p+itemsize*idx, p+itemsize*(cnt - m), itemsize*m);
			md->cnt -= n;
		}
	}
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DG__DYNARR_H */

/* ############## Implementation of non-inline functions ############## */

#ifdef DG_DYNARR_IMPLEMENTATION

/* by default, C's malloc(), realloc() and free() is used to allocate/free heap memory.
 * you can #define DG_DYNARR_MALLOC, DG_DYNARR_REALLOC and DG_DYNARR_FREE
 * to provide alternative implementations like Win32 Heap(Re)Alloc/HeapFree
 */
#ifndef DG_DYNARR_MALLOC
	#define DG_DYNARR_MALLOC(elemSize, numElems)  malloc(elemSize*numElems)

	/* oldNumElems is not used here, but maybe you need it for your allocator
	 * to copy the old elements over
	 */
	#define DG_DYNARR_REALLOC(ptr, elemSize, oldNumElems, newCapacity) \
		realloc(ptr, elemSize*newCapacity);

	#define DG_DYNARR_FREE(ptr)  free(ptr)
#endif

/* you can #define DG_DYNARR_OUT_OF_MEMORY to some code that will be executed
 * if allocating memory fails
 */
#ifndef DG_DYNARR_OUT_OF_MEMORY
	#define DG_DYNARR_OUT_OF_MEMORY  DG_DYNARR_ASSERT(0, "Out of Memory!");
#endif

#ifdef __cplusplus
extern "C" {
#endif

DG_DYNARR_DEF void
dg__dynarr_free(void** p, dg__dynarr_md* md)
{
	/* only free memory if it doesn't point to external memory */
	if(!(md->cap & DG__DYNARR_SIZE_T_MSB))
	{
		DG_DYNARR_FREE(*p);
		*p = NULL;
		md->cap = 0;
	}
	md->cnt = 0;
}

DG_DYNARR_DEF int
dg__dynarr_grow(void** arr, dg__dynarr_md* md, size_t itemsize, size_t min_needed)
{
	size_t cap = md->cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB;

	DG_DYNARR_ASSERT(min_needed > cap, "dg__dynarr_grow() should only be called if storage actually needs to grow!");

	if(min_needed < DG__DYNARR_SIZE_T_MSB)
	{
		size_t newcap = (cap > 4) ? (2*cap) : 8; /* allocate for at least 8 elements */
		/* make sure not to set DG__DYNARR_SIZE_T_MSB (unlikely anyway) */
		if(newcap >= DG__DYNARR_SIZE_T_MSB)  newcap = DG__DYNARR_SIZE_T_MSB-1;
		if(min_needed > newcap)  newcap = min_needed;

		/* the memory was allocated externally, don't free it, just copy contents */
		if(md->cap & DG__DYNARR_SIZE_T_MSB)
		{
			void* p = DG_DYNARR_MALLOC(itemsize, newcap);
			if(p != NULL)  memcpy(p, *arr, itemsize*md->cnt);
			*arr = p;
		}
		else
		{
			void* p = DG_DYNARR_REALLOC(*arr, itemsize, md->cnt, newcap);
			if(p == NULL)  DG_DYNARR_FREE(*arr); /* realloc failed, at least don't leak memory */
			*arr = p;
		}

		/* TODO: handle OOM by setting highest bit of count and keeping old data? */

		if(*arr)  md->cap = newcap;
		else
		{
			md->cap = 0;
			md->cnt = 0;

			DG_DYNARR_OUT_OF_MEMORY ;

			return 0;
		}
		return 1;
	}
	DG_DYNARR_ASSERT(min_needed < DG__DYNARR_SIZE_T_MSB, "Arrays must stay below SIZE_T_MAX/2 elements!");
	return 0;
}

DG_DYNARR_DEF void
dg__dynarr_shrink_to_fit(void** arr, dg__dynarr_md* md, size_t itemsize)
{
	/* only do this if we allocated the memory ourselves */
	if(!(md->cap & DG__DYNARR_SIZE_T_MSB))
	{
		size_t cnt = md->cnt;
		if(cnt == 0)  dg__dynarr_free(arr, md);
		else if((md->cap & DG__DYNARR_SIZE_T_ALL_BUT_MSB) > cnt)
		{
			void* p = DG_DYNARR_MALLOC(itemsize, cnt);
			if(p != NULL)
			{
				memcpy(p, *arr, cnt*itemsize);
				md->cap = cnt;
				DG_DYNARR_FREE(*arr);
				*arr = p;
			}
		}
	}
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DG_DYNARR_IMPLEMENTATION */
