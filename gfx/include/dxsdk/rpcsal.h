/****************************************************************\
*                                                                *
* rpcsal.h - markers for documenting the semantics of RPC APIs   *
*                                                                *
* Version 1.0                                                    *
*                                                                *
* Copyright (c) 2004 Microsoft Corporation. All rights reserved. *
*                                                                *
\****************************************************************/

// -------------------------------------------------------------------------------
// Introduction
//
// rpcsal.h provides a set of annotations to describe how RPC functions use their
// parameters - the assumptions it makes about them, adn the guarantees it makes 
// upon finishing.  These annotations are similar to those found in specstrings.h,
// but are designed to be used by the MIDL compiler when it generates annotations
// enabled header files.
//
// IDL authors do not need to annotate their functions declarations.  The MIDL compiler
// will interpret the IDL directives and use one of the annotations contained 
// in this header.  This documentation is intended to help those trying to  understand 
// the MIDL-generated header files or those who maintain their own copies of these files.
//
// -------------------------------------------------------------------------------
// Differences between rpcsal.h and specstrings.h
// 
// There are a few important differences between the annotations found in rpcsal.h and
// those in specstrings.h:
// 
// 1. [in] parameters are not marked as read-only.  They may be used for scratch space 
// at the server and changes will not affect the client.
// 2. String versions of each macro alleviates the need for a special type definition
//
// -------------------------------------------------------------------------------
// Interpreting RPC Annotations
//
// These annotations are interpreted precisely in the same way as those in specstrings.h.  
// Please refer to that header for information related to general usage in annotations. 
//
// To construct an RPC annotation, concatenate the appropriate value from each category
// along with a leading __RPC_.  A typical annotation looks like "__RPC__in_string".
//
// |----------------------------------------------------------------------------------|
// | RPC Annotations                                                                  |
// |------------|------------|---------|--------|----------|----------|---------------|
// |   Level    |   Usage    |  Size   | Output | Optional |  String  |  Parameters   |
// |------------|------------|---------|--------|----------|----------|---------------|
// | <>         | <>         | <>      | <>     | <>       | <>       | <>            |
// | _deref     | _in        | _ecount | _full  | _opt     | _string  | (size)        |
// | _deref_opt | _out       | _bcount | _part  |          |          | (size,length) |
// |            | _inout     |         |        |          |          |               |
// |            |            |         |        |          |          |               |
// |------------|------------|---------|--------|----------|----------|---------------|
//
// Level: Describes the buffer pointer's level of indirection from the parameter or
//          return value 'p'.
//
// <>         : p is the buffer pointer.
// _deref     : *p is the buffer pointer. p must not be NULL.
// _deref_opt : *p may be the buffer pointer. p may be NULL, in which case the rest of
//                the annotation is ignored.
//
// Usage: Describes how the function uses the buffer.
//
// <>     : The buffer is not accessed. If used on the return value or with _deref, the
//            function will provide the buffer, and it will be uninitialized at exit.
//            Otherwise, the caller must provide the buffer. This should only be used
//            for alloc and free functions.
// _in    : The function will only read from the buffer. The caller must provide the
//            buffer and initialize it. Cannot be used with _deref.
// _out   : The function will only write to the buffer. If used on the return value or
//            with _deref, the function will provide the buffer and initialize it.
//            Otherwise, the caller must provide the buffer, and the function will
//            initialize it.
// _inout : The function may freely read from and write to the buffer. The caller must
//            provide the buffer and initialize it. If used with _deref, the buffer may
//            be reallocated by the function.
//
// Size: Describes the total size of the buffer. This may be less than the space actually
//         allocated for the buffer, in which case it describes the accessible amount.
//
// <>      : No buffer size is given. If the type specifies the buffer size (such as
//             with LPSTR and LPWSTR), that amount is used. Otherwise, the buffer is one
//             element long. Must be used with _in, _out, or _inout.
// _ecount : The buffer size is an explicit element count.
// _bcount : The buffer size is an explicit byte count.
//
// Output: Describes how much of the buffer will be initialized by the function. For
//           _inout buffers, this also describes how much is initialized at entry. Omit this
//           category for _in buffers; they must be fully initialized by the caller.
//
// <>    : The type specifies how much is initialized. For instance, a function initializing
//           an LPWSTR must NULL-terminate the string.
// _full : The function initializes the entire buffer.
// _part : The function initializes part of the buffer, and explicitly indicates how much.
//
// Optional: Describes if the buffer itself is optional.
//
// <>   : The pointer to the buffer must not be NULL.
// _opt : The pointer to the buffer might be NULL. It will be checked before being dereferenced.
//
// String: Describes if the buffer is NULL terminated
//
// <>      : The buffer is not assumed to be NULL terminated
// _string : The buffer is assumed to be NULL terminated once it has been initialized
//
// Parameters: Gives explicit counts for the size and length of the buffer.
//
// <>            : There is no explicit count. Use when neither _ecount nor _bcount is used.
// (size)        : Only the buffer's total size is given. Use with _ecount or _bcount but not _part.
// (size,length) : The buffer's total size and initialized length are given. Use with _ecount_part
//                   and _bcount_part.
//
// Notes:
//
// 1. Specifying two buffer annotations on a single parameter results in unspecified behavior
//    (e.g. __RPC__in_bcount(5) __RPC__out_bcount(6)
// 
// 2. The size of the buffer and the amount that has been initialized are separate concepts.  
//    Specify the size using _ecount or _bcount.  Specify the amount that is initialized using 
//    _full, _part, or _string.  As a special case, a single element buffer does not need 
//    _ecount, _bcount, _full, or _part
// 
// 3. The count may be less than the total size of the buffer in which case it describes the 
//    accessible portion. 
// 
// 4. "__RPC__opt" and "__RPC_deref" are not valid annotations.
// 
// 5. The placement of _opt when using _deref is important:
//      __RPC__deref_opt_...      : Input may be NULL
//      __RPC__deref_..._opt      : Output may be NULL
//      __RPC__deref_opt_..._opt  : Both input and output may be NULL
//

