/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef RARCH_XDK_RESOURCE_H
#define RARCH_XDK_RESOURCE_H

#include "xdk_defines.h"

DWORD XBResource_SizeOf( LPDIRECT3DRESOURCE pResource );

//structure member offsets matter
struct XBRESOURCE
{
#if defined(_XBOX1)
    CHAR* strName;
    DWORD dwOffset;
#elif defined(_XBOX360)
    DWORD dwType;
    DWORD dwOffset;
    DWORD dwSize;
    CHAR* strName;
#endif
};

// Resource types
enum
{
    RESOURCETYPE_USERDATA       = ( ( 'U' << 24 ) | ( 'S' << 16 ) | ( 'E' << 8 ) | ( 'R' ) ),
    RESOURCETYPE_TEXTURE        = ( ( 'T' << 24 ) | ( 'X' << 16 ) | ( '2' << 8 ) | ( 'D' ) ),
    RESOURCETYPE_CUBEMAP        = ( ( 'T' << 24 ) | ( 'X' << 16 ) | ( 'C' << 8 ) | ( 'M' ) ),
    RESOURCETYPE_VOLUMETEXTURE  = ( ( 'T' << 24 ) | ( 'X' << 16 ) | ( '3' << 8 ) | ( 'D' ) ),
    RESOURCETYPE_VERTEXBUFFER   = ( ( 'V' << 24 ) | ( 'B' << 16 ) | ( 'U' << 8 ) | ( 'F' ) ),
    RESOURCETYPE_INDEXBUFFER    = ( ( 'I' << 24 ) | ( 'B' << 16 ) | ( 'U' << 8 ) | ( 'F' ) ),
    RESOURCETYPE_EOF            = 0xffffffff
};

class PackedResource
{
protected:
    BYTE*       m_pSysMemData;        // Alloc'ed memory for resource headers etc.
    DWORD       m_dwSysMemDataSize;

    BYTE*       m_pVidMemData;        // Alloc'ed memory for resource data, etc.
    DWORD       m_dwVidMemDataSize;
 
    XBRESOURCE* m_pResourceTags;     // Tags to associate names with the resources
    DWORD       m_dwNumResourceTags; // Number of resource tags
    BOOL m_bInitialized;       // Resource is fully initialized

public:
    // Loads the resources out of the specified bundle
#if defined(_XBOX1)
    HRESULT Create( const char *strFilename, DWORD dwNumResourceTags = 0L, 
                    XBRESOURCE* pResourceTags = NULL );
#elif defined(_XBOX360)
    HRESULT Create( const char * strFilename );
#endif

    void Destroy();

    BOOL    Initialized() const;

#ifdef _XBOX360
    // Retrieves the resource tags
    void GetResourceTags( DWORD* pdwNumResourceTags, XBRESOURCE** ppResourceTags );
#endif

    // Helper function to make sure a resource is registered
    LPDIRECT3DRESOURCE RegisterResource( LPDIRECT3DRESOURCE pResource ) const
    {
#ifdef _XBOX1
        // Register the resource, if it has not yet been registered. We mark
        // a resource as registered by upping it's reference count.
        if( pResource && ( pResource->Common & D3DCOMMON_REFCOUNT_MASK ) == 1 )
        {
            // Special case CPU-copy push buffers (which live in system memory)
            if( ( pResource->Common & D3DCOMMON_TYPE_PUSHBUFFER ) &&
                ( pResource->Common & D3DPUSHBUFFER_RUN_USING_CPU_COPY ) )
                pResource->Data += (DWORD)m_pSysMemData;
            else
                pResource->Register( m_pVidMemData );

            pResource->AddRef();
        }
#endif
        return pResource;
    }

    // Functions to retrieve resources by their offset
    void *GetData( DWORD dwOffset ) const
    { return &m_pSysMemData[dwOffset]; }

    LPDIRECT3DRESOURCE GetResource( DWORD dwOffset ) const
    { return RegisterResource( (LPDIRECT3DRESOURCE)GetData(dwOffset) ); }

    LPDIRECT3DTEXTURE GetTexture( DWORD dwOffset ) const
    { return (LPDIRECT3DTEXTURE)GetResource( dwOffset ); }

    LPDIRECT3DCUBETEXTURE GetCubemap( DWORD dwOffset ) const
    { return (LPDIRECT3DCUBETEXTURE)GetResource( dwOffset ); }

    LPDIRECT3DVOLUMETEXTURE GetVolumeTexture( DWORD dwOffset ) const
    { return (LPDIRECT3DVOLUMETEXTURE)GetResource( dwOffset ); }

    LPDIRECT3DVERTEXBUFFER GetVertexBuffer( DWORD dwOffset ) const
    { return (LPDIRECT3DVERTEXBUFFER)GetResource( dwOffset ); }

#ifdef _XBOX1
    LPDIRECT3DPUSHBUFFER8 GetPushBuffer( DWORD dwOffset ) const
    { return (LPDIRECT3DPUSHBUFFER8)GetResource( dwOffset ); }
#endif

    // Functions to retrieve resources by their name
    void *GetData( const CHAR* strName ) const;

    LPDIRECT3DRESOURCE GetResource( const CHAR* strName ) const
    { return RegisterResource( (LPDIRECT3DRESOURCE)GetData( strName ) ); }

    LPDIRECT3DTEXTURE GetTexture( const CHAR* strName ) const
    { return (LPDIRECT3DTEXTURE)GetResource( strName ); }

    LPDIRECT3DCUBETEXTURE GetCubemap( const CHAR* strName ) const
    { return (LPDIRECT3DCUBETEXTURE)GetResource( strName ); }

    LPDIRECT3DVOLUMETEXTURE GetVolumeTexture( const CHAR* strName ) const
    { return (LPDIRECT3DVOLUMETEXTURE)GetResource( strName ); }

    LPDIRECT3DVERTEXBUFFER GetVertexBuffer( const CHAR* strName ) const
    { return (LPDIRECT3DVERTEXBUFFER)GetResource( strName ); }

#ifdef _XBOX1
    LPDIRECT3DPUSHBUFFER8 GetPushBuffer( const CHAR* strName ) const
    { return (LPDIRECT3DPUSHBUFFER8)GetResource( strName ); }
#endif

    // Constructor/destructor
    PackedResource();
    ~PackedResource();
};

#endif RARCH_XDK_RESOURCE_H
