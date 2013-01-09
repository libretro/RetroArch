/*
   xaudio.h (2010-08-14) / (2012-12-04)
   authors: OV2, Themaister
*/

// Kinda stripped down. Only contains the bare essentials used in RetroArch.

#ifndef XAUDIO2_XDK360_H
#define XAUDIO2_XDK360_H

#include <comdecl.h>        // For DEFINE_CLSID and DEFINE_IID

DEFINE_CLSID(XAudio2, 3eda9b49, 2085, 498b, 9b, b2, 39, a6, 77, 84, 93, de);
DEFINE_CLSID(XAudio2_Debug, 47199894, 7cc2, 444d, 98, 73, ce, d2, 56, 2c, c6, 0e);
DEFINE_IID(IXAudio2, 8bcf1f58, 9fe7, 4583, 8a, c6, e2, ad, c4, 65, c8, bb);

#include <sal.h>

#if _MSC_VER > 1000
#pragma once
#endif

#define __RPC_FAR

typedef unsigned char byte;
typedef unsigned char boolean;

typedef double DOUBLE;

#ifdef CONST_VTABLE
#define CONST_VTBL const
#else
#define CONST_VTBL
#endif

/****************************************************************************
 * Special things for VC5 Com support
 ****************************************************************************/

#ifndef DECLSPEC_SELECTANY
#if (_MSC_VER >= 1100)
#define DECLSPEC_SELECTANY __declspec(selectany)
#else
#define DECLSPEC_SELECTANY
#endif
#endif

#ifndef DECLSPEC_NOVTABLE
#define DECLSPEC_NOVTABLE
#endif

#ifndef DECLSPEC_UUID
#define DECLSPEC_UUID(x)
#endif

#define MIDL_INTERFACE(x)   struct DECLSPEC_UUID(x) DECLSPEC_NOVTABLE

#if _MSC_VER >= 1100
#define EXTERN_GUID(itf,l1,s1,s2,c1,c2,c3,c4,c5,c6,c7,c8)  \
  EXTERN_C const IID DECLSPEC_SELECTANY itf = {l1,s1,s2,{c1,c2,c3,c4,c5,c6,c7,c8}}
#else
#define EXTERN_GUID(itf,l1,s1,s2,c1,c2,c3,c4,c5,c6,c7,c8) EXTERN_C const IID itf
#endif

#include <pshpack8.h>

#define WINOLEAPI        EXTERN_C DECLSPEC_IMPORT HRESULT STDAPICALLTYPE
#define WINOLEAPI_(type) EXTERN_C DECLSPEC_IMPORT type STDAPICALLTYPE

/****** Interface Declaration ***********************************************/

/*
 *      These are macros for declaring interfaces.  They exist so that
 *      a single definition of the interface is simulataneously a proper
 *      declaration of the interface structures (C++ abstract classes)
 *      for both C and C++.
 *
 *      DECLARE_INTERFACE(iface) is used to declare an interface that does
 *      not derive from a base interface.
 *      DECLARE_INTERFACE_(iface, baseiface) is used to declare an interface
 *      that does derive from a base interface.
 *
 *      By default if the source file has a .c extension the C version of
 *      the interface declaratations will be expanded; if it has a .cpp
 *      extension the C++ version will be expanded. if you want to force
 *      the C version expansion even though the source file has a .cpp
 *      extension, then define the macro "CINTERFACE".
 *      eg.     cl -DCINTERFACE file.cpp
 *
 *
 *      Example C expansion:
 *
 *          typedef struct IClassFactory
 *          {
 *              const struct IClassFactoryVtbl FAR* lpVtbl;
 *          } IClassFactory;
 *
 *          typedef struct IClassFactoryVtbl IClassFactoryVtbl;
 *
 *          struct IClassFactoryVtbl
 *          {
 *              HRESULT (STDMETHODCALLTYPE * QueryInterface) (
 *                                                  IClassFactory FAR* This,
 *                                                  IID FAR* riid,
 *                                                  LPVOID FAR* ppvObj) ;
 *              ULONG (STDMETHODCALLTYPE * AddRef) (IClassFactory FAR* This) ;
 *              ULONG (STDMETHODCALLTYPE * Release) (IClassFactory FAR* This) ;
 *              HRESULT (STDMETHODCALLTYPE * CreateInstance) (
 *                                                  IClassFactory FAR* This,
 *                                                  LPUNKNOWN pUnkOuter,
 *                                                  IID FAR* riid,
 *                                                  LPVOID FAR* ppvObject);
 *              HRESULT (STDMETHODCALLTYPE * LockServer) (
 *                                                  IClassFactory FAR* This,
 *                                                  BOOL fLock);
 *          };
 */

#define interface               struct

#define STDMETHOD(method)       HRESULT (STDMETHODCALLTYPE * method)
#define STDMETHOD_(type,method) type (STDMETHODCALLTYPE * method)
#define STDMETHODV(method)       HRESULT (STDMETHODVCALLTYPE * method)
#define STDMETHODV_(type,method) type (STDMETHODVCALLTYPE * method)

#if !defined(BEGIN_INTERFACE)
    #define BEGIN_INTERFACE
    #define END_INTERFACE
#endif


#define PURE
#define THIS_                   INTERFACE FAR* This,
#define THIS                    INTERFACE FAR* This
#ifdef CONST_VTABLE
#undef CONST_VTBL
#define CONST_VTBL const
#define DECLARE_INTERFACE(iface)    typedef interface iface { \
                                    const struct iface##Vtbl FAR* lpVtbl; \
                                } iface; \
                                typedef const struct iface##Vtbl iface##Vtbl; \
                                const struct iface##Vtbl
#else
#undef CONST_VTBL
#define CONST_VTBL
#define DECLARE_INTERFACE(iface)    typedef interface iface { \
                                    struct iface##Vtbl FAR* lpVtbl; \
                                } iface; \
                                typedef struct iface##Vtbl iface##Vtbl; \
                                struct iface##Vtbl
