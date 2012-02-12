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
#include "xdk360_video_debugfonts.h"
#include "../general.h"

#define CALCFONTFILEHEADERSIZE(x) ( sizeof(unsigned long) + (sizeof(float)* 4) + sizeof(unsigned short) + (sizeof(wchar_t)*(x)) )
#define FONTFILEVERSION 5

typedef struct FontFileHeaderImage_t {
    unsigned long m_dwFileVersion;          // Version of the font file (Must match FONTFILEVERSION)
    float m_fFontHeight;            // Height of the font strike in pixels
    float m_fFontTopPadding;        // Padding above the strike zone
    float m_fFontBottomPadding;     // Padding below the strike zone
    float m_fFontYAdvance;          // Number of pixels to move the cursor for a line feed
    unsigned short m_cMaxGlyph;               // Number of font characters (Should be an odd number to maintain DWORD Alignment)
    wchar_t m_TranslatorTable[1];     // ASCII to Glyph lookup table, NOTE: It's m_cMaxGlyph+1 in size.
                                    // Entry 0 maps to the "Unknown" glyph.    
} FontFileHeaderImage_t;

// Font strike array. Immediately follows the FontFileHeaderImage_t
// structure image

typedef struct FontFileStrikesImage_t {
    unsigned long m_dwNumGlyphs;            // Size of font strike array (First entry is the unknown glyph)
    GLYPH_ATTR m_Glyphs[1];         // Array of font strike uv's etc... NOTE: It's m_dwNumGlyphs in size
} FontFileStrikesImage_t; 

//--------------------------------------------------------------------------------------
// Vertex and pixel shaders for font rendering
// Please note the removal of comment or dead lines...
// They are commented out because the shader compiler has no use for them.
//--------------------------------------------------------------------------------------
static const char g_strFontShader[] =
    "struct VS_IN\n"
    "{\n"
        "float2 Pos : POSITION;\n"
        "float2 Tex : TEXCOORD0;\n"
        "float4 ChannelSelector : TEXCOORD1;\n"
    "};\n"
//  "\n"
    "struct VS_OUT\n"
    "{\n"
        "float4 Position : POSITION;\n"
        "float4 Diffuse : COLOR0_center;\n"
        "float2 TexCoord0 : TEXCOORD0;\n"
        "float4 ChannelSelector : TEXCOORD1;\n"
    "};\n"
//  "\n"
    "uniform float4 Color : register(c1);\n"
    "uniform float2 TexScale : register(c2);\n"
//  "\n"
    "sampler FontTexture : register(s0);\n"
//  "\n"
    "VS_OUT FontVertexShader( VS_IN In )\n"
    "{\n"
        "VS_OUT Out;\n"
        "Out.Position.x  = (In.Pos.x-0.5);\n"
        "Out.Position.y  = (In.Pos.y-0.5);\n"
        "Out.Position.z  = ( 0.0 );\n"
        "Out.Position.w  = ( 1.0 );\n"
        "Out.Diffuse = Color;\n"
        "Out.TexCoord0.x = In.Tex.x * TexScale.x;\n"
        "Out.TexCoord0.y = In.Tex.y * TexScale.y;\n"
        "Out.ChannelSelector = In.ChannelSelector;\n"
        "return Out;\n"
    "}\n"
 // "\n"
    "float4 FontPixelShader( VS_OUT In ) : COLOR0\n"
    "{\n"
//      "// Fetch a texel from the font texture\n"
        "float4 FontTexel = tex2D( FontTexture, In.TexCoord0 );\n"
//      "\n"
        "if( dot( In.ChannelSelector, float4(1,1,1,1) ) )\n"
        "{\n"
//          "// Select the color from the channel\n"
            "float value = dot( FontTexel, In.ChannelSelector );\n"
//          "\n"
//          "// For white pixels, the high bit is 1 and the low\n"
//          "// bits are luminance, so r0.a will be > 0.5. For the\n"
//          "// RGB channel, we want to lop off the msb and shift\n"
//          "// the lower bits up one bit. This is simple to do\n"
//          "// with the _bx2 modifier. Since these pixels are\n"
//          "// opaque, we emit a 1 for the alpha channel (which\n"
//          "// is 0.5 x2 ).\n"
//          "\n"
//          "// For black pixels, the high bit is 0 and the low\n"
//          "// bits are alpha, so r0.a will be < 0.5. For the RGB\n"
//          "// channel, we emit zero. For the alpha channel, we\n"
//          "// just use the x2 modifier to scale up the low bits\n"
//          "// of the alpha.\n"
            "float4 Color;\n"
            "Color.rgb = ( value > 0.5f ? 2*value-1 : 0.0f );\n"
            "Color.a = 2 * ( value > 0.5f ? 1.0f : value );\n"