#pragma once

#include <specstrings.h>

#ifndef __RPCSAL_H_VERSION__
#define __RPCSAL_H_VERSION__        ( 100 )
#endif // __RPCSAL_H_VERSION__

#ifdef __REQUIRED_RPCSAL_H_VERSION__
    #if ( __RPCSAL_H_VERSION__ < __REQUIRED_RPCSAL_H_VERSION__ )
        #error incorrect <rpcsal.h> version. Use the header that matches with the MIDL compiler.
    #endif
#endif


#ifdef  __cplusplus
extern "C" {
#endif  // #ifdef __cplusplus


#ifndef _SAL1_2_Source_
#define _SAL1_2_Source_(Name, args, annotes) _SA_annotes3(SAL_name, #Name, "", "1.2") _Group_(annotes _SAL_nop_impl_)
#endif // _SAL1_2_Source_

// [in]
#define __RPC__in                                    _SAL1_2_Source_(__RPC__in, (), _Pre_ _Notref_ _Notnull_ _Pre_ _Valid_)
#define __RPC__in_string                             _SAL1_2_Source_(__RPC__in_string, (), __RPC__in _Pre_ _Null_terminated_)
#define __RPC__in_ecount(size)                       _SAL1_2_Source_(__RPC__in_ecount, (size), __RPC__in _Pre_readable_size_(size))
#define __RPC__in_ecount_full(size)                  _SAL1_2_Source_(__RPC__in_ecount_full, (size), __RPC__in_ecount(size))
#define __RPC__in_ecount_full_string(size)           _SAL1_2_Source_(__RPC__in_ecount_full_string, (size), __RPC__in_ecount_full(size) _Pre_ _Null_terminated_)
#define __RPC__in_ecount_part(size, length)          _SAL1_2_Source_(__RPC__in_ecount_part, (size,length), __RPC__in_ecount(length) _Pre_writable_size_(size))
#define __RPC__in_xcount(size)                       _SAL1_2_Source_(__RPC__in_xcount, (size), __RPC__in _Pre_readable_size_(size))
#define __RPC__in_xcount_full(size)                  _SAL1_2_Source_(__RPC__in_xcount_full, (size), __RPC__in_ecount(size))
#define __RPC__in_xcount_full_string(size)           _SAL1_2_Source_(__RPC__in_xcount_full_string, (size), __RPC__in_ecount_full(size) _Pre_ _Null_terminated_)
#define __RPC__in_xcount_part(size, length)          _SAL1_2_Source_(__RPC__in_xcount_part, (size,length), __RPC__in_ecount(length) _Pre_writable_size_(size))


#define __RPC__deref_in                              _SAL1_2_Source_(__RPC__deref_in, (), __RPC__in _At_(*_Curr_, _Pre_ _Notnull_))
#define __RPC__deref_in_string                       _SAL1_2_Source_(__RPC__deref_in_string, (), __RPC__deref_in _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_in_opt                          _SAL1_2_Source_(__RPC__deref_in_opt, (), __RPC__in _At_(*_Curr_, _Pre_ _Maybenull_))
#define __RPC__deref_in_opt_string                   _SAL1_2_Source_(__RPC__deref_in_opt_string, (), __RPC__deref_in_opt _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_opt_in                          _SAL1_2_Source_(__RPC__deref_opt_in, (), __RPC__in_opt _At_(*_Curr_, _Pre_ _Notnull_))
#define __RPC__deref_opt_in_string                   _SAL1_2_Source_(__RPC__deref_opt_in_string, (), __RPC__deref_opt_in _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_opt_in_opt                      _SAL1_2_Source_(__RPC__deref_opt_in_opt, (), __RPC__in_opt _At_(*_Curr_, _Pre_ _Maybenull_))
#define __RPC__deref_opt_in_opt_string               _SAL1_2_Source_(__RPC__deref_opt_in_opt_string, (), __RPC__deref_opt_in_opt _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_in_ecount(size)                 _SAL1_2_Source_(__RPC__deref_in_ecount, (size), __RPC__in _At_(*_Curr_, _Pre_ _Notnull_ _Pre_readable_size_(size)))
#define __RPC__deref_in_ecount_part(size, length)    _SAL1_2_Source_(__RPC__deref_in_ecount_part, (size,length), __RPC__deref_in_ecount(size)  _At_(*_Curr_, _Pre_readable_size_(length)))
#define __RPC__deref_in_ecount_full(size)            _SAL1_2_Source_(__RPC__deref_in_ecount_full, (size), __RPC__deref_in_ecount_part(size, size))
#define __RPC__deref_in_ecount_full_opt(size)        _SAL1_2_Source_(__RPC__deref_in_ecount_full_opt, (size), __RPC__deref_in_ecount_part_opt(size, size))
#define __RPC__deref_in_ecount_full_opt_string(size)  _SAL1_2_Source_(__RPC__deref_in_ecount_full_opt_string, (size), __RPC__deref_in_ecount_full_opt(size) _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_in_ecount_full_string(size)     _SAL1_2_Source_(__RPC__deref_in_ecount_full_string, (size), __RPC__deref_in_ecount_full(size) _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_in_ecount_opt(size)             _SAL1_2_Source_(__RPC__deref_in_ecount_opt, (size), __RPC__in _At_(*_Curr_, _Pre_ _Maybenull_ _Pre_readable_size_(size)))
#define __RPC__deref_in_ecount_opt_string(size)      _SAL1_2_Source_(__RPC__deref_in_ecount_opt_string, (size), __RPC__deref_in_ecount_opt(size) _At_(*_Curr_, _Pre_ _Null_terminated_))
#define __RPC__deref_in_ecount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_in_ecount_part_opt, (size,length), __RPC__deref_in_ecount_opt(size) _At_(*_Curr_, _Pre_readable_size_(length)))
#define __RPC__deref_in_xcount(size)                 _SAL1_2_Source_(__RPC__deref_in_xcount, (size), __RPC__deref_in_ecount(size))
#define __RPC__deref_in_xcount_part(size, length)    _SAL1_2_Source_(__RPC__deref_in_xcount_part, (size,length), __RPC__deref_in_ecount_part(size, length))
#define __RPC__deref_in_xcount_full(size)            _SAL1_2_Source_(__RPC__deref_in_xcount_full, (size), __RPC__deref_in_ecount_part(size, size))
#define __RPC__deref_in_xcount_full_opt(size)        _SAL1_2_Source_(__RPC__deref_in_xcount_full_opt, (size), __RPC__deref_in_ecount_full_opt(size))
#define __RPC__deref_in_xcount_full_opt_string(size)  _SAL1_2_Source_(__RPC__deref_in_xcount_full_opt_string, (size), __RPC__deref_in_ecount_full_opt_string(size))
#define __RPC__deref_in_xcount_full_string(size)     _SAL1_2_Source_(__RPC__deref_in_xcount_full_string, (size), __RPC__deref_in_ecount_full_string(size))
#define __RPC__deref_in_xcount_opt(size)             _SAL1_2_Source_(__RPC__deref_in_xcount_opt, (size), __RPC__deref_in_ecount_opt(size))
#define __RPC__deref_in_xcount_opt_string(size)      _SAL1_2_Source_(__RPC__deref_in_xcount_opt_string, (size), __RPC__deref_in_ecount_opt_string(size))
#define __RPC__deref_in_xcount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_in_xcount_part_opt, (size,length), __RPC__deref_in_ecount_part_opt(size))

// [out]
#define __RPC__out                                   _SAL1_2_Source_(__RPC__out, (), _Out_)
#define __RPC__out_ecount(size)                      _SAL1_2_Source_(__RPC__out_ecount, (size), _Out_writes_(size)  _Post_writable_size_(size))
#define __RPC__out_ecount_string(size)               _SAL1_2_Source_(__RPC__out_ecount_string, (size), __RPC__out_ecount(size) _Post_ _Null_terminated_)
#define __RPC__out_ecount_part(size, length)         _SAL1_2_Source_(__RPC__out_ecount_part, (size,length), __RPC__out_ecount(size) _Post_readable_size_(length))
#define __RPC__out_ecount_full(size)                 _SAL1_2_Source_(__RPC__out_ecount_full, (size), __RPC__out_ecount_part(size, size))
#define __RPC__out_ecount_full_string(size)          _SAL1_2_Source_(__RPC__out_ecount_full_string, (size), __RPC__out_ecount_full(size) _Post_ _Null_terminated_)
#define __RPC__out_xcount(size)                      _SAL1_2_Source_(__RPC__out_xcount, (size), _Out_)
#define __RPC__out_xcount_string(size)               _SAL1_2_Source_(__RPC__out_xcount_string, (size), __RPC__out _Post_ _Null_terminated_)
#define __RPC__out_xcount_part(size, length)         _SAL1_2_Source_(__RPC__out_xcount_part, (size,length), __RPC__out)
#define __RPC__out_xcount_full(size)                 _SAL1_2_Source_(__RPC__out_xcount_full, (size), __RPC__out)
#define __RPC__out_xcount_full_string(size)          _SAL1_2_Source_(__RPC__out_xcount_full_string, (size), __RPC__out _Post_ _Null_terminated_)

// [in,out] 
#define __RPC__inout                                 _SAL1_2_Source_(__RPC__inout, (), _Inout_)
#define __RPC__inout_string                          _SAL1_2_Source_(__RPC__inout_string, (), __RPC__inout  _Pre_ _Null_terminated_ _Post_ _Null_terminated_)
#define __RPC__inout_ecount(size)                    _SAL1_2_Source_(__RPC__inout_ecount, (size), _Inout_updates_(size))
#define __RPC__inout_ecount_part(size, length)       _SAL1_2_Source_(__RPC__inout_ecount_part, (size,length), _Inout_updates_to_(size, length))
#define __RPC__inout_ecount_full(size)               _SAL1_2_Source_(__RPC__inout_ecount_full, (size), __RPC__inout_ecount_part(size, size))
#define __RPC__inout_ecount_full_string(size)        _SAL1_2_Source_(__RPC__inout_ecount_full_string, (size), __RPC__inout_ecount_full(size) _Pre_ _Null_terminated_ _Post_ _Null_terminated_)
#define __RPC__inout_xcount(size)                    _SAL1_2_Source_(__RPC__inout_xcount, (size), _Inout_)
#define __RPC__inout_xcount_part(size, length)       _SAL1_2_Source_(__RPC__inout_xcount_part, (size,length), _Inout_)
#define __RPC__inout_xcount_full(size)               _SAL1_2_Source_(__RPC__inout_xcount_full, (size), __RPC__inout)
#define __RPC__inout_xcount_full_string(size)        _SAL1_2_Source_(__RPC__inout_xcount_full_string, (size), __RPC__inout _Pre_ _Null_terminated_ _Post_ _Null_terminated_)

// [in,unique] 
#define __RPC__in_opt                                _SAL1_2_Source_(__RPC__in_opt, (), _Pre_ _Notref_ _Maybenull_ _Pre_ _Valid_)
#define __RPC__in_opt_string                         _SAL1_2_Source_(__RPC__in_opt_string, (), __RPC__in_opt  _Pre_ _Null_terminated_)
#define __RPC__in_ecount_opt(size)                   _SAL1_2_Source_(__RPC__in_ecount_opt, (size), __RPC__in_opt  _Pre_readable_size_(size))
#define __RPC__in_ecount_opt_string(size)            _SAL1_2_Source_(__RPC__in_ecount_opt_string, (size), __RPC__in_ecount_opt(size) _Pre_ _Null_terminated_)
#define __RPC__in_ecount_full_opt(size)              _SAL1_2_Source_(__RPC__in_ecount_full_opt, (size), __RPC__in_ecount_opt(size))
#define __RPC__in_ecount_full_opt_string(size)       _SAL1_2_Source_(__RPC__in_ecount_full_opt_string, (size), __RPC__in_ecount_full_opt(size) _Pre_ _Null_terminated_)
#define __RPC__in_ecount_part_opt(size, length)      _SAL1_2_Source_(__RPC__in_ecount_part_opt, (size,length), __RPC__in_ecount_opt(length) _Pre_writable_size_(size))
#define __RPC__in_xcount_full_opt(size)              _SAL1_2_Source_(__RPC__in_xcount_full_opt, (size), __RPC__in_ecount_opt(size))
#define __RPC__in_xcount_full_opt_string(size)       _SAL1_2_Source_(__RPC__in_xcount_full_opt_string, (size), __RPC__in_ecount_full_opt(size) _Pre_ _Null_terminated_)
#define __RPC__in_xcount_part_opt(size, length)      _SAL1_2_Source_(__RPC__in_xcount_part_opt, (size,length), __RPC__in_ecount_part_opt(size, length))
#define __RPC__in_xcount_opt(size)                   _SAL1_2_Source_(__RPC__in_xcount_opt, (size), __RPC__in_ecount_opt(size))
#define __RPC__in_xcount_opt_string(size)            _SAL1_2_Source_(__RPC__in_xcount_opt_string, (size), __RPC__in_ecount_opt(size) _Pre_ _Null_terminated_)

// [in,out,unique] 
#define __RPC__inout_opt                             _SAL1_2_Source_(__RPC__inout_opt, (), _Inout_opt_)
#define __RPC__inout_opt_string                      _SAL1_2_Source_(__RPC__inout_opt_string, (), __RPC__inout_opt  _Pre_ _Null_terminated_)
#define __RPC__inout_ecount_opt(size)                _SAL1_2_Source_(__RPC__inout_ecount_opt, (size), _Inout_updates_opt_(size))
#define __RPC__inout_ecount_part_opt(size, length)   _SAL1_2_Source_(__RPC__inout_ecount_part_opt, (size,length), _Inout_updates_to_opt_(size, length))
#define __RPC__inout_ecount_full_opt(size)           _SAL1_2_Source_(__RPC__inout_ecount_full_opt, (size), __RPC__inout_ecount_part_opt(size, size))
#define __RPC__inout_ecount_full_opt_string(size)    _SAL1_2_Source_(__RPC__inout_ecount_full_opt_string, (size), __RPC__inout_ecount_full_opt(size)  _Pre_ _Null_terminated_ _Post_ _Null_terminated_)
#define __RPC__inout_xcount_opt(size)                _SAL1_2_Source_(__RPC__inout_xcount_opt, (size), _Inout_opt_)
#define __RPC__inout_xcount_part_opt(size, length)   _SAL1_2_Source_(__RPC__inout_xcount_part_opt, (size,length), _Inout_opt_)
#define __RPC__inout_xcount_full_opt(size)           _SAL1_2_Source_(__RPC__inout_xcount_full_opt, (size), __RPC__inout_opt)
#define __RPC__inout_xcount_full_opt_string(size)    _SAL1_2_Source_(__RPC__inout_xcount_full_opt_string, (size), __RPC__inout_opt _Pre_ _Null_terminated_ _Post_ _Null_terminated_)

// [out] **
#define __RPC__deref_out                             _SAL1_2_Source_(__RPC__deref_out, (), _Outptr_)
#define __RPC__deref_out_string                      _SAL1_2_Source_(__RPC__deref_out_string, (), _Outptr_result_z_)
#define __RPC__deref_out_opt                        _SAL1_2_Source_(__RPC__deref_out_opt, (), __RPC__deref_out)
#define __RPC__deref_out_opt_string                  _SAL1_2_Source_(__RPC__deref_out_opt_string, (), _Outptr_result_maybenull_z_  _At_(*_Curr_, _Pre_opt_z_))
#define __RPC__deref_out_ecount(size)                _SAL1_2_Source_(__RPC__deref_out_ecount, (size), _Outptr_result_buffer_(size))
#define __RPC__deref_out_ecount_part(size, length)   _SAL1_2_Source_(__RPC__deref_out_ecount_part, (size,length), _Outptr_result_buffer_to_(size, length))
#define __RPC__deref_out_ecount_full(size)           _SAL1_2_Source_(__RPC__deref_out_ecount_full, (size), __RPC__deref_out_ecount_part(size,size))
#define __RPC__deref_out_ecount_full_string(size)    _SAL1_2_Source_(__RPC__deref_out_ecount_full_string, (size), __RPC__deref_out_ecount_full(size) _At_(*_Curr_, _Post_ _Null_terminated_))
#define __RPC__deref_out_xcount(size)                _SAL1_2_Source_(__RPC__deref_out_xcount, (size), _Outptr_)
#define __RPC__deref_out_xcount_part(size, length)   _SAL1_2_Source_(__RPC__deref_out_xcount_part, (size,length), __RPC__deref_out)
#define __RPC__deref_out_xcount_full(size)           _SAL1_2_Source_(__RPC__deref_out_xcount_full, (size), __RPC__deref_out)
#define __RPC__deref_out_xcount_full_string(size)    _SAL1_2_Source_(__RPC__deref_out_xcount_full_string, (size), __RPC__deref_out _At_(*_Curr_, _Post_ _Null_terminated_))

// [in,out] **, second pointer decoration. 
#define __RPC__deref_inout                           _SAL1_2_Source_(__RPC__deref_inout, (), _Inout_ _At_(*_Curr_, _Pre_ _Notnull_ _Post_ _Notnull_))
#define __RPC__deref_inout_string                    _SAL1_2_Source_(__RPC__deref_inout_string, (), __RPC__deref_inout _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_inout_opt                       _SAL1_2_Source_(__RPC__deref_inout_opt, (), _Inout_ _At_(*_Curr_, _Pre_ _Maybenull_ _Post_ _Maybenull_))
#define __RPC__deref_inout_opt_string                _SAL1_2_Source_(__RPC__deref_inout_opt_string, (), __RPC__deref_inout_opt _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_inout_ecount_opt(size)          _SAL1_2_Source_(__RPC__deref_inout_ecount_opt, (size), __RPC__deref_inout_opt _At_(*_Curr_, _Pre_writable_size_(size) _Post_writable_size_(size)))
#define __RPC__deref_inout_ecount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_inout_ecount_part_opt, (size,length), __RPC__deref_inout_ecount_opt(size) _At_(*_Curr_, _Pre_readable_size_(length) _Post_readable_size_(length)))
#define __RPC__deref_inout_ecount_full_opt(size)     _SAL1_2_Source_(__RPC__deref_inout_ecount_full_opt, (size), __RPC__deref_inout_ecount_part_opt(size, size))
#define __RPC__deref_inout_ecount_full(size)         _SAL1_2_Source_(__RPC__deref_inout_ecount_full, (size), __RPC__deref_inout _At_(*_Curr_, _Pre_readable_size_(size) _Post_readable_size_(size)))
#define __RPC__deref_inout_ecount_full_string(size)  _SAL1_2_Source_(__RPC__deref_inout_ecount_full_string, (size), __RPC__deref_inout_ecount_full(size) _At_(*_Curr_, _Post_ _Null_terminated_))
#define __RPC__deref_inout_ecount_full_opt_string(size)  _SAL1_2_Source_(__RPC__deref_inout_ecount_full_opt_string, (size), __RPC__deref_inout_ecount_full_opt(size) _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_inout_xcount_opt(size)          _SAL1_2_Source_(__RPC__deref_inout_xcount_opt, (size), __RPC__deref_inout_opt)
#define __RPC__deref_inout_xcount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_inout_xcount_part_opt, (size,length), __RPC__deref_inout_opt)
#define __RPC__deref_inout_xcount_full_opt(size)     _SAL1_2_Source_(__RPC__deref_inout_xcount_full_opt, (size), __RPC__deref_inout_opt)
#define __RPC__deref_inout_xcount_full(size)         _SAL1_2_Source_(__RPC__deref_inout_xcount_full, (size), __RPC__deref_inout)
#define __RPC__deref_inout_xcount_full_string(size)  _SAL1_2_Source_(__RPC__deref_inout_xcount_full_string, (size), __RPC__deref_inout _At_(*_Curr_, _Post_ _Null_terminated_))
#define __RPC__deref_inout_xcount_full_opt_string(size)  _SAL1_2_Source_(__RPC__deref_inout_xcount_full_opt_string, (size), __RPC__deref_inout_opt _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))


// #define __RPC_out_opt    out_opt is not allowed in rpc

// [in,out,unique] 
#define __RPC__deref_opt_inout                           _SAL1_2_Source_(__RPC__deref_opt_inout, (), _Inout_opt_ _At_(*_Curr_, _Pre_ _Notnull_ _Post_ _Notnull_))
#define __RPC__deref_opt_inout_ecount(size)              _SAL1_2_Source_(__RPC__deref_opt_inout_ecount, (size), __RPC__deref_opt_inout _At_(*_Curr_, _Pre_writable_size_(size) _Post_writable_size_(size)))
#define __RPC__deref_opt_inout_string                    _SAL1_2_Source_(__RPC__deref_opt_inout_string, (), __RPC__deref_opt_inout _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_opt_inout_ecount_part(size, length)  _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_part, (size,length), __RPC__deref_opt_inout_ecount(size) _At_(*_Curr_, _Pre_readable_size_(length) _Post_readable_size_(length)))
#define __RPC__deref_opt_inout_ecount_full(size)         _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_full, (size), __RPC__deref_opt_inout_ecount_part(size, size))
#define __RPC__deref_opt_inout_ecount_full_string(size)   _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_full_string, (size), __RPC__deref_opt_inout_ecount_full(size) _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_opt_inout_xcount_part(size, length)  _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_part, (size,length), __RPC__deref_opt_inout)
#define __RPC__deref_opt_inout_xcount_full(size)         _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_full, (size), __RPC__deref_opt_inout)
#define __RPC__deref_opt_inout_xcount_full_string(size)  _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_full_string, (size), __RPC__deref_opt_inout_string)


#define __RPC__deref_out_ecount_opt(size)                _SAL1_2_Source_(__RPC__deref_out_ecount_opt, (size), _Outptr_result_buffer_maybenull_(size) _At_(*_Curr_, _Pre_maybenull_))
#define __RPC__deref_out_ecount_part_opt(size, length)   _SAL1_2_Source_(__RPC__deref_out_ecount_part_opt, (size,length), _Outptr_result_buffer_to_maybenull_(size, length) _At_(*_Curr_, _Pre_maybenull_))
#define __RPC__deref_out_ecount_full_opt(size)           _SAL1_2_Source_(__RPC__deref_out_ecount_full_opt, (size), __RPC__deref_out_ecount_part_opt(size, size))
#define __RPC__deref_out_ecount_full_opt_string(size)    _SAL1_2_Source_(__RPC__deref_out_ecount_full_opt_string, (size), __RPC__deref_out_ecount_part_opt(size, size) _At_(*_Curr_, _Post_ _Null_terminated_))
#define __RPC__deref_out_xcount_opt(size)                _SAL1_2_Source_(__RPC__deref_out_xcount_opt, (size), __RPC__out _At_(*_Curr_, _Pre_maybenull_ _Pre_writable_size_(_Inexpressible_(size)) _Post_ _Maybenull_))
#define __RPC__deref_out_xcount_part_opt(size, length)   _SAL1_2_Source_(__RPC__deref_out_xcount_part_opt, (size,length), __RPC__deref_out _At_(*_Curr_, _Pre_maybenull_ _Pre_writable_size_(_Inexpressible_(size)) _Post_ _Maybenull_))
#define __RPC__deref_out_xcount_full_opt(size)           _SAL1_2_Source_(__RPC__deref_out_xcount_full_opt, (size), __RPC__deref_out_opt _At_(*_Curr_, _Pre_maybenull_ _Pre_writable_size_(_Inexpressible_(size))))
#define __RPC__deref_out_xcount_full_opt_string(size)    _SAL1_2_Source_(__RPC__deref_out_xcount_full_opt_string, (size), __RPC__deref_out_opt _At_(*_Curr_, _Pre_maybenull_ _Pre_writable_size_(_Inexpressible_(size)) _Post_ _Null_terminated_))

#define __RPC__deref_opt_inout_opt                       _SAL1_2_Source_(__RPC__deref_opt_inout_opt, (), _Inout_opt_ _At_(*_Curr_, _Pre_ _Maybenull_ _Post_ _Maybenull_))
#define __RPC__deref_opt_inout_opt_string                _SAL1_2_Source_(__RPC__deref_opt_inout_opt_string, (), __RPC__deref_opt_inout_opt _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_opt_inout_ecount_opt(size)          _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_opt, (size), _Inout_opt_ _At_(*_Curr_, _Pre_ _Maybenull_ _Pre_writable_size_(size) _Post_ _Maybenull_ _Post_writable_size_(size)))
#define __RPC__deref_opt_inout_ecount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_part_opt, (size,length), __RPC__deref_opt_inout_ecount_opt(size) _At_(*_Curr_, _Pre_readable_size_(length) _Post_readable_size_(length)))
#define __RPC__deref_opt_inout_ecount_full_opt(size)     _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_full_opt, (size), __RPC__deref_opt_inout_ecount_part_opt(size, size))
#define __RPC__deref_opt_inout_ecount_full_opt_string(size)   _SAL1_2_Source_(__RPC__deref_opt_inout_ecount_full_opt_string, (size), __RPC__deref_opt_inout_ecount_full_opt(size) _At_(*_Curr_, _Pre_ _Null_terminated_ _Post_ _Null_terminated_))
#define __RPC__deref_opt_inout_xcount_opt(size)          _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_opt, (size), __RPC__deref_opt_inout_opt)
#define __RPC__deref_opt_inout_xcount_part_opt(size, length)  _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_part_opt, (size,length), __RPC__deref_opt_inout_opt)
#define __RPC__deref_opt_inout_xcount_full_opt(size)     _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_full_opt, (size), __RPC__deref_opt_inout_opt)
#define __RPC__deref_opt_inout_xcount_full_opt_string(size)   _SAL1_2_Source_(__RPC__deref_opt_inout_xcount_full_opt_string, (size), __RPC__deref_opt_inout_opt_string)

#define __RPC_full_pointer                               _SAL1_2_Source_(__RPC_full_pointer, (), _Maybenull_)
#define __RPC_unique_pointer                             _SAL1_2_Source_(__RPC_unique_pointer, (), _Maybenull_)
#define __RPC_ref_pointer                                _SAL1_2_Source_(__RPC_ref_pointer, (), _Notnull_)
#define __RPC_string                                     _SAL1_2_Source_(__RPC_string, (), _Null_terminated_)

#define __RPC__range(min,max)                            _SAL1_2_Source_(__RPC__range, (min,max), __range(min,max))
#define __RPC__in_range(min,max)                         _SAL1_2_Source_(__RPC__in_range, (min,max), _In_range_(min,max))


#ifdef  __cplusplus
}
#endif