#endif
#define DECLARE_INTERFACE_(iface, baseiface)    DECLARE_INTERFACE(iface)





/****** Additional basic types **********************************************/


#ifndef FARSTRUCT
#define FARSTRUCT
#endif  // FARSTRUCT



#ifndef HUGEP
#if defined(_WIN32) || defined(_MPPC_)
#define HUGEP
#else
#define HUGEP __huge
#endif // WIN32
#endif // HUGEP


#include <stdlib.h>

#define LISet32(li, v) ((li).HighPart = (v) < 0 ? -1 : 0, (li).LowPart = (v))

#define ULISet32(li, v) ((li).HighPart = 0, (li).LowPart = (v))


#if defined(_WIN32) && !defined(OLE2ANSI)
typedef WCHAR OLECHAR;

typedef /* [string] */ OLECHAR __RPC_FAR *LPOLESTR;

typedef /* [string] */ const OLECHAR __RPC_FAR *LPCOLESTR;

#define OLESTR(str) L##str

#else

typedef char      OLECHAR;
typedef LPSTR     LPOLESTR;
typedef LPCSTR    LPCOLESTR;
#define OLESTR(str) str
#endif


/* Forward Declarations */ 

#ifndef __IUnknown_FWD_DEFINED__
#define __IUnknown_FWD_DEFINED__
typedef interface IUnknown IUnknown;
#endif 	/* __IUnknown_FWD_DEFINED__ */


#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__


/* interface IUnknown */
/* [unique][uuid][object][local] */ 

typedef /* [unique] */ IUnknown __RPC_FAR *LPUNKNOWN;
typedef /* [unique] */ IUnknown __RPC_FAR *PUNKNOWN;

//////////////////////////////////////////////////////////////////
// IID_IUnknown and all other system IIDs are provided in UUID.LIB
// Link that library in with your proxies, clients and servers
//////////////////////////////////////////////////////////////////

EXTERN_C const IID IID_IUnknown;

    typedef struct IUnknownVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IUnknown __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IUnknown __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IUnknown __RPC_FAR * This);
        
        END_INTERFACE
    } IUnknownVtbl;

    interface IUnknown
    {
        CONST_VTBL struct IUnknownVtbl __RPC_FAR *lpVtbl;
    };


#define IUnknown_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IUnknown_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IUnknown_Release(This)	\
    (This)->lpVtbl -> Release(This)






/* interface __MIDL_itf_unknwn_0005 */
/* [local] */ 

#endif // VC6 hack


#ifndef __ISequentialStream_FWD_DEFINED__
#define __ISequentialStream_FWD_DEFINED__
typedef interface ISequentialStream ISequentialStream;
#endif 	/* __ISequentialStream_FWD_DEFINED__ */


#ifndef __ISequentialStream_INTERFACE_DEFINED__
#define __ISequentialStream_INTERFACE_DEFINED__

/* interface ISequentialStream */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID_ISequentialStream;

    typedef struct ISequentialStreamVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            ISequentialStream __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            ISequentialStream __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            ISequentialStream __RPC_FAR * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Read )( 
            ISequentialStream __RPC_FAR * This,
            /* [length_is][size_is][out] */ __out_bcount_part_opt(cb, *pcbRead) void __RPC_FAR *pv,
            /* [in] */ __in ULONG cb,
            /* [out] */ __out_opt ULONG __RPC_FAR *pcbRead);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Write )( 
            ISequentialStream __RPC_FAR * This,
            /* [size_is][in] */ __in_bcount_opt(cb) const void __RPC_FAR *pv,
            /* [in] */ __in ULONG cb,
            /* [out] */ __out_opt ULONG __RPC_FAR *pcbWritten);
        
        END_INTERFACE
    } ISequentialStreamVtbl;

    interface ISequentialStream
    {
        CONST_VTBL struct ISequentialStreamVtbl __RPC_FAR *lpVtbl;
    };


#define ISequentialStream_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISequentialStream_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISequentialStream_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISequentialStream_Read(This,pv,cb,pcbRead)	\
    (This)->lpVtbl -> Read(This,pv,cb,pcbRead)

#define ISequentialStream_Write(This,pv,cb,pcbWritten)	\
    (This)->lpVtbl -> Write(This,pv,cb,pcbWritten)


#endif 	/* __ISequentialStream_INTERFACE_DEFINED__ */



#ifndef __IStream_FWD_DEFINED__
#define __IStream_FWD_DEFINED__
typedef interface IStream IStream;
#endif 	/* __IStream_FWD_DEFINED__ */


#ifndef __IStream_INTERFACE_DEFINED__
#define __IStream_INTERFACE_DEFINED__

/* interface IStream */
/* [unique][uuid][object] */ 

typedef /* [unique] */ IStream __RPC_FAR *LPSTREAM;

typedef struct tagSTATSTG
    {
    LPOLESTR pwcsName;
    DWORD type;
    ULARGE_INTEGER cbSize;
    FILETIME mtime;
    FILETIME ctime;
    FILETIME atime;
    DWORD grfMode;
    DWORD grfLocksSupported;
    CLSID clsid;
    DWORD grfStateBits;
    DWORD reserved;
    }	STATSTG;

typedef 
enum tagSTGTY
    {	STGTY_STORAGE	= 1,
	STGTY_STREAM	= 2,
	STGTY_LOCKBYTES	= 3,
	STGTY_PROPERTY	= 4
    }	STGTY;

typedef 
enum tagSTREAM_SEEK
    {	STREAM_SEEK_SET	= 0,
	STREAM_SEEK_CUR	= 1,
	STREAM_SEEK_END	= 2
    }	STREAM_SEEK;

typedef 
enum tagLOCKTYPE
    {	LOCK_WRITE	= 1,
	LOCK_EXCLUSIVE	= 2,
	LOCK_ONLYONCE	= 4
    }	LOCKTYPE;