//          "\n"
//          "// Return the texture color modulated with the vertex\n"
//          "// color\n"
            "return Color * In.Diffuse;\n"
        "}\n"
        "else\n"
        "{\n"
            "return FontTexel * In.Diffuse;\n"
        "}\n"
    "}\n";

//
// These are shared assets common among all instances of AtgFont
//

typedef struct AtgFont_Locals_t {
    D3DVertexDeclaration* m_pFontVertexDecl;    // Shared vertex buffer
    D3DVertexShader* m_pFontVertexShader;       // Created vertex shader
    D3DPixelShader* m_pFontPixelShader;         // Created pixel shader
} AtgFont_Locals_t;

// All elements are defaulted to NULL
static AtgFont_Locals_t s_AtgFontLocals;    // Global static instance

//--------------------------------------------------------------------------------------
// Name: CreateFontShaders()
// Desc: Creates the global font shaders
//--------------------------------------------------------------------------------------

HRESULT XdkFont::CreateFontShaders()
{   
    //
    // There are only two states the globals could be in,
    // Initialized, in which the ref count is increased,
    // Uninialized, in which the vertex/pixel shaders need to be
    // started up and a vertex array created.
    ///
    
    HRESULT Result;         // Returned error code
    if (!s_AtgFontLocals.m_pFontVertexDecl)
    {
        // Use the do {} while(0); trick for a fake goto
        // It simplies tear down on error conditions.
        do
		{
        
            // Step #1, create my vertex array with 16 bytes per entry
            // Floats for the position,
            // shorts for the uvs
            // 32 bit packed ARGB 8:8:8:8 for color

            static const D3DVERTEXELEMENT9 decl[] =
            {
                { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                { 0,  8, D3DDECLTYPE_USHORT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
                D3DDECL_END()
            };
            
            // Cache this global into a register
			xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
            D3DDevice *pd3dDevice = vid->xdk360_render_device;

            Result = pd3dDevice->CreateVertexDeclaration( decl, &s_AtgFontLocals.m_pFontVertexDecl );
            if ( SUCCEEDED( Result ) )
            {

                // Step #2, create my vertex shader
                ID3DXBuffer* pShaderCode;
                Result = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                    NULL, NULL, "FontVertexShader", "vs.2.0", 0,&pShaderCode, NULL, NULL );
                if ( SUCCEEDED( Result ) )
                {
                    Result = pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                        &s_AtgFontLocals.m_pFontVertexShader );
                    // Release the compiled shader
                    pShaderCode->Release();
                    
                    if( SUCCEEDED( Result ) )
                    {
                        // Step #3, create my pixel shader
                        Result = D3DXCompileShader( g_strFontShader, sizeof(g_strFontShader)-1 ,
                            NULL, NULL, "FontPixelShader", "ps.2.0", 0,&pShaderCode, NULL, NULL );
                        if ( SUCCEEDED( Result ) )
                        {
                            Result = pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                &s_AtgFontLocals.m_pFontPixelShader );
                            // Release the compiled shader
                            pShaderCode->Release();

                            if ( SUCCEEDED( Result ) ) 
                            {
                                Result = S_OK;
                                break;              // Skip the teardown code
                            }
                        }
                        // If the code got to here, a fatal error has occured
                        // and a clean shutdown needs to be performed.

                        s_AtgFontLocals.m_pFontVertexShader->Release();
                    }
                    // Ensure the pointer is NULL
                    s_AtgFontLocals.m_pFontVertexShader = NULL;
                }
                s_AtgFontLocals.m_pFontVertexDecl->Release();
            }
            // Ensure this pointer is NULL    
            s_AtgFontLocals.m_pFontVertexDecl = NULL;
        }while(0);            // Exit point for the break command.
        return Result;
    }
    else
    {
    //
    // Already initialized, so just add to the ref counts
    //

        s_AtgFontLocals.m_pFontVertexDecl->AddRef();
        s_AtgFontLocals.m_pFontVertexShader->AddRef();
        s_AtgFontLocals.m_pFontPixelShader->AddRef();
        Result = S_OK;
    }
    return Result;          // Return the error code if any
}

//--------------------------------------------------------------------------------------
// Name: ReleaseFontShaders()
// Desc: Releases the font shaders by reference
//--------------------------------------------------------------------------------------

