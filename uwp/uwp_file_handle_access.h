#pragma once

// These are copied from WindowsStorageCOM.h
// You can remove this header file once the real file has been updated
// to fix the WINAPI_PARTITION_DESKTOP block

typedef interface IOplockBreakingHandler IOplockBreakingHandler;
typedef interface IStorageItemHandleAccess IStorageItemHandleAccess;
typedef interface IStorageFolderHandleAccess IStorageFolderHandleAccess;

#ifdef __cplusplus
extern "C" {
#endif

   typedef /* [v1_enum] */
      enum HANDLE_OPTIONS {
      HO_NONE = 0,
      HO_OPEN_REQUIRING_OPLOCK = 0x40000,
      HO_DELETE_ON_CLOSE = 0x4000000,
      HO_SEQUENTIAL_SCAN = 0x8000000,
      HO_RANDOM_ACCESS = 0x10000000,
      HO_NO_BUFFERING = 0x20000000,
      HO_OVERLAPPED = 0x40000000,
      HO_WRITE_THROUGH = 0x80000000
   } HANDLE_OPTIONS;

   DEFINE_ENUM_FLAG_OPERATORS(HANDLE_OPTIONS);
   typedef /* [v1_enum] */
      enum HANDLE_ACCESS_OPTIONS {
      HAO_NONE = 0,
      HAO_READ_ATTRIBUTES = 0x80,
      HAO_READ = 0x120089,
      HAO_WRITE = 0x120116,
      HAO_DELETE = 0x10000
   } HANDLE_ACCESS_OPTIONS;

   DEFINE_ENUM_FLAG_OPERATORS(HANDLE_ACCESS_OPTIONS);
   typedef /* [v1_enum] */
      enum HANDLE_SHARING_OPTIONS {
      HSO_SHARE_NONE = 0,
      HSO_SHARE_READ = 0x1,
      HSO_SHARE_WRITE = 0x2,
      HSO_SHARE_DELETE = 0x4
   } HANDLE_SHARING_OPTIONS;

   DEFINE_ENUM_FLAG_OPERATORS(HANDLE_SHARING_OPTIONS);
   typedef /* [v1_enum] */
      enum HANDLE_CREATION_OPTIONS {
      HCO_CREATE_NEW = 0x1,
      HCO_CREATE_ALWAYS = 0x2,
      HCO_OPEN_EXISTING = 0x3,
      HCO_OPEN_ALWAYS = 0x4,
      HCO_TRUNCATE_EXISTING = 0x5
   } HANDLE_CREATION_OPTIONS;

   EXTERN_C const IID IID_IOplockBreakingHandler;
   MIDL_INTERFACE("826ABE3D-3ACD-47D3-84F2-88AAEDCF6304")
      IOplockBreakingHandler : public IUnknown
   {
      public:
       virtual HRESULT STDMETHODCALLTYPE OplockBreaking(void) = 0;
   };

   EXTERN_C const IID IID_IStorageItemHandleAccess;
   MIDL_INTERFACE("5CA296B2-2C25-4D22-B785-B885C8201E6A")
      IStorageItemHandleAccess : public IUnknown
   {
      public:
       virtual HRESULT STDMETHODCALLTYPE Create(
          /* [in] */ HANDLE_ACCESS_OPTIONS accessOptions,
          /* [in] */ HANDLE_SHARING_OPTIONS sharingOptions,
          /* [in] */ HANDLE_OPTIONS options,
          /* [optional][in] */ __RPC__in_opt IOplockBreakingHandler * oplockBreakingHandler,
          /* [system_handle][retval][out] */ __RPC__deref_out_opt HANDLE * interopHandle) = 0;
   };

   EXTERN_C const IID IID_IStorageFolderHandleAccess;
   MIDL_INTERFACE("DF19938F-5462-48A0-BE65-D2A3271A08D6")
      IStorageFolderHandleAccess : public IUnknown
   {
      public:
       virtual HRESULT STDMETHODCALLTYPE Create(
          /* [string][in] */ __RPC__in_string LPCWSTR fileName,
          /* [in] */ HANDLE_CREATION_OPTIONS creationOptions,
          /* [in] */ HANDLE_ACCESS_OPTIONS accessOptions,
          /* [in] */ HANDLE_SHARING_OPTIONS sharingOptions,
          /* [in] */ HANDLE_OPTIONS options,
          /* [optional][in] */ __RPC__in_opt IOplockBreakingHandler * oplockBreakingHandler,
          /* [system_handle][retval][out] */ __RPC__deref_out_opt HANDLE * interopHandle) = 0;
   };
#ifdef __cplusplus
}
#endif