EXTERN_C const IID IID_IStream;


    typedef struct IStreamVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IStream __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IStream __RPC_FAR * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Read )( 
            IStream __RPC_FAR * This,
            /* [length_is][size_is][out] */ __out_bcount_part_opt(cb, *pcbRead) void __RPC_FAR *pv,
            /* [in] */ __in ULONG cb,
            /* [out] */ __out_opt ULONG __RPC_FAR *pcbRead);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Write )( 
            IStream __RPC_FAR * This,
            /* [size_is][in] */ __in_bcount_opt(cb) const void __RPC_FAR *pv,
            /* [in] */ __in ULONG cb,
            /* [out] */ __out_opt ULONG __RPC_FAR *pcbWritten);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Seek )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in LARGE_INTEGER dlibMove,
            /* [in] */ __in DWORD dwOrigin,
            /* [out] */ __out_opt ULARGE_INTEGER __RPC_FAR *plibNewPosition);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *SetSize )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in ULARGE_INTEGER libNewSize);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CopyTo )( 
            IStream __RPC_FAR * This,
            /* [unique][in] */ __in IStream __RPC_FAR *pstm,
            /* [in] */ __in ULARGE_INTEGER cb,
            /* [out] */ __out_opt ULARGE_INTEGER __RPC_FAR *pcbRead,
            /* [out] */ __out_opt ULARGE_INTEGER __RPC_FAR *pcbWritten);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Commit )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in DWORD grfCommitFlags);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Revert )( 
            IStream __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *LockRegion )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in ULARGE_INTEGER libOffset,
            /* [in] */ __in ULARGE_INTEGER cb,
            /* [in] */ __in DWORD dwLockType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *UnlockRegion )( 
            IStream __RPC_FAR * This,
            /* [in] */ __in ULARGE_INTEGER libOffset,
            /* [in] */ __in ULARGE_INTEGER cb,
            /* [in] */ __in DWORD dwLockType);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Stat )( 
            IStream __RPC_FAR * This,
            /* [out] */ __out STATSTG __RPC_FAR *pstatstg,
            /* [in] */ __in DWORD grfStatFlag);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Clone )( 
            IStream __RPC_FAR * This,
            /* [out] */ __deref_out_opt IStream __RPC_FAR *__RPC_FAR *ppstm);
        
        END_INTERFACE
    } IStreamVtbl;

    interface IStream
    {
        CONST_VTBL struct IStreamVtbl __RPC_FAR *lpVtbl;
    };


#define IStream_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IStream_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IStream_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IStream_Read(This,pv,cb,pcbRead)	\
    (This)->lpVtbl -> Read(This,pv,cb,pcbRead)

#define IStream_Write(This,pv,cb,pcbWritten)	\
    (This)->lpVtbl -> Write(This,pv,cb,pcbWritten)


#define IStream_Seek(This,dlibMove,dwOrigin,plibNewPosition)	\
    (This)->lpVtbl -> Seek(This,dlibMove,dwOrigin,plibNewPosition)

#define IStream_SetSize(This,libNewSize)	\
    (This)->lpVtbl -> SetSize(This,libNewSize)

#define IStream_CopyTo(This,pstm,cb,pcbRead,pcbWritten)	\
    (This)->lpVtbl -> CopyTo(This,pstm,cb,pcbRead,pcbWritten)

#define IStream_Commit(This,grfCommitFlags)	\
    (This)->lpVtbl -> Commit(This,grfCommitFlags)

#define IStream_Revert(This)	\
    (This)->lpVtbl -> Revert(This)

#define IStream_LockRegion(This,libOffset,cb,dwLockType)	\
    (This)->lpVtbl -> LockRegion(This,libOffset,cb,dwLockType)

#define IStream_UnlockRegion(This,libOffset,cb,dwLockType)	\
    (This)->lpVtbl -> UnlockRegion(This,libOffset,cb,dwLockType)

#define IStream_Stat(This,pstatstg,grfStatFlag)	\
    (This)->lpVtbl -> Stat(This,pstatstg,grfStatFlag)

#define IStream_Clone(This,ppstm)	\
    (This)->lpVtbl -> Clone(This,ppstm)




#endif 	/* __IStream_INTERFACE_DEFINED__ */


#ifndef __IClassFactory_FWD_DEFINED__
#define __IClassFactory_FWD_DEFINED__
typedef interface IClassFactory IClassFactory;
#endif 	/* __IClassFactory_FWD_DEFINED__ */


#ifndef __IClassFactory_INTERFACE_DEFINED__
#define __IClassFactory_INTERFACE_DEFINED__

/* interface IClassFactory */
/* [unique][uuid][object] */ 

typedef /* [unique] */ IClassFactory __RPC_FAR *LPCLASSFACTORY;


EXTERN_C const IID IID_IClassFactory;


    typedef struct IClassFactoryVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IClassFactory __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IClassFactory __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IClassFactory __RPC_FAR * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *CreateInstance )( 
            IClassFactory __RPC_FAR * This,
            /* [unique][in] */ __in_opt IUnknown __RPC_FAR *pUnkOuter,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *LockServer )( 
            IClassFactory __RPC_FAR * This,
            /* [in] */ __in BOOL fLock);
        
        END_INTERFACE
    } IClassFactoryVtbl;

    interface IClassFactory
    {
        CONST_VTBL struct IClassFactoryVtbl __RPC_FAR *lpVtbl;
    };



#define IClassFactory_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IClassFactory_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IClassFactory_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IClassFactory_CreateInstance(This,pUnkOuter,riid,ppvObject)	\
    (This)->lpVtbl -> CreateInstance(This,pUnkOuter,riid,ppvObject)

#define IClassFactory_LockServer(This,fLock)	\
    (This)->lpVtbl -> LockServer(This,fLock)





#endif 	/* __IClassFactory_INTERFACE_DEFINED__ */



#ifndef __IPersist_FWD_DEFINED__
#define __IPersist_FWD_DEFINED__
typedef interface IPersist IPersist;
#endif 	/* __IPersist_FWD_DEFINED__ */


#ifndef __IPersist_INTERFACE_DEFINED__
#define __IPersist_INTERFACE_DEFINED__

/* interface IPersist */
/* [uuid][object] */ 