void XdkFont::ReleaseFontShaders()
{
    // Safely release shaders
    // NOTE: They are released in reverse order of creation
    // to make sure any interdependencies are dealt with

    if( ( s_AtgFontLocals.m_pFontPixelShader != NULL ) && ( s_AtgFontLocals.m_pFontPixelShader->Release() == 0 ) )
        s_AtgFontLocals.m_pFontPixelShader = NULL;
    if( ( s_AtgFontLocals.m_pFontVertexShader != NULL ) && ( s_AtgFontLocals.m_pFontVertexShader->Release() == 0 ) )
        s_AtgFontLocals.m_pFontVertexShader = NULL;
    if( ( s_AtgFontLocals.m_pFontVertexDecl != NULL ) && ( s_AtgFontLocals.m_pFontVertexDecl->Release() == 0 ) )
        s_AtgFontLocals.m_pFontVertexDecl = NULL;
}

//--------------------------------------------------------------------------------------
// Name: Font()
// Desc: Constructor
//--------------------------------------------------------------------------------------
XdkFont::XdkFont()
{
    m_pFontTexture = NULL;

    m_dwNumGlyphs = 0L;
    m_Glyphs = NULL;

    m_fCursorX = 0.0f;
    m_fCursorY = 0.0f;

    m_fXScaleFactor = 2.0f;
    m_fYScaleFactor = 2.0f;
    m_fSlantFactor = 0.0f;
    m_bRotate = FALSE;
    m_dRotCos = cos( 0.0 );
    m_dRotSin = sin( 0.0 );

    m_cMaxGlyph = 0;
    m_TranslatorTable = NULL;

    m_dwNestedBeginCount = 0L;
}


