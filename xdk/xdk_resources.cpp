/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xgraphics.h>
#include "xdk_resources.h"

#ifdef _XBOX360
struct XPR_HEADER
{
   DWORD dwMagic;
   DWORD dwHeaderSize;
   DWORD dwDataSize;
};
#endif

#define XPR0_MAGIC_VALUE 0x30525058
#define XPR1_MAGIC_VALUE 0x31525058
#define XPR2_MAGIC_VALUE 0x58505232

PackedResource::PackedResource()
{
   m_pSysMemData = NULL;
   m_dwSysMemDataSize = 0L;
   m_pVidMemData = NULL;
   m_dwVidMemDataSize = 0L;
   m_pResourceTags = NULL;
   m_dwNumResourceTags = 0L;
   m_bInitialized = FALSE;
}


PackedResource::~PackedResource()
{
   Destroy();
}

void *PackedResource::GetData( const char *strName ) const
{
   if (m_pResourceTags == NULL || strName == NULL)
      return NULL;

#if defined(_XBOX1)
   for (DWORD i=0; m_pResourceTags[i].strName; i++ )
#elif defined(_XBOX360)
      for (DWORD i = 0; i < m_dwNumResourceTags; i++ )
#endif
      {
         if (!strcasecmp( strName, m_pResourceTags[i].strName))
            return &m_pSysMemData[m_pResourceTags[i].dwOffset];
      }

   return NULL;
}

static inline void* AllocateContiguousMemory( DWORD Size, DWORD Alignment)
{
#if defined(_XBOX1)
   return D3D_AllocContiguousMemory(Size, Alignment);
#elif defined(_XBOX360)
   return XMemAlloc( Size, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            Alignment, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL ) );
#endif
}

static inline void FreeContiguousMemory( void* pData )
{
#if defined(_XBOX1)
   return D3D_FreeContiguousMemory(pData);
#elif defined(_XBOX360)
   return XMemFree( pData, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_GameMax,
            0, 0, 0, XALLOC_MEMTYPE_PHYSICAL ) );
#endif
}

#ifdef _XBOX1
char g_strMediaPath[512] = "D:\\Media\\";

static HRESULT FindMediaFile( char *strPath, const char *strFilename, size_t strPathsize)
{
   // Check for valid arguments
   if( strFilename == NULL || strPath == NULL )
   {
      RARCH_ERR("Util_FindMediaFile(): Invalid arguments\n" );
      return E_INVALIDARG;
   }

   // Default path is the filename itself as a fully qualified path
   strlcpy( strPath, strFilename, strPathsize);

   // Check for the ':' character to see if the filename is a fully
   // qualified path. If not, pre-pend the media directory
   if(strFilename[1] != ':')
      snprintf(strPath, strPathsize, "%s%s", g_strMediaPath, strFilename);

   // Try to open the file
   HANDLE hFile = CreateFile( strPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
         OPEN_EXISTING, 0, NULL );

   if( hFile == INVALID_HANDLE_VALUE )
   {
      RARCH_ERR("FindMediaFile(): Could not find file.\n");
      return 0x82000004;
   }

   // Found the file. Close the file and return
   CloseHandle( hFile );

   return S_OK;
}

#endif

#if defined(_XBOX1)
HRESULT PackedResource::Create( const char *strFilename,
      DWORD dwNumResourceTags, XBRESOURCE* pResourceTags)
#elif defined(_XBOX360)
HRESULT PackedResource::Create( const char *strFilename )
#endif
{
#ifdef _XBOX1
   BOOL bHasResourceOffsetsTable = FALSE;

   // Find the media file
   CHAR strResourcePath[512];
   if( FAILED(FindMediaFile(strResourcePath, strFilename, sizeof(strResourcePath))))
      return E_FAIL;
   else
      strFilename = strResourcePath;
#endif

   // Open the file
   HANDLE hFile;
   DWORD dwNumBytesRead;
   hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL );
   if( hFile == INVALID_HANDLE_VALUE )
   {
      RARCH_ERR( "PackedResource::Create(): File <%s> not found.\n", strFilename );
      return E_FAIL;
   }

   // Read in and verify the XPR magic header
   XPR_HEADER xprh;
   bool retval = ReadFile( hFile, &xprh, sizeof( XPR_HEADER ), &dwNumBytesRead, NULL );

#if defined(_XBOX1)
   if( xprh.dwMagic == XPR0_MAGIC_VALUE )
      bHasResourceOffsetsTable = FALSE;
   else if( xprh.dwMagic == XPR1_MAGIC_VALUE )
      bHasResourceOffsetsTable = TRUE;
   else