typedef /* [unique] */ IPersist __RPC_FAR *LPPERSIST;


EXTERN_C const IID IID_IPersist;


    typedef struct IPersistVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IPersist __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IPersist __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IPersist __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetClassID )( 
            IPersist __RPC_FAR * This,
            /* [out] */ __out CLSID __RPC_FAR *pClassID);
        
        END_INTERFACE
    } IPersistVtbl;

    interface IPersist
    {
        CONST_VTBL struct IPersistVtbl __RPC_FAR *lpVtbl;
    };

#define IPersist_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IPersist_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IPersist_Release(This)	\
    (This)->lpVtbl -> Release(This)

#define IPersist_GetClassID(This,pClassID)	\
    (This)->lpVtbl -> GetClassID(This,pClassID)

#endif 	/* __IPersist_INTERFACE_DEFINED__ */

#ifndef __IPersistStream_FWD_DEFINED__
#define __IPersistStream_FWD_DEFINED__
typedef interface IPersistStream IPersistStream;
#endif 	/* __IPersistStream_FWD_DEFINED__ */

#ifndef __IPersistStream_INTERFACE_DEFINED__
#define __IPersistStream_INTERFACE_DEFINED__

/* interface IPersistStream */
/* [unique][uuid][object] */ 

typedef /* [unique] */ IPersistStream __RPC_FAR *LPPERSISTSTREAM;


EXTERN_C const IID IID_IPersistStream;

    typedef struct IPersistStreamVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IPersistStream __RPC_FAR * This,
            /* [in] */ __in REFIID riid,
            /* [iid_is][out] */ __deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IPersistStream __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IPersistStream __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetClassID )( 
            IPersistStream __RPC_FAR * This,
            /* [out] */ __out CLSID __RPC_FAR *pClassID);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *IsDirty )( 
            IPersistStream __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Load )( 
            IPersistStream __RPC_FAR * This,
            /* [unique][in] */ __in_opt IStream __RPC_FAR *pStm);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Save )( 
            IPersistStream __RPC_FAR * This,
            /* [unique][in] */ __in_opt IStream __RPC_FAR *pStm,
            /* [in] */ __in BOOL fClearDirty);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetSizeMax )( 
            IPersistStream __RPC_FAR * This,
            /* [out] */ __out ULARGE_INTEGER __RPC_FAR *pcbSize);
        
        END_INTERFACE
    } IPersistStreamVtbl;

    interface IPersistStream
    {
        CONST_VTBL struct IPersistStreamVtbl __RPC_FAR *lpVtbl;
    };



#define IPersistStream_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IPersistStream_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IPersistStream_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IPersistStream_GetClassID(This,pClassID)	\
    (This)->lpVtbl -> GetClassID(This,pClassID)


#define IPersistStream_IsDirty(This)	\
    (This)->lpVtbl -> IsDirty(This)

#define IPersistStream_Load(This,pStm)	\
    (This)->lpVtbl -> Load(This,pStm)

#define IPersistStream_Save(This,pStm,fClearDirty)	\
    (This)->lpVtbl -> Save(This,pStm,fClearDirty)

#define IPersistStream_GetSizeMax(This,pcbSize)	\
    (This)->lpVtbl -> GetSizeMax(This,pcbSize)

#endif 	/* __IPersistStream_INTERFACE_DEFINED__ */

#include <guiddef.h>

#ifndef INITGUID
#include <cguid.h>
#endif

#ifndef RC_INVOKED
#include <poppack.h>
#endif // RC_INVOKED

#include <audiodefs.h>      // Basic audio data types and constants
#include <xma2defs.h>       // Data types and constants for XMA2 audio

// All structures defined in this file use tight field packing
#pragma pack(push, 1)

// Numeric boundary values
#define XAUDIO2_MAX_BUFFER_BYTES        0x80000000    // Maximum bytes allowed in a source buffer
#define XAUDIO2_MAX_QUEUED_BUFFERS      64            // Maximum buffers allowed in a voice queue
#define XAUDIO2_MAX_BUFFERS_SYSTEM      2             // Maximum buffers allowed for system threads (Xbox 360 only)
#define XAUDIO2_MAX_AUDIO_CHANNELS      64            // Maximum channels in an audio stream
#define XAUDIO2_MIN_SAMPLE_RATE         1000          // Minimum audio sample rate supported
#define XAUDIO2_MAX_SAMPLE_RATE         200000        // Maximum audio sample rate supported
#define XAUDIO2_MAX_VOLUME_LEVEL        16777216.0f   // Maximum acceptable volume level (2^24)
#define XAUDIO2_MIN_FREQ_RATIO          (1/1024.0f)   // Minimum SetFrequencyRatio argument
#define XAUDIO2_MAX_FREQ_RATIO          1024.0f       // Maximum MaxFrequencyRatio argument
#define XAUDIO2_DEFAULT_FREQ_RATIO      2.0f          // Default MaxFrequencyRatio argument
#define XAUDIO2_MAX_FILTER_ONEOVERQ     1.5f          // Maximum XAUDIO2_FILTER_PARAMETERS.OneOverQ
#define XAUDIO2_MAX_FILTER_FREQUENCY    1.0f          // Maximum XAUDIO2_FILTER_PARAMETERS.Frequency
#define XAUDIO2_MAX_LOOP_COUNT          254           // Maximum non-infinite XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_MAX_INSTANCES           8             // Maximum simultaneous XAudio2 objects on Xbox 360

#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MONO         600000
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MULTICHANNEL 300000

// Numeric values with special meanings
#define XAUDIO2_COMMIT_NOW              0             // Used as an OperationSet argument
#define XAUDIO2_COMMIT_ALL              0             // Used in IXAudio2::CommitChanges
#define XAUDIO2_INVALID_OPSET           (UINT32)(-1)  // Not allowed for OperationSet arguments
#define XAUDIO2_NO_LOOP_REGION          0             // Used in XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_LOOP_INFINITE           255           // Used in XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_DEFAULT_CHANNELS        0             // Used in CreateMasteringVoice
#define XAUDIO2_DEFAULT_SAMPLERATE      0             // Used in CreateMasteringVoice