//--------------------------------------------------------------------------------------
// Name: ~Font()
// Desc: Destructor
//--------------------------------------------------------------------------------------
XdkFont::~XdkFont()
{
    Destroy();
}


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Create the font's internal objects (texture and array of glyph info)
//       using the XPR packed resource file
//--------------------------------------------------------------------------------------
HRESULT XdkFont::Create( const char * strFontFileName )
{
    // Create the font
    if( FAILED( m_xprResource.Create( strFontFileName ) ) )
        return E_FAIL;

	D3DTexture * pFontTexture = m_xprResource.GetTexture( "FontTexture" );
	const void * pFontData = m_xprResource.GetData( "FontData"); 

	 // Save a copy of the texture
    m_pFontTexture = pFontTexture;

    // Check version of file (to make sure it matches up with the FontMaker tool)
    const unsigned char * pData = static_cast<const unsigned char *>(pFontData);
    unsigned long dwFileVersion = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_dwFileVersion;

    if( dwFileVersion == FONTFILEVERSION )
    {
        m_fFontHeight = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontHeight;
        m_fFontTopPadding = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontTopPadding;
        m_fFontBottomPadding = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontBottomPadding;
        m_fFontYAdvance = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_fFontYAdvance;

        // Point to the translator string which immediately follows the 4 floats
        m_cMaxGlyph = reinterpret_cast<const FontFileHeaderImage_t *>(pData)->m_cMaxGlyph;
  
        m_TranslatorTable = const_cast<FontFileHeaderImage_t*>(reinterpret_cast<const FontFileHeaderImage_t *>(pData))->m_TranslatorTable;

        pData += CALCFONTFILEHEADERSIZE( m_cMaxGlyph + 1 );

        // Read the glyph attributes from the file
        m_dwNumGlyphs = reinterpret_cast<const FontFileStrikesImage_t *>(pData)->m_dwNumGlyphs;
        m_Glyphs = reinterpret_cast<const FontFileStrikesImage_t *>(pData)->m_Glyphs;        // Pointer
    }
    else
    {
        SSNES_ERR( "Incorrect version number on font file.\n" );
        return E_FAIL;
    }

    // Create the vertex and pixel shaders for rendering the font
    if( FAILED( CreateFontShaders() ) )
    {
        SSNES_ERR( "Could not create font shaders.\n" );
        return E_FAIL;
    }

	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *pd3dDevice = vid->xdk360_render_device;

    // Initialize the window
    D3DDISPLAYMODE DisplayMode;
    pd3dDevice->GetDisplayMode( 0, &DisplayMode );
    m_rcWindow.x1 = 0;
    m_rcWindow.y1 = 0;
    m_rcWindow.x2 = DisplayMode.Width;
    m_rcWindow.y2 = DisplayMode.Height;

    // Determine whether we should save/restore state
    m_bSaveState = TRUE;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroy the font object
//--------------------------------------------------------------------------------------
void XdkFont::Destroy()
{
    m_pFontTexture = NULL;
    m_dwNumGlyphs = 0L;
    m_Glyphs = NULL;
    m_cMaxGlyph = 0;
    m_TranslatorTable = NULL;
    m_dwNestedBeginCount = 0L;

    // Safely release shaders
    ReleaseFontShaders();

    if( m_xprResource.m_bInitialized)
        m_xprResource.Destroy();
}


//--------------------------------------------------------------------------------------
// Name: SetWindow()
// Desc: Sets the  and the bounds rect for drawing text and resets the cursor position
//--------------------------------------------------------------------------------------
void XdkFont::SetWindow(const D3DRECT &rcWindow )
{
    m_rcWindow.x1 = rcWindow.x1;
    m_rcWindow.y1 = rcWindow.y1;
    m_rcWindow.x2 = rcWindow.x2;
    m_rcWindow.y2 = rcWindow.y2;

    m_fCursorX = 0.0f;
    m_fCursorY = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: SetWindow()
// Desc: Sets the  and the bounds rect for drawing text and resets the cursor position
//--------------------------------------------------------------------------------------
void XdkFont::SetWindow( long x1, long y1, long x2, long y2 )
{
    m_rcWindow.x1 = x1;
    m_rcWindow.y1 = y1;
    m_rcWindow.x2 = x2;
    m_rcWindow.y2 = y2;

    m_fCursorX = 0.0f;
    m_fCursorY = 0.0f;
}

//--------------------------------------------------------------------------------------
// Name: GetWindow()
// Desc: Gets the current bounds rect for drawing
//--------------------------------------------------------------------------------------
void XdkFont::GetWindow( D3DRECT &rcWindow ) const
{
    rcWindow = m_rcWindow;      // NOTE: This is a structure copy
}

//--------------------------------------------------------------------------------------
// Name: SetCursorPosition()
// Desc: Sets the cursor position for drawing text
//--------------------------------------------------------------------------------------
void XdkFont::SetCursorPosition( float fCursorX, float fCursorY )
{
    m_fCursorX = floorf( fCursorX );
    m_fCursorY = floorf( fCursorY );
}

//--------------------------------------------------------------------------------------
// Name: GetTextExtent()
// Desc: Get the dimensions of a text string
//--------------------------------------------------------------------------------------

void XdkFont::GetTextExtent( const wchar_t * strText, float * pWidth,
                          float * pHeight, int bFirstLineOnly ) const
{
    // Set default text extent in output parameters
    int iWidth = 0;
    float fHeight = 0.0f;

    if( strText )
    {
        // Initialize counters that keep track of text extent
        int ix = 0;
        float fy = m_fFontHeight;       // One character high to start
        if( fy > fHeight )
            fHeight = fy;

        // Loop through each character and update text extent
        DWORD letter;
        while( (letter = *strText) != 0 )
        {
            ++strText;

            // Handle newline character
            if( letter == L'\n' )
            {
                if( bFirstLineOnly )
                    break;
                ix = 0;
                fy += m_fFontYAdvance;
                // since the height has changed, test against the height extent
                if( fy > fHeight )
                    fHeight = fy;
           }

            // Handle carriage return characters by ignoring them. This helps when
            // displaying text from a file.
            if( letter == L'\r' )
                continue;

            // Translate unprintable characters
            const GLYPH_ATTR* pGlyph;
            
            if( letter > m_cMaxGlyph )
                letter = 0;     // Out of bounds?
            else
                letter = m_TranslatorTable[letter];     // Remap ASCII to glyph

            pGlyph = &m_Glyphs[letter];                 // Get the requested glyph

            // Get text extent for this character's glyph
            ix += pGlyph->wOffset;
            ix += pGlyph->wAdvance;

            // Since the x widened, test against the x extent

            if( ix > iWidth )
                iWidth = ix;
        }
    }

    // Convert the width to a float here, load/hit/store. :(
    float fWidth = static_cast<float>(iWidth);          // Delay the use if fWidth to reduce LHS pain
    // Apply the scale factor to the result
    fHeight *= m_fYScaleFactor;
     // Store the final results
    *pHeight = fHeight;

    fWidth *= m_fXScaleFactor;
    *pWidth = fWidth;
}

//--------------------------------------------------------------------------------------
// Name: GetTextWidth()
// Desc: Returns the width in pixels of a text string
//--------------------------------------------------------------------------------------
float XdkFont::GetTextWidth( const wchar_t * strText ) const
{
    float fTextWidth, fTextHeight;
    GetTextExtent( strText, &fTextWidth, &fTextHeight );
    return fTextWidth;
}


//--------------------------------------------------------------------------------------
// Name: Begin()
// Desc: Prepares the font vertex buffers for rendering.
//--------------------------------------------------------------------------------------
VOID XdkFont::Begin()
{
    PIXBeginNamedEvent( 0, "Text Rendering" );

    // Set state on the first call
    if( m_dwNestedBeginCount == 0 )
    {
        // Cache the global pointer into a register
		xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
		D3DDevice *pD3dDevice = vid->xdk360_render_device;

        // Save state
        if( m_bSaveState )
        {
            // Note, we are not saving the texture, vertex, or pixel shader,
            //       since it's not worth the performance. We're more interested
            //       in saving state that would cause hard to find problems.
            pD3dDevice->GetRenderState( D3DRS_ALPHABLENDENABLE,
                                          &m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ] );
            pD3dDevice->GetRenderState( D3DRS_SRCBLEND, &m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
            pD3dDevice->GetRenderState( D3DRS_DESTBLEND, &m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
            pD3dDevice->GetRenderState( D3DRS_BLENDOP, &m_dwSavedState[ SAVEDSTATE_D3DRS_BLENDOP ] );
            pD3dDevice->GetRenderState( D3DRS_ALPHATESTENABLE, &m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHATESTENABLE ] );
            pD3dDevice->GetRenderState( D3DRS_ALPHAREF, &m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
            pD3dDevice->GetRenderState( D3DRS_ALPHAFUNC, &m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
            pD3dDevice->GetRenderState( D3DRS_FILLMODE, &m_dwSavedState[ SAVEDSTATE_D3DRS_FILLMODE ] );
            pD3dDevice->GetRenderState( D3DRS_CULLMODE, &m_dwSavedState[ SAVEDSTATE_D3DRS_CULLMODE ] );
            pD3dDevice->GetRenderState( D3DRS_ZENABLE, &m_dwSavedState[ SAVEDSTATE_D3DRS_ZENABLE ] );
            pD3dDevice->GetRenderState( D3DRS_STENCILENABLE, &m_dwSavedState[ SAVEDSTATE_D3DRS_STENCILENABLE ] );
            pD3dDevice->GetRenderState( D3DRS_VIEWPORTENABLE, &m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
            pD3dDevice->GetSamplerState( 0, D3DSAMP_MINFILTER, &m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ] );
            pD3dDevice->GetSamplerState( 0, D3DSAMP_MAGFILTER, &m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ] );
            pD3dDevice->GetSamplerState( 0, D3DSAMP_ADDRESSU, &m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSU ] );
            pD3dDevice->GetSamplerState( 0, D3DSAMP_ADDRESSV, &m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSV ] );
        }

        // Set the texture scaling factor as a vertex shader constant
        D3DSURFACE_DESC TextureDesc;
        m_pFontTexture->GetLevelDesc( 0, &TextureDesc );		// Get the description
 
        // Set render state
        pD3dDevice->SetTexture( 0, m_pFontTexture );
 
        // Read the TextureDesc here to ensure no load/hit/store from GetLevelDesc()
        float vTexScale[4];
        vTexScale[0] = 1.0f / TextureDesc.Width;		// LHS due to int->float conversion
        vTexScale[1] = 1.0f / TextureDesc.Height;
        vTexScale[2] = 0.0f;
        vTexScale[3] = 0.0f;
        
        pD3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        pD3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        pD3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        pD3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
        pD3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
        pD3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x08 );
        pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
        pD3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
        pD3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        pD3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        pD3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
        pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        
        pD3dDevice->SetVertexDeclaration( s_AtgFontLocals.m_pFontVertexDecl );
        pD3dDevice->SetVertexShader( s_AtgFontLocals.m_pFontVertexShader );
        pD3dDevice->SetPixelShader( s_AtgFontLocals.m_pFontPixelShader );

        // Set the texture scaling factor as a vertex shader constant
        // Call here to avoid load hit store from writing to vTexScale above
        pD3dDevice->SetVertexShaderConstantF( 2, vTexScale, 1 );
    }

    // Keep track of the nested begin/end calls.
    m_dwNestedBeginCount++;
}