#elif defined(_XBOX360)
      if(!retval)
      {
         RARCH_ERR("Error reading XPR header in file %s.\n", strFilename );
         CloseHandle( hFile );
         return E_FAIL;
      }

   if( xprh.dwMagic != XPR2_MAGIC_VALUE )
#endif
   {
      RARCH_ERR( "Invalid Xbox Packed Resource (.xpr) file: Magic = 0x%08lx\n", xprh.dwMagic );
      CloseHandle( hFile );
      return E_FAIL;
   }

   // Compute memory requirements
#if defined(_XBOX1)
   m_dwSysMemDataSize = xprh.dwHeaderSize - sizeof(XPR_HEADER);
   m_dwVidMemDataSize = xprh.dwTotalSize - xprh.dwHeaderSize;
#elif defined(_XBOX360)
   m_dwSysMemDataSize = xprh.dwHeaderSize;
   m_dwVidMemDataSize = xprh.dwDataSize;
#endif

   // Allocate memory
   m_pSysMemData = (BYTE*)malloc(m_dwSysMemDataSize);
   if (m_pSysMemData == NULL)
   {
      RARCH_ERR( "Could not allocate system memory.\n" );
      m_dwSysMemDataSize = 0;
      return E_FAIL;
   }

   m_pVidMemData = ( BYTE* )AllocateContiguousMemory( m_dwVidMemDataSize,
#if defined(_XBOX1)
         D3DTEXTURE_ALIGNMENT
#elif defined(_XBOX360)
         XALLOC_PHYSICAL_ALIGNMENT_4K
#endif
         );

   if( m_pVidMemData == NULL )
   {
      RARCH_ERR( "Could not allocate physical memory.\n" );
      m_dwSysMemDataSize = 0;
      m_dwVidMemDataSize = 0;
      free(m_pSysMemData);
      m_pSysMemData = NULL;
      return E_FAIL;
   }

   // Read in the data from the file
   if( !ReadFile( hFile, m_pSysMemData, m_dwSysMemDataSize, &dwNumBytesRead, NULL ) ||
         !ReadFile( hFile, m_pVidMemData, m_dwVidMemDataSize, &dwNumBytesRead, NULL ) )
   {
      RARCH_ERR( "Unable to read Xbox Packed Resource (.xpr) file\n" );
      CloseHandle( hFile );
      return E_FAIL;
   }

   // Done with the file
   CloseHandle( hFile );

#ifdef _XBOX1
   if (bHasResourceOffsetsTable)
   {
#endif

      // Extract resource table from the header data
      m_dwNumResourceTags = *( DWORD* )( m_pSysMemData + 0 );
      m_pResourceTags = ( XBRESOURCE* )( m_pSysMemData + 4 );

      // Patch up the resources
      for( DWORD i = 0; i < m_dwNumResourceTags; i++ )
      {
         m_pResourceTags[i].strName = ( CHAR* )( m_pSysMemData + ( DWORD )m_pResourceTags[i].strName );
#ifdef _XBOX360
         // Fixup the texture memory
         if( ( m_pResourceTags[i].dwType & 0xffff0000 ) == ( RESOURCETYPE_TEXTURE & 0xffff0000 ) )
         {
            D3DTexture* pTexture = ( D3DTexture* )&m_pSysMemData[m_pResourceTags[i].dwOffset];

            // Adjust Base address according to where memory was allocated
            XGOffsetBaseTextureAddress( pTexture, m_pVidMemData, m_pVidMemData );
         }
#endif
      }

#ifdef _XBOX1
   }
#endif

#ifdef _XBOX1
   // Use user-supplied number of resources and the resource tags
   if( dwNumResourceTags != 0 || pResourceTags != NULL )
   {
      m_pResourceTags     = pResourceTags;
      m_dwNumResourceTags = dwNumResourceTags;
   }
#endif

   m_bInitialized = TRUE;

   return S_OK;
}

#ifdef _XBOX360
void PackedResource::GetResourceTags( DWORD* pdwNumResourceTags,
      XBRESOURCE** ppResourceTags )
{
   if (pdwNumResourceTags)
      (*pdwNumResourceTags) = m_dwNumResourceTags;

   if (ppResourceTags )
      (*ppResourceTags) = m_pResourceTags;
}
#endif

void PackedResource::Destroy()
{
   free(m_pSysMemData);
   m_pSysMemData = NULL;
   m_dwSysMemDataSize = 0L;

   if (m_pVidMemData != NULL)
      FreeContiguousMemory(m_pVidMemData);

   m_pVidMemData = NULL;
   m_dwVidMemDataSize = 0L;

   m_pResourceTags = NULL;
   m_dwNumResourceTags = 0L;

   m_bInitialized = FALSE;
}

BOOL PackedResource::Initialized() const
{
   return m_bInitialized;
}