// Flags
#define XAUDIO2_DEBUG_ENGINE            0x0001        // Used in XAudio2Create on Windows only
#define XAUDIO2_VOICE_NOPITCH           0x0002        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_VOICE_NOSRC             0x0004        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_VOICE_USEFILTER         0x0008        // Used in IXAudio2::CreateSource/SubmixVoice
#define XAUDIO2_VOICE_MUSIC             0x0010        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_PLAY_TAILS              0x0020        // Used in IXAudio2SourceVoice::Stop
#define XAUDIO2_END_OF_STREAM           0x0040        // Used in XAUDIO2_BUFFER.Flags
#define XAUDIO2_SEND_USEFILTER          0x0080        // Used in XAUDIO2_SEND_DESCRIPTOR.Flags

// Default parameters for the built-in filter
#define XAUDIO2_DEFAULT_FILTER_TYPE     LowPassFilter
#define XAUDIO2_DEFAULT_FILTER_FREQUENCY XAUDIO2_MAX_FILTER_FREQUENCY
#define XAUDIO2_DEFAULT_FILTER_ONEOVERQ 1.0f

// Internal XAudio2 constants
#define XAUDIO2_QUANTUM_NUMERATOR   2             // On Xbox 360, XAudio2 processes audio
#define XAUDIO2_QUANTUM_DENOMINATOR 375           //  in 5.333ms chunks (= 2/375 seconds)
#define XAUDIO2_QUANTUM_MS (1000.0f * XAUDIO2_QUANTUM_NUMERATOR / XAUDIO2_QUANTUM_DENOMINATOR)

// XAudio2 error codes
#define FACILITY_XAUDIO2 0x896
#define XAUDIO2_E_INVALID_CALL          0x88960001    // An API call or one of its arguments was illegal
#define XAUDIO2_E_XMA_DECODER_ERROR     0x88960002    // The XMA hardware suffered an unrecoverable error
#define XAUDIO2_E_XAPO_CREATION_FAILED  0x88960003    // XAudio2 failed to initialize an XAPO effect
#define XAUDIO2_E_DEVICE_INVALIDATED    0x88960004    // An audio device became unusable (unplugged, etc)

#define FWD_DECLARE(x) typedef interface x x

FWD_DECLARE(IXAudio2);
FWD_DECLARE(IXAudio2Voice);
FWD_DECLARE(IXAudio2SourceVoice);
FWD_DECLARE(IXAudio2SubmixVoice);
FWD_DECLARE(IXAudio2MasteringVoice);
FWD_DECLARE(IXAudio2EngineCallback);
FWD_DECLARE(IXAudio2VoiceCallback);

typedef enum XAUDIO2_XBOX_HWTHREAD_SPECIFIER
{
    XboxThread0 = 0x01,
    XboxThread1 = 0x02,
    XboxThread2 = 0x04,
    XboxThread3 = 0x08,
    XboxThread4 = 0x10,
    XboxThread5 = 0x20,
    XAUDIO2_ANY_PROCESSOR = XboxThread4,
    XAUDIO2_DEFAULT_PROCESSOR = XAUDIO2_ANY_PROCESSOR
} XAUDIO2_XBOX_HWTHREAD_SPECIFIER, XAUDIO2_PROCESSOR;

typedef enum XAUDIO2_DEVICE_ROLE
{
    NotDefaultDevice            = 0x0,
    DefaultConsoleDevice        = 0x1,
    DefaultMultimediaDevice     = 0x2,
    DefaultCommunicationsDevice = 0x4,
    DefaultGameDevice           = 0x8,
    GlobalDefaultDevice         = 0xf,
    InvalidDeviceRole = ~GlobalDefaultDevice
} XAUDIO2_DEVICE_ROLE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
    WCHAR DeviceID[256];                // String identifier for the audio device.
    WCHAR DisplayName[256];             // Friendly name suitable for display to a human.
    XAUDIO2_DEVICE_ROLE Role;           // Roles that the device should be used for.
    WAVEFORMATEXTENSIBLE OutputFormat;  // The device's native PCM audio output format.
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS
{
    UINT32 CreationFlags;               // Flags the voice was created with.
    UINT32 InputChannels;               // Channels in the voice's input audio.
    UINT32 InputSampleRate;             // Sample rate of the voice's input audio.
} XAUDIO2_VOICE_DETAILS;

// Used in XAUDIO2_VOICE_SENDS below
typedef struct XAUDIO2_SEND_DESCRIPTOR
{
    UINT32 Flags;                       // Either 0 or XAUDIO2_SEND_USEFILTER.
    IXAudio2Voice* pOutputVoice;        // This send's destination voice.
} XAUDIO2_SEND_DESCRIPTOR;

// Used in the voice creation functions and in IXAudio2Voice::SetOutputVoices
typedef struct XAUDIO2_VOICE_SENDS
{
    UINT32 SendCount;                   // Number of sends from this voice.
    XAUDIO2_SEND_DESCRIPTOR* pSends;    // Array of SendCount send descriptors.
} XAUDIO2_VOICE_SENDS;

// Used in XAUDIO2_EFFECT_CHAIN below
typedef struct XAUDIO2_EFFECT_DESCRIPTOR
{
    IUnknown* pEffect;                  // Pointer to the effect object's IUnknown interface.
    BOOL InitialState;                  // TRUE if the effect should begin in the enabled state.
    UINT32 OutputChannels;              // How many output channels the effect should produce.
} XAUDIO2_EFFECT_DESCRIPTOR;

// Used in the voice creation functions and in IXAudio2Voice::SetEffectChain
typedef struct XAUDIO2_EFFECT_CHAIN
{
    UINT32 EffectCount;                 // Number of effects in this voice's effect chain.
    XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors; // Array of effect descriptors.
} XAUDIO2_EFFECT_CHAIN;