//--------------------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draws text as textured polygons
//--------------------------------------------------------------------------------------
VOID XdkFont::DrawText( DWORD dwColor, const wchar_t * strText, unsigned long dwFlags,
                     float fMaxPixelWidth )
{
    DrawText( m_fCursorX, m_fCursorY, dwColor, strText, dwFlags, fMaxPixelWidth );
}


//--------------------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draws text as textured polygons
//       TODO: This function should use the Begin/SetVertexData/End() API when it
//       becomes available.
//--------------------------------------------------------------------------------------
VOID XdkFont::DrawText( float fOriginX, float fOriginY, unsigned long dwColor,
                     const wchar_t * strText, unsigned long dwFlags, float fMaxPixelWidth )
{
    if( strText == NULL )
		return;
    if( L'\0' == strText[0] )
		return;

	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	D3DDevice *pd3dDevice = vid->xdk360_render_device;
 
    // Create a PIX user-defined event that encapsulates all of the text draw calls.
    // This makes DrawText calls easier to recognize in PIX captures, and it makes
    // them take up fewer entries in the event list.
    PIXBeginNamedEvent( dwColor, "DrawText: %S", strText );

    // Set the color as a vertex shader constant
    float vColor[4];
    vColor[0] = ( ( dwColor & 0x00ff0000 ) >> 16L ) / 255.0F;
    vColor[1] = ( ( dwColor & 0x0000ff00 ) >> 8L ) / 255.0F;
    vColor[2] = ( ( dwColor & 0x000000ff ) >> 0L ) / 255.0F;
    vColor[3] = ( ( dwColor & 0xff000000 ) >> 24L ) / 255.0F;

    // Set up stuff to prepare for drawing text
    Begin();

    // Perform the actual storing of the color constant here to prevent
    // a load-hit-store by inserting work between the store and the use of
    // the vColor array.
    pd3dDevice->SetVertexShaderConstantF( 1, vColor, 1 );

    // Set the starting screen position
    if( ( fOriginX < 0.0f ) || ( ( dwFlags & ATGFONT_RIGHT ) && ( fOriginX <= 0.0f ) ) )
        fOriginX += ( m_rcWindow.x2 - m_rcWindow.x1 );
    if( fOriginY < 0.0f )
        fOriginY += ( m_rcWindow.y2 - m_rcWindow.y1 );

    m_fCursorX = floorf( fOriginX );
    m_fCursorY = floorf( fOriginY );

    // Adjust for padding
    fOriginY -= m_fFontTopPadding;

    float fEllipsesPixelWidth = m_fXScaleFactor * 3.0f * ( m_Glyphs[m_TranslatorTable[L'.']].wOffset +
                                                           m_Glyphs[m_TranslatorTable[L'.']].wAdvance );

    if( dwFlags & ATGFONT_TRUNCATED )
    {
        // Check if we will really need to truncate the string
        if( fMaxPixelWidth <= 0.0f )
            dwFlags &= ( ~ATGFONT_TRUNCATED );
        else
        {
            float w, h;
            GetTextExtent( strText, &w, &h, TRUE );

            // If not, then clear the flag
            if( w <= fMaxPixelWidth )
                dwFlags &= ( ~ATGFONT_TRUNCATED );
        }
    }

    // If vertically centered, offset the starting m_fCursorY value
    if( dwFlags & ATGFONT_CENTER_Y )
    {
        float w, h;
        GetTextExtent( strText, &w, &h );
        m_fCursorY = floorf( m_fCursorY - (h * 0.5f) );
    }

    // Add window offsets
    float Winx = static_cast<float>(m_rcWindow.x1);
    float Winy = static_cast<float>(m_rcWindow.y1);
    fOriginX += Winx;
    fOriginY += Winy;
    m_fCursorX += Winx;
    m_fCursorY += Winy;

    // Set a flag so we can determine initial justification effects
    BOOL bStartingNewLine = TRUE;

    unsigned long dwNumEllipsesToDraw = 0;

    // Begin drawing the vertices

    // Declared as volatile to force writing in ascending
    // address order. It prevents out of sequence writing in write combined
    // memory.

    volatile float * pVertex;

    unsigned long dwNumChars = wcslen( strText ) + ( dwFlags & ATGFONT_TRUNCATED ? 3 : 0 );
    HRESULT hr = pd3dDevice->BeginVertices( D3DPT_QUADLIST, 4 * dwNumChars, sizeof( XMFLOAT4 ) ,
                                              ( VOID** )&pVertex );
    // The ring buffer may run out of space when tiling, doing z-prepasses,
    // or using BeginCommandBuffer. If so, make the buffer larger.
    if( FAILED( hr ) )
        SSNES_ERR( "Ring buffer out of memory.\n" );

    bStartingNewLine = TRUE;

    // Draw four vertices for each glyph
    while( *strText )
    {
        wchar_t letter;

        if( dwNumEllipsesToDraw )
            letter = L'.';
        else
        {
            // If starting text on a new line, determine justification effects
            if( bStartingNewLine )
            {
                if( dwFlags & ( ATGFONT_RIGHT | ATGFONT_CENTER_X ) )
                {
                    // Get the extent of this line
                    float w, h;
                    GetTextExtent( strText, &w, &h, TRUE );

                    // Offset this line's starting m_fCursorX value
                    if( dwFlags & ATGFONT_RIGHT )
                        m_fCursorX = floorf( fOriginX - w );
                    if( dwFlags & ATGFONT_CENTER_X )
                        m_fCursorX = floorf( fOriginX - w * 0.5f );
                }
                bStartingNewLine = FALSE;
            }

            // Get the current letter in the string
            letter = *strText++;

            // Handle the newline character
            if( letter == L'\n' )
            {
                m_fCursorX = fOriginX;
                m_fCursorY += m_fFontYAdvance * m_fYScaleFactor;
                bStartingNewLine = TRUE;
                continue;
            }

            // Handle carriage return characters by ignoring them. This helps when
            // displaying text from a file.
            if( letter == L'\r' )
                continue;
        }

        // Translate unprintable characters
        const GLYPH_ATTR * pGlyph = &m_Glyphs[ ( letter <= m_cMaxGlyph ) ? m_TranslatorTable[letter] : 0 ];

        float fOffset = m_fXScaleFactor * (float)pGlyph->wOffset;
        float fAdvance = m_fXScaleFactor * (float)pGlyph->wAdvance;
        float fWidth = m_fXScaleFactor * (float)pGlyph->wWidth;
        float fHeight = m_fYScaleFactor * m_fFontHeight;

        if( 0 == dwNumEllipsesToDraw )
        {
            if( dwFlags & ATGFONT_TRUNCATED )
            {
                // Check if we will be exceeded the max allowed width
                if( m_fCursorX + fOffset + fWidth + fEllipsesPixelWidth + m_fSlantFactor > fOriginX + fMaxPixelWidth )
                {
                    // Yup, draw the three ellipses dots instead
                    dwNumEllipsesToDraw = 3;
                    continue;
                }
            }
        }

        // Setup the screen coordinates
        m_fCursorX += fOffset;
        float X4 = m_fCursorX;
        float X1 = X4 + m_fSlantFactor;
        float X3 = X4 + fWidth;
        float X2 = X1 + fWidth;
        float Y1 = m_fCursorY;
        float Y3 = Y1 + fHeight;
        float Y2 = Y1;
        float Y4 = Y3;

        // Rotate the points by the rotation factor
        if( m_bRotate )
        {
            RotatePoint( &X1, &Y1, fOriginX, fOriginY );
            RotatePoint( &X2, &Y2, fOriginX, fOriginY );
            RotatePoint( &X3, &Y3, fOriginX, fOriginY );
            RotatePoint( &X4, &Y4, fOriginX, fOriginY );
        }

        m_fCursorX += fAdvance;

        // Select the RGBA channel that the compressed glyph is stored in
        // Takes a 4 bit per pixel ARGB value and expand it to an 8 bit per pixel ARGB value

        unsigned long dwChannelSelector = pGlyph->wMask;        // Convert to 32 bit
        // Perform the conversion without branching

        // Splat the 4 bit per pixels from 0x1234 to 0x01020304
        dwChannelSelector = ((dwChannelSelector&0xF000)<<(24-12))|((dwChannelSelector&0xF00)<<(16-8))|
            ((dwChannelSelector&0xF0)<<(8-4))|(dwChannelSelector&0xF);

        // Perform a vectorized multiply to make 0x01020304 into 0x11223344
        dwChannelSelector *= 0x11;

        // Add the vertices to draw this glyph

        unsigned long tu1 = pGlyph->tu1;        // Convert shorts to 32 bit longs for in register merging
        unsigned long tv1 = pGlyph->tv1;
        unsigned long tu2 = pGlyph->tu2;
        unsigned long tv2 = pGlyph->tv2;

        // NOTE: The vertexs are 2 floats for the screen coordinates,
        // followed by two USHORTS for the u/vs of the character,
        // terminated with the ARGB 32 bit color.
        // This makes for 16 bytes per vertex data (Easier to read)
        // Second NOTE: The uvs are merged and written using a DWORD due
        // to the write combining hardware being only able to handle 32,
        // 64 and 128 writes. Never store to write combined memory with
        // 8 or 16 bit instructions. You've been warned.

        pVertex[0] = X1;
        pVertex[1] = Y1;
        reinterpret_cast<volatile unsigned long *>(pVertex)[2] = (tu1<<16)|tv1;         // Merged using big endian rules
        reinterpret_cast<volatile unsigned long *>(pVertex)[3] = dwChannelSelector;
        pVertex[4] = X2;
        pVertex[5] = Y2;
        reinterpret_cast<volatile unsigned long *>(pVertex)[6] = (tu2<<16)|tv1;         // Merged using big endian rules
        reinterpret_cast<volatile unsigned long *>(pVertex)[7] = dwChannelSelector;
        pVertex[8] = X3;
        pVertex[9] = Y3;
        reinterpret_cast<volatile unsigned long *>(pVertex)[10] = (tu2<<16)|tv2;        // Merged using big endian rules
        reinterpret_cast<volatile unsigned long *>(pVertex)[11] = dwChannelSelector;
        pVertex[12] = X4;
        pVertex[13] = Y4;
        reinterpret_cast<volatile unsigned long *>(pVertex)[14] = (tu1<<16)|tv2;        // Merged using big endian rules
        reinterpret_cast<volatile unsigned long *>(pVertex)[15] = dwChannelSelector;
        pVertex+=16;

        // If drawing ellipses, exit when they're all drawn
        if( dwNumEllipsesToDraw )
        {
            if( --dwNumEllipsesToDraw == 0 )
                break;
        }

        dwNumChars--;
    }

    // Since we allocated vertex data space based on the string length, we now need to
    // add some dummy verts for any skipped characters (like newlines, etc.)
    while( dwNumChars )
    {
        pVertex[0] = 0;
        pVertex[1] = 0;
        pVertex[2] = 0;
        pVertex[3] = 0;
        pVertex[4] = 0;
        pVertex[5] = 0;
        pVertex[6] = 0;
        pVertex[7] = 0;
        pVertex[8] = 0;
        pVertex[9] = 0;
        pVertex[10] = 0;
        pVertex[11] = 0;
        pVertex[12] = 0;
        pVertex[13] = 0;
        pVertex[14] = 0;
        pVertex[15] = 0;
        pVertex+=16;
        dwNumChars--;
    }

    // Stop drawing vertices
    pd3dDevice->EndVertices();

    // Undo window offsets
    m_fCursorX -= Winx;
    m_fCursorY -= Winy;

    // Call End() to complete the begin/end pair for drawing text
    End();

    // Close off the user-defined event opened with PIXBeginNamedEvent.
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: End()
// Desc: Paired call that restores state set in the Begin() call.
//--------------------------------------------------------------------------------------
VOID XdkFont::End()
{
    if( --m_dwNestedBeginCount > 0 )
    {
        PIXEndNamedEvent();
        return;
    }

    // Restore state
    if( m_bSaveState )
    {
        // Cache the global pointer into a register
		xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
		D3DDevice *pD3dDevice = vid->xdk360_render_device;

        pD3dDevice->SetTexture( 0, NULL );
        pD3dDevice->SetVertexDeclaration( NULL );
        pD3dDevice->SetVertexShader( NULL );
        pD3dDevice->SetPixelShader( NULL );
        pD3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHABLENDENABLE ] );
        pD3dDevice->SetRenderState( D3DRS_SRCBLEND, m_dwSavedState[ SAVEDSTATE_D3DRS_SRCBLEND ] );
        pD3dDevice->SetRenderState( D3DRS_DESTBLEND, m_dwSavedState[ SAVEDSTATE_D3DRS_DESTBLEND ] );
        pD3dDevice->SetRenderState( D3DRS_BLENDOP, m_dwSavedState[ SAVEDSTATE_D3DRS_BLENDOP ] );
        pD3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHATESTENABLE ] );
        pD3dDevice->SetRenderState( D3DRS_ALPHAREF, m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAREF ] );
        pD3dDevice->SetRenderState( D3DRS_ALPHAFUNC, m_dwSavedState[ SAVEDSTATE_D3DRS_ALPHAFUNC ] );
        pD3dDevice->SetRenderState( D3DRS_FILLMODE, m_dwSavedState[ SAVEDSTATE_D3DRS_FILLMODE ] );
        pD3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwSavedState[ SAVEDSTATE_D3DRS_CULLMODE ] );
        pD3dDevice->SetRenderState( D3DRS_ZENABLE, m_dwSavedState[ SAVEDSTATE_D3DRS_ZENABLE ] );
        pD3dDevice->SetRenderState( D3DRS_STENCILENABLE, m_dwSavedState[ SAVEDSTATE_D3DRS_STENCILENABLE ] );
        pD3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, m_dwSavedState[ SAVEDSTATE_D3DRS_VIEWPORTENABLE ] );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, m_dwSavedState[ SAVEDSTATE_D3DSAMP_MINFILTER ] );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, m_dwSavedState[ SAVEDSTATE_D3DSAMP_MAGFILTER ] );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSU ] );
        pD3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, m_dwSavedState[ SAVEDSTATE_D3DSAMP_ADDRESSV ] );
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: RotatePoint()
// Desc: Rotate a 2D point around the origin
//-------------------------------------------------------------------------------------
void XdkFont::RotatePoint( float * X, float * Y, double OriginX, double OriginY ) const
{
    double dTempX = *X - OriginX;
    double dTempY = *Y - OriginY;
    double dXprime = OriginX + ( m_dRotCos * dTempX - m_dRotSin * dTempY );
    double dYprime = OriginY + ( m_dRotSin * dTempX + m_dRotCos * dTempY );

    *X = static_cast<float>( dXprime );
    *Y = static_cast<float>( dYprime );
}
