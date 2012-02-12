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

#pragma once

#ifndef SSNES360_RESOURCES_H
#define SSNES360_RESOURCES_H

//--------------------------------------------------------------------------------------
// Name tag for resources. An app may initialize this structure, and pass
// it to the resource's Create() function. From then on, the app may call
// GetResource() to retrieve a resource using an ascii name.
//--------------------------------------------------------------------------------------
struct RESOURCE
{
    unsigned long dwType;
    unsigned long dwOffset;
    unsigned long dwSize;
    char * strName;
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


//--------------------------------------------------------------------------------------
// Name: PackedResource
//--------------------------------------------------------------------------------------
class PackedResource
{
protected:
    unsigned char * m_pSysMemData;        // Alloc'ed memory for resource headers etc.
    unsigned long m_dwSysMemDataSize;

    unsigned char * m_pVidMemData;        // Alloc'ed memory for resource data, etc.
    unsigned long m_dwVidMemDataSize;

    RESOURCE* m_pResourceTags;      // Tags to associate names with the resources
    unsigned long m_dwNumResourceTags;  // Number of resource tags
public:
	int m_bInitialized;       // Resource is fully initialized
    // Loads the resources out of the specified bundle
    HRESULT Create( const char * strFilename );

    void    Destroy();

    // Retrieves the resource tags
    void   GetResourceTags( unsigned long * pdwNumResourceTags, 
		RESOURCE** ppResourceTags ) const;

    // Helper function to make sure a resource is registered
    D3DResource* RegisterResource( D3DResource* pResource ) const
    {
        return pResource;
    }

    // Functions to retrieve resources by their offset
    void * GetData( unsigned long dwOffset ) const
    {
        return &m_pSysMemData[dwOffset];
    }

    D3DResource* GetResource( unsigned long dwOffset ) const
    {
        return RegisterResource( ( D3DResource* )GetData( dwOffset ) );
    }

    D3DTexture* GetTexture( unsigned long dwOffset ) const
    {
        return ( D3DTexture* )GetResource( dwOffset );
    }

    D3DArrayTexture* GetArrayTexture( unsigned long dwOffset ) const
    {
        return ( D3DArrayTexture* )GetResource( dwOffset );
    }

    D3DCubeTexture* GetCubemap( unsigned long dwOffset ) const
    {
        return ( D3DCubeTexture* )GetResource( dwOffset );
    }

    D3DVolumeTexture* GetVolumeTexture( unsigned long dwOffset ) const
    {
        return ( D3DVolumeTexture* )GetResource( dwOffset );
    }

    D3DVertexBuffer* GetVertexBuffer( unsigned long dwOffset ) const
    {
        return ( D3DVertexBuffer* )GetResource( dwOffset );
    }

    // Functions to retrieve resources by their name
    void * GetData( const char * strName ) const;

    D3DResource* GetResource( const char * strName ) const
    {
        return RegisterResource( ( D3DResource* )GetData( strName ) );
    }

    D3DTexture* GetTexture( const char * strName ) const
    {
        return ( D3DTexture* )GetResource( strName );
    }

    D3DArrayTexture* GetArrayTexture( const char * strName ) const
    {
        return ( D3DArrayTexture* )GetResource( strName );
    }

    D3DCubeTexture* GetCubemap( const char * strName ) const
    {
        return ( D3DCubeTexture* )GetResource( strName );
    }

    D3DVolumeTexture* GetVolumeTexture( const char * strName ) const
    {
        return ( D3DVolumeTexture* )GetResource( strName );
    }

    D3DVertexBuffer* GetVertexBuffer( const char * strName ) const
    {
        return ( D3DVertexBuffer* )GetResource( strName );
    }

            PackedResource();
            ~PackedResource();
};

#endif