// Used in XAUDIO2_FILTER_PARAMETERS below
typedef enum XAUDIO2_FILTER_TYPE
{
    LowPassFilter,                      // Attenuates frequencies above the cutoff frequency.
    BandPassFilter,                     // Attenuates frequencies outside a given range.
    HighPassFilter,                     // Attenuates frequencies below the cutoff frequency.
    NotchFilter                         // Attenuates frequencies inside a given range.
} XAUDIO2_FILTER_TYPE;

// Used in IXAudio2Voice::Set/GetFilterParameters and Set/GetOutputFilterParameters
typedef struct XAUDIO2_FILTER_PARAMETERS
{
    XAUDIO2_FILTER_TYPE Type;           // Low-pass, band-pass or high-pass.
    float Frequency;                    // Radian frequency (2 * sin(pi*CutoffFrequency/SampleRate));
                                        //  must be >= 0 and <= XAUDIO2_MAX_FILTER_FREQUENCY
                                        //  (giving a maximum CutoffFrequency of SampleRate/6).
    float OneOverQ;                     // Reciprocal of the filter's quality factor Q;
                                        //  must be > 0 and <= XAUDIO2_MAX_FILTER_ONEOVERQ.
} XAUDIO2_FILTER_PARAMETERS;

// Used in IXAudio2SourceVoice::SubmitSourceBuffer
typedef struct XAUDIO2_BUFFER
{
    UINT32 Flags;                       // Either 0 or XAUDIO2_END_OF_STREAM.
    UINT32 AudioBytes;                  // Size of the audio data buffer in bytes.
    const BYTE* pAudioData;             // Pointer to the audio data buffer.
    UINT32 PlayBegin;                   // First sample in this buffer to be played.
    UINT32 PlayLength;                  // Length of the region to be played in samples,
                                        //  or 0 to play the whole buffer.
    UINT32 LoopBegin;                   // First sample of the region to be looped.
    UINT32 LoopLength;                  // Length of the desired loop region in samples,
                                        //  or 0 to loop the entire buffer.
    UINT32 LoopCount;                   // Number of times to repeat the loop region,
                                        //  or XAUDIO2_LOOP_INFINITE to loop forever.
    void* pContext;                     // Context value to be passed back in callbacks.
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_BUFFER_WMA
{
    const UINT32* pDecodedPacketCumulativeBytes; // Decoded packet's cumulative size array.
                                                 //  Each element is the number of bytes accumulated
                                                 //  when the corresponding XWMA packet is decoded in
                                                 //  order.  The array must have PacketCount elements.
    UINT32 PacketCount;                          // Number of XWMA packets submitted. Must be >= 1 and
                                                 //  divide evenly into XAUDIO2_BUFFER.AudioBytes.
} XAUDIO2_BUFFER_WMA;

// Returned by IXAudio2SourceVoice::GetState
typedef struct XAUDIO2_VOICE_STATE
{
    void* pCurrentBufferContext;        // The pContext value provided in the XAUDIO2_BUFFER
                                        //  that is currently being processed, or NULL if
                                        //  there are no buffers in the queue.
    UINT32 BuffersQueued;               // Number of buffers currently queued on the voice
                                        //  (including the one that is being processed).
    UINT64 SamplesPlayed;               // Total number of samples produced by the voice since
                                        //  it began processing the current audio stream.
} XAUDIO2_VOICE_STATE;

#define XAUDIO2_LOG_ERRORS     0x0001   // For handled errors with serious effects.
#define XAUDIO2_LOG_WARNINGS   0x0002   // For handled errors that may be recoverable.
#define XAUDIO2_LOG_INFO       0x0004   // Informational chit-chat (e.g. state changes).
#define XAUDIO2_LOG_DETAIL     0x0008   // More detailed chit-chat.
#define XAUDIO2_LOG_API_CALLS  0x0010   // Public API function entries and exits.
#define XAUDIO2_LOG_FUNC_CALLS 0x0020   // Internal function entries and exits.
#define XAUDIO2_LOG_TIMING     0x0040   // Delays detected and other timing data.
#define XAUDIO2_LOG_LOCKS      0x0080   // Usage of critical sections and mutexes.
#define XAUDIO2_LOG_MEMORY     0x0100   // Memory heap usage information.
#define XAUDIO2_LOG_STREAMING  0x1000   // Audio streaming information.

#define X2DEFAULT(x)

#undef INTERFACE
#define INTERFACE IXAudio2
DECLARE_INTERFACE_(IXAudio2, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, __deref_out void** ppvInterface) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS) PURE;
    STDMETHOD_(ULONG, Release) (THIS) PURE;
    STDMETHOD(GetDeviceCount) (THIS_ __out UINT32* pCount) PURE;
    STDMETHOD(GetDeviceDetails) (THIS_ UINT32 Index, __out XAUDIO2_DEVICE_DETAILS* pDeviceDetails) PURE;
    STDMETHOD(Initialize) (THIS_ UINT32 Flags X2DEFAULT(0),
                           XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR)) PURE;
    STDMETHOD(RegisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) PURE;
    STDMETHOD_(void, UnregisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) PURE;
    STDMETHOD(CreateSourceVoice) (THIS_ __deref_out IXAudio2SourceVoice** ppSourceVoice,
                                  __in const WAVEFORMATEX* pSourceFormat,
                                  UINT32 Flags X2DEFAULT(0),
                                  float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
                                  __in_opt IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
    STDMETHOD(CreateSubmixVoice) (THIS_ __deref_out IXAudio2SubmixVoice** ppSubmixVoice,
                                  UINT32 InputChannels, UINT32 InputSampleRate,
                                  UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
    STDMETHOD(CreateMasteringVoice) (THIS_ __deref_out IXAudio2MasteringVoice** ppMasteringVoice,
                                     UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
                                     UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
                                     UINT32 Flags X2DEFAULT(0), UINT32 DeviceIndex X2DEFAULT(0),
                                     __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;


    STDMETHOD(StartEngine) (THIS) PURE;
    STDMETHOD_(void, StopEngine) (THIS) PURE;
    STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) PURE;
};

#undef INTERFACE
#define INTERFACE IXAudio2Voice
DECLARE_INTERFACE(IXAudio2Voice)
{
    #define Declare_IXAudio2Voice_Methods() \
    STDMETHOD_(void, GetVoiceDetails) (THIS_ __out XAUDIO2_VOICE_DETAILS* pVoiceDetails) PURE; \
    \
    STDMETHOD(SetOutputVoices) (THIS_ __in_opt const XAUDIO2_VOICE_SENDS* pSendList) PURE; \
    \
    STDMETHOD(SetEffectChain) (THIS_ __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain) PURE; \
    \
    STDMETHOD(EnableEffect) (THIS_ UINT32 EffectIndex, \
                             UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD(DisableEffect) (THIS_ UINT32 EffectIndex, \
                              UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetEffectState) (THIS_ UINT32 EffectIndex, __out BOOL* pEnabled) PURE; \
    \
    STDMETHOD(SetEffectParameters) (THIS_ UINT32 EffectIndex, \
                                    __in_bcount(ParametersByteSize) const void* pParameters, \
                                    UINT32 ParametersByteSize, \
                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD(GetEffectParameters) (THIS_ UINT32 EffectIndex, \
                                    __out_bcount(ParametersByteSize) void* pParameters, \
                                    UINT32 ParametersByteSize) PURE; \
    \
    STDMETHOD(SetFilterParameters) (THIS_ __in const XAUDIO2_FILTER_PARAMETERS* pParameters, \
                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetFilterParameters) (THIS_ __out XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
    \
    STDMETHOD(SetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                          __in const XAUDIO2_FILTER_PARAMETERS* pParameters, \
                                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                                 __out XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
    \
    STDMETHOD(SetVolume) (THIS_ float Volume, \
                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetVolume) (THIS_ __out float* pVolume) PURE; \
    \
    STDMETHOD(SetChannelVolumes) (THIS_ UINT32 Channels, __in_ecount(Channels) const float* pVolumes, \
                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetChannelVolumes) (THIS_ UINT32 Channels, __out_ecount(Channels) float* pVolumes) PURE; \
    \
    STDMETHOD(SetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                UINT32 SourceChannels, UINT32 DestinationChannels, \
                                __in_ecount(SourceChannels * DestinationChannels) const float* pLevelMatrix, \
                                UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                       UINT32 SourceChannels, UINT32 DestinationChannels, \
                                       __out_ecount(SourceChannels * DestinationChannels) float* pLevelMatrix) PURE; \
    \
    STDMETHOD_(void, DestroyVoice) (THIS) PURE

    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2SourceVoice
DECLARE_INTERFACE_(IXAudio2SourceVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
    STDMETHOD(Start) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD(Stop) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD(SubmitSourceBuffer) (THIS_ __in const XAUDIO2_BUFFER* pBuffer, __in_opt const XAUDIO2_BUFFER_WMA* pBufferWMA X2DEFAULT(NULL)) PURE;
    STDMETHOD(FlushSourceBuffers) (THIS) PURE;
    STDMETHOD(Discontinuity) (THIS) PURE;
    STDMETHOD(ExitLoop) (THIS_ UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD_(void, GetState) (THIS_ __out XAUDIO2_VOICE_STATE* pVoiceState) PURE;
    STDMETHOD(SetFrequencyRatio) (THIS_ float Ratio,
                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD_(void, GetFrequencyRatio) (THIS_ __out float* pRatio) PURE;
    STDMETHOD(SetSourceSampleRate) (THIS_ UINT32 NewSourceSampleRate) PURE;
};

#undef INTERFACE
#define INTERFACE IXAudio2SubmixVoice
DECLARE_INTERFACE_(IXAudio2SubmixVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2MasteringVoice
DECLARE_INTERFACE_(IXAudio2MasteringVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2EngineCallback
DECLARE_INTERFACE(IXAudio2EngineCallback)
{
    STDMETHOD_(void, OnProcessingPassStart) (THIS) PURE;
    STDMETHOD_(void, OnProcessingPassEnd) (THIS) PURE;
    STDMETHOD_(void, OnCriticalError) (THIS_ HRESULT Error) PURE;
};

#undef INTERFACE
#define INTERFACE IXAudio2VoiceCallback
DECLARE_INTERFACE(IXAudio2VoiceCallback)
{
    STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired) PURE;
    STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) PURE;
    STDMETHOD_(void, OnStreamEnd) (THIS) PURE;
    STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext) PURE;
    STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext) PURE;
    STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext) PURE;
    STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) PURE;
};

// IXAudio2
#define IXAudio2_QueryInterface(This,riid,ppvInterface) ((This)->lpVtbl->QueryInterface(This,riid,ppvInterface))
#define IXAudio2_AddRef(This) ((This)->lpVtbl->AddRef(This))
#define IXAudio2_Release(This) ((This)->lpVtbl->Release(This))
#define IXAudio2_GetDeviceCount(This,puCount) ((This)->lpVtbl->GetDeviceCount(This,puCount))
#define IXAudio2_GetDeviceDetails(This,Index,pDeviceDetails) ((This)->lpVtbl->GetDeviceDetails(This,Index,pDeviceDetails))
#define IXAudio2_Initialize(This,Flags,XAudio2Processor) ((This)->lpVtbl->Initialize(This,Flags,XAudio2Processor))
#define IXAudio2_CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain) ((This)->lpVtbl->CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain))
#define IXAudio2_CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain) ((This)->lpVtbl->CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain))
#define IXAudio2_CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain) ((This)->lpVtbl->CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain))
#define IXAudio2_StartEngine(This) ((This)->lpVtbl->StartEngine(This))
#define IXAudio2_StopEngine(This) ((This)->lpVtbl->StopEngine(This))
#define IXAudio2_CommitChanges(This,OperationSet) ((This)->lpVtbl->CommitChanges(This,OperationSet))

// IXAudio2Voice
#define IXAudio2Voice_GetVoiceDetails(This,pVoiceDetails) ((This)->lpVtbl->GetVoiceDetails(This,pVoiceDetails))
#define IXAudio2Voice_SetOutputVoices(This,pSendList) ((This)->lpVtbl->SetOutputVoices(This,pSendList))
#define IXAudio2Voice_SetEffectChain(This,pEffectChain) ((This)->lpVtbl->SetEffectChain(This,pEffectChain))
#define IXAudio2Voice_EnableEffect(This,EffectIndex,OperationSet) ((This)->lpVtbl->EnableEffect(This,EffectIndex,OperationSet))
#define IXAudio2Voice_DisableEffect(This,EffectIndex,OperationSet) ((This)->lpVtbl->DisableEffect(This,EffectIndex,OperationSet))
#define IXAudio2Voice_GetEffectState(This,EffectIndex,pEnabled) ((This)->lpVtbl->GetEffectState(This,EffectIndex,pEnabled))
#define IXAudio2Voice_SetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize, OperationSet) ((This)->lpVtbl->SetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize,OperationSet))
#define IXAudio2Voice_GetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize) ((This)->lpVtbl->GetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize))
#define IXAudio2Voice_SetFilterParameters(This,pParameters,OperationSet) ((This)->lpVtbl->SetFilterParameters(This,pParameters,OperationSet))
#define IXAudio2Voice_GetFilterParameters(This,pParameters) ((This)->lpVtbl->GetFilterParameters(This,pParameters))
#define IXAudio2Voice_SetOutputFilterParameters(This,pDestinationVoice,pParameters,OperationSet) ((This)->lpVtbl->SetOutputFilterParameters(This,pDestinationVoice,pParameters,OperationSet))
#define IXAudio2Voice_GetOutputFilterParameters(This,pDestinationVoice,pParameters) ((This)->lpVtbl->GetOutputFilterParameters(This,pDestinationVoice,pParameters))
#define IXAudio2Voice_SetVolume(This,Volume,OperationSet) ((This)->lpVtbl->SetVolume(This,Volume,OperationSet))
#define IXAudio2Voice_GetVolume(This,pVolume) ((This)->lpVtbl->GetVolume(This,pVolume))
#define IXAudio2Voice_SetChannelVolumes(This,Channels,pVolumes,OperationSet) ((This)->lpVtbl->SetChannelVolumes(This,Channels,pVolumes,OperationSet))
#define IXAudio2Voice_GetChannelVolumes(This,Channels,pVolumes) ((This)->lpVtbl->GetChannelVolumes(This,Channels,pVolumes))
#define IXAudio2Voice_SetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix,OperationSet) ((This)->lpVtbl->SetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix,OperationSet))
#define IXAudio2Voice_GetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix) ((This)->lpVtbl->GetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix))
#define IXAudio2Voice_DestroyVoice(This) ((This)->lpVtbl->DestroyVoice(This))

// IXAudio2SourceVoice
#define IXAudio2SourceVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2SourceVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2SourceVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2SourceVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2SourceVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2SourceVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2SourceVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2SourceVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2SourceVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2SourceVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2SourceVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2SourceVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2SourceVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2SourceVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2SourceVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2SourceVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2SourceVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2SourceVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2SourceVoice_DestroyVoice IXAudio2Voice_DestroyVoice
#define IXAudio2SourceVoice_Start(This,Flags,OperationSet) ((This)->lpVtbl->Start(This,Flags,OperationSet))
#define IXAudio2SourceVoice_Stop(This,Flags,OperationSet) ((This)->lpVtbl->Stop(This,Flags,OperationSet))
#define IXAudio2SourceVoice_SubmitSourceBuffer(This,pBuffer,pBufferWMA) ((This)->lpVtbl->SubmitSourceBuffer(This,pBuffer,pBufferWMA))
#define IXAudio2SourceVoice_FlushSourceBuffers(This) ((This)->lpVtbl->FlushSourceBuffers(This))
#define IXAudio2SourceVoice_Discontinuity(This) ((This)->lpVtbl->Discontinuity(This))
#define IXAudio2SourceVoice_ExitLoop(This,OperationSet) ((This)->lpVtbl->ExitLoop(This,OperationSet))
#define IXAudio2SourceVoice_GetState(This,pVoiceState) ((This)->lpVtbl->GetState(This,pVoiceState))
#define IXAudio2SourceVoice_SetFrequencyRatio(This,Ratio,OperationSet) ((This)->lpVtbl->SetFrequencyRatio(This,Ratio,OperationSet))
#define IXAudio2SourceVoice_GetFrequencyRatio(This,pRatio) ((This)->lpVtbl->GetFrequencyRatio(This,pRatio))
#define IXAudio2SourceVoice_SetSourceSampleRate(This,NewSourceSampleRate) ((This)->lpVtbl->SetSourceSampleRate(This,NewSourceSampleRate))

// IXAudio2SubmixVoice
#define IXAudio2SubmixVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2SubmixVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2SubmixVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2SubmixVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2SubmixVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2SubmixVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2SubmixVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2SubmixVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2SubmixVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2SubmixVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2SubmixVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2SubmixVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2SubmixVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2SubmixVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2SubmixVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2SubmixVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2SubmixVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2SubmixVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2SubmixVoice_DestroyVoice IXAudio2Voice_DestroyVoice

// IXAudio2MasteringVoice
#define IXAudio2MasteringVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2MasteringVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2MasteringVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2MasteringVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2MasteringVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2MasteringVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2MasteringVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2MasteringVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2MasteringVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2MasteringVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2MasteringVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2MasteringVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2MasteringVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2MasteringVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2MasteringVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2MasteringVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2MasteringVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2MasteringVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2MasteringVoice_DestroyVoice IXAudio2Voice_DestroyVoice

STDAPI XAudio2Create(__deref_out IXAudio2** ppXAudio2, UINT32 Flags X2DEFAULT(0),
                     XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR));


// Undo the #pragma pack(push, 1) directive at the top of this file
#pragma pack(pop)

#endif
