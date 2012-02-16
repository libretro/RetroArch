/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xtl.h>
#include <xgraphics.h>
#include "xdk360_video.h"
#include "xdk360_video_resources.h"
#include "../general.h"

//--------------------------------------------------------------------------------------
// Magic values to identify XPR files
//--------------------------------------------------------------------------------------
struct XPR_HEADER
{
    unsigned long dwMagic;
    unsigned long dwHeaderSize;
    unsigned long dwDataSize;
};

#define XPR2_MAGIC_VALUE (0x58505232)
#define eXALLOCAllocatorId_AtgResource eXALLOCAllocatorId_GameMax

//--------------------------------------------------------------------------------------
// Name: PackedResource
//--------------------------------------------------------------------------------------
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


//--------------------------------------------------------------------------------------
// Name: PackedResource
//--------------------------------------------------------------------------------------
PackedResource::~PackedResource()
{
    Destroy();
}

//--------------------------------------------------------------------------------------
// Name: GetData
// Desc: Loads all the texture resources from the given XPR.
//--------------------------------------------------------------------------------------
void * PackedResource::GetData( const char * strName ) const
{
    if( m_pResourceTags == NULL || strName == NULL )
        return NULL;

    for( unsigned long i = 0; i < m_dwNumResourceTags; i++ )
    {
        if( !_stricmp( strName, m_pResourceTags[i].strName ) )
            return &m_pSysMemData[m_pResourceTags[i].dwOffset];
    }

    return NULL;
}

//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Loads all the texture resources from the given XPR.
//--------------------------------------------------------------------------------------
HRESULT PackedResource::Create( const char * strFilename )
{
    // Open the file
    unsigned long dwNumBytesRead;
    HANDLE hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        SSNES_ERR( "File <%s> not found.\n", strFilename );
        return E_FAIL;
    }

    // Read in and verify the XPR magic header
    XPR_HEADER xprh;
    if( !ReadFile( hFile, &xprh, sizeof( XPR_HEADER ), &dwNumBytesRead, NULL ) )
    {
        SSNES_ERR( "Error reading XPR header in file <%s>.\n", strFilename );
        CloseHandle( hFile );
        return E_FAIL;
    }

    if( xprh.dwMagic != XPR2_MAGIC_VALUE )
    {
        SSNES_ERR( "Invalid Xbox Packed Resource (.xpr) file: Magic = 0x%08lx.\n", xprh.dwMagic );
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Compute memory requirements
    m_dwSysMemDataSize = xprh.dwHeaderSize;
    m_dwVidMemDataSize = xprh.dwDataSize;

    // Allocate memory
    m_pSysMemData = new BYTE[m_dwSysMemDataSize];
    if( m_pSysMemData == NULL )
    {
        SSNES_ERR( "Could not allocate system memory.\n" );
        m_dwSysMemDataSize = 0;
        return E_FAIL;
    }
    m_pVidMemData = ( BYTE* )XMemAlloc( m_dwVidMemDataSize, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_AtgResource,
			    XALLOC_PHYSICAL_ALIGNMENT_4K, XALLOC_MEMPROTECT_WRITECOMBINE, 0, XALLOC_MEMTYPE_PHYSICAL ) );

    if( m_pVidMemData == NULL )
    {
        SSNES_ERR( "Could not allocate physical memory.\n" );
        m_dwSysMemDataSize = 0;
        m_dwVidMemDataSize = 0;
        delete[] m_pSysMemData;
        m_pSysMemData = NULL;
        return E_FAIL;
    }

    // Read in the data from the file
    if( !ReadFile( hFile, m_pSysMemData, m_dwSysMemDataSize, &dwNumBytesRead, NULL ) ||
        !ReadFile( hFile, m_pVidMemData, m_dwVidMemDataSize, &dwNumBytesRead, NULL ) )
    {
        SSNES_ERR( "Unable to read Xbox Packed Resource (.xpr) file.\n" );
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Done with the file
    CloseHandle( hFile );

    // Extract resource table from the header data
    m_dwNumResourceTags = *( unsigned long * )( m_pSysMemData + 0 );
    m_pResourceTags = ( RESOURCE* )( m_pSysMemData + 4 );

    // Patch up the resources
    for( unsigned long i = 0; i < m_dwNumResourceTags; i++ )
    {
        m_pResourceTags[i].strName = ( char * )( m_pSysMemData + ( unsigned long )m_pResourceTags[i].strName );

        // Fixup the texture memory
        if( ( m_pResourceTags[i].dwType & 0xffff0000 ) == ( RESOURCETYPE_TEXTURE & 0xffff0000 ) )
        {
            D3DTexture* pTexture = ( D3DTexture* )&m_pSysMemData[m_pResourceTags[i].dwOffset];
            // Adjust Base address according to where memory was allocated
            XGOffsetBaseTextureAddress( pTexture, m_pVidMemData, m_pVidMemData );
        }
    }

    m_bInitialized = TRUE;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Destroy
// Desc: Cleans up the packed resource data
//--------------------------------------------------------------------------------------
void PackedResource::Destroy()
{
    delete[] m_pSysMemData;
    m_pSysMemData = NULL;
    m_dwSysMemDataSize = 0L;

    if( m_pVidMemData != NULL )
	    XMemFree( m_pVidMemData, MAKE_XALLOC_ATTRIBUTES( 0, 0, 0, 0, eXALLOCAllocatorId_AtgResource,
            0, 0, 0, XALLOC_MEMTYPE_PHYSICAL ) );

    m_pVidMemData = NULL;
    m_dwVidMemDataSize = 0L;

    m_pResourceTags = NULL;
    m_dwNumResourceTags = 0L;

    m_bInitialized = FALSE;
}
