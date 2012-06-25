/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#pragma once

#ifndef RARCH_360_RESOURCES_H
#define RARCH_360_RESOURCES_H

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

      RESOURCE* m_pResourceTags;            // Tags to associate names with the resources
      unsigned long m_dwNumResourceTags;    // Number of resource tags
   public:
      int m_bInitialized;                   // Resource is fully initialized
      HRESULT Create( const char * strFilename );
      void    Destroy();

      void * GetData( unsigned long dwOffset ) const
      {
         return &m_pSysMemData[dwOffset];
      }

      D3DResource* GetResource( unsigned long dwOffset ) const
      {
         return (( D3DResource* )GetData( dwOffset ) );
      }

      D3DTexture* GetTexture( unsigned long dwOffset ) const
      {
         return ( D3DTexture* )GetResource( dwOffset );
      }

      void * GetData( const char * strName ) const;

      D3DResource* GetResource( const char * strName ) const
      {
         return ( ( D3DResource* )GetData( strName ) );
      }

      D3DTexture* GetTexture( const char * strName ) const
      {
         return ( D3DTexture* )GetResource( strName );
      }

      PackedResource();
      ~PackedResource();
};

#endif
