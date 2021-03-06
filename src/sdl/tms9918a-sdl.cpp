//----------------------------------------------------------------------------
//
// File:        tms9918a-sdl.cpp
// Date:        02-Apr-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains SDL specific code for the TMS9918A
//
// Copyright (c) 2000-2003 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <memory.h>
#include <stdio.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "ti994a.hpp"
#include "SDL/SDL.h"
#include "bitmap.hpp"
#include "tms9918a-sdl.hpp"

#include "psp_sdl.h"
#include "psp_ti99.h"

DBG_REGISTER ( __FILE__ );

cSdlTMS9918A::cSdlTMS9918A ( sRGBQUAD colorTable [17], int refreshRate, bool useScale2x, bool fullScreen, int width, int height ) :
    cTMS9918A ( refreshRate ),
    m_TableSize ( 1 ),
    m_TextMode ( false ),
    m_ChangesMade ( false ),
    m_BlankChanged ( false ),
    m_ColorsChanged ( false ),
    m_SpritesChanged ( false ),
    m_NeedsUpdate ( false ),
    m_Scale2x ( useScale2x ),
    m_Screen ( NULL ),
    m_BitmapScreen ( NULL ),
    m_BitmapSpriteScreen ( NULL ),
    m_CharacterPattern ( NULL ),
    m_BytesPerPixel ( 0 ),
    m_Mutex ( NULL ),
    m_FullScreen ( false ),
    m_OnFrames ( 1 ),
    m_OffFrames ( 0 ),
    m_FrameCycle ( 1 )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A ctor", true );

    m_Mutex = SDL_CreateMutex ();

    memset ( m_RawColorTable, 0, sizeof ( m_RawColorTable ));
    memset ( m_SDLColorTable, 0, sizeof ( m_SDLColorTable ));

    memset ( m_ScreenChanged, 0, sizeof ( m_ScreenChanged ));

    memset ( m_CharUse, 0, sizeof ( m_CharUse ));
    memset ( m_SpriteCharUse, 0, sizeof ( m_SpriteCharUse ));
    memset ( m_PatternChanged, 0, sizeof ( m_PatternChanged ));

    m_CharacterPattern    = new UCHAR [ 3 * 256 * 8 * 8 ];
    memset ( m_CharacterPattern, 0, 3 * 256 * 8 * 8 );

    // See if we're starting if fullscreen mode
# if 0
    if ( fullScreen == true ) {
        m_Screen = CreateMainWindowFullScreen ( width, height );
    }
# endif

    if ( m_Screen == NULL ) {
        if ( width == 0 ) width = VDP_WIDTH;
        if ( height == 0 ) height = VDP_HEIGHT;
        m_Screen = CreateMainWindow ( width, height );
    }

    m_BitmapScreen       = CreateBitMap ( VDP_WIDTH, VDP_HEIGHT );
    m_BitmapSpriteScreen = CreateBitMap ( VDP_WIDTH, VDP_HEIGHT );

    SetColorTable ( colorTable );

    m_BytesPerPixel = m_Screen->GetSurface ()->format->BitsPerPixel / 8;

}

cSdlTMS9918A::~cSdlTMS9918A ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A dtor", true );

    SDL_DestroyMutex ( m_Mutex );

    delete [] m_CharacterPattern;

    delete m_BitmapSpriteScreen;
    delete m_BitmapScreen;
    delete m_Screen;
}

cBitMap *cSdlTMS9918A::CreateMainWindow ( int width, int height )
{
# if 0 //LUDO:
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::CreateMainWindow", true );

    TRACE ( "Geometry: " << width << "x" << height );

    SDL_Surface *screen = SDL_SetVideoMode ( width, height, 0, SDL_HWSURFACE | SDL_RESIZABLE );
# else
    SDL_Surface *screen = blit_surface;
# endif
    m_FullScreen = false;

    return new cBitMap ( screen, m_Scale2x );
}

int cSdlTMS9918A::GetFullScreenResolutions ( int *x, int *y, int n )
{
    FUNCTION_ENTRY ( NULL, "cSdlTMS9918A::GetFullScreenResolutions", true );

    if (( x == NULL ) || ( y == NULL ) || ( n == 0 )) return 0;

    // Get the list of supported resolutions we can pick from
    SDL_Rect **modes = SDL_ListModes ( NULL, SDL_FULLSCREEN );

    if ( modes == NULL ) return 0;

    // Unlimited number of resolutions
    if ( modes == ( SDL_Rect ** ) -1 ) return -1;

    // Find out how many there are
    int max = 0;
    while ( modes [max+1] != NULL ) max++;

    int count = 0;
    for ( int i = max; i >= 0; i-- ) {
        double xScale = ( double ) modes [i]->w / ( double ) VDP_WIDTH;
        double yScale = ( double ) modes [i]->h / ( double ) VDP_HEIGHT;
        // We don't want to scale down
        if (( xScale < 1.0 ) || ( yScale < 1.0 )) continue;
        // We to preserve the aspect ratio
        if ( xScale != yScale ) continue;
        x [count] = modes [i]->w;
        y [count] = modes [i]->h;
        if ( ++count == n ) break;
    }

    return count;
}

cBitMap *cSdlTMS9918A::CreateMainWindowFullScreen ( int width, int height )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::CreateMainWindowFullScreen", true );

    if (( width == 0 ) || ( height == 0 )) {

        // Get the list of supported resolutions we can pick from
        SDL_Rect **modes = SDL_ListModes ( NULL, SDL_FULLSCREEN );

        if ( modes == NULL ) {
            fprintf ( stderr, "No video modes available!\n" );
            return NULL;
        }

        // Set our default sizes in case there's not any matches
        width   = VDP_WIDTH;
        height  = VDP_HEIGHT;

        int bestGap = 10000;

        // Find the lowest resolution that matches our aspect ratio & size best
        if ( modes != ( SDL_Rect ** ) -1 ) {
            int max = 0;
            while ( modes [max+1] != NULL ) max++;
            for ( int i = max; i >= 0; i-- ) {
                double xScale = ( double ) modes [i]->w / ( double ) VDP_WIDTH;
                double yScale = ( double ) modes [i]->h / ( double ) VDP_HEIGHT;
                // We don't want to scale down
                if (( xScale < 1.0 ) || ( yScale < 1.0 )) continue;
                // We want to preserve the aspect ratio
                if ( xScale != yScale ) continue;
                int gap = modes [i]->w / ( int ) xScale - VDP_WIDTH;
                if ( gap < bestGap ) {
                    width   = modes [i]->w;
                    height  = modes [i]->h;
                    bestGap = gap;
                    if ( gap == 0 ) break;
                }
            }
        }
    }

    TRACE ( "Geometry: " << width << "x" << height );

    SDL_Surface *screen = SDL_SetVideoMode ( width, height, 8, SDL_FULLSCREEN );

    if (( screen == NULL ) || (( screen->flags & SDL_FULLSCREEN ) == 0 )) {
        ERROR ( "Unable to set video mode to full screen" );
        if ( screen != NULL ) SDL_FreeSurface ( screen );
        return NULL;
    }

    m_FullScreen = true;

    SDL_ShowCursor ( SDL_DISABLE );

    return new cBitMap ( screen, m_Scale2x );
}

void cSdlTMS9918A::ResizeWindow ( int width, int height )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::ResizeWindow", true );

    if ( m_FullScreen == true ) return;

# if 0 //TO_BE_DONE
    SDL_mutexP ( m_Mutex );
# endif

    delete m_Screen;
    
    m_Screen = CreateMainWindow ( width, height );

    // Make sure we fill in around the edges
    BlankScreen ( true );
    
    // Force a repaint during the next call to Refresh
    m_ChangesMade    = true;
    m_SpritesChanged = true;
    memset ( m_ScreenChanged, true, sizeof ( m_ScreenChanged ));
    memset ( m_PatternChanged, true, sizeof ( m_PatternChanged ));

# if 0 //TO_BE_DONE
    SDL_mutexV ( m_Mutex );
# endif
}

cBitMap *cSdlTMS9918A::CreateBitMap ( int width, int height )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::CreateBitMap", true );

    SDL_Surface *screen = m_Screen->GetSurface ();

    int depth = screen->format->BitsPerPixel;

    Uint32 rMask = screen->format->Rmask;
    Uint32 gMask = screen->format->Gmask;
    Uint32 bMask = screen->format->Bmask;
    Uint32 aMask = screen->format->Amask;

    SDL_Surface *surface = SDL_CreateRGBSurface ( SDL_SWSURFACE, width, height, depth, rMask, gMask, bMask, aMask );

    if ( depth <= 8 ) {
        SDL_SetColors ( surface, m_SDLColorTable, 0, SIZE ( m_SDLColorTable ));
    }

    return new cBitMap ( surface, false );
}

void cSdlTMS9918A::SetColorTable ( sRGBQUAD colorTable [17] )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::SelectColorTable", true );

    if ( memcmp ( m_RawColorTable, colorTable, sizeof ( m_RawColorTable )) != 0 ) {
        memcpy ( m_RawColorTable, colorTable, sizeof ( m_RawColorTable ));
        ConvertColors ();
    }
}

void cSdlTMS9918A::SetFrameRate ( int onFrames, int offFrames )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::SetFrequency", true );

    ASSERT ( onFrames > 0 );
    ASSERT ( offFrames >= 0 );

    // Start off on an "on" frame
    m_FrameCycle = onFrames;
    m_OnFrames   = onFrames;
    m_OffFrames  = offFrames;
}

void cSdlTMS9918A::ConvertColors ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::ConvertColors", true );

# if 0 //TO_BE_DONE
    SDL_mutexP ( m_Mutex );
# endif

    SDL_PixelFormat *format = m_Screen->GetSurface ()->format;

    memcpy ( m_SDLColorTable, m_RawColorTable, sizeof ( m_SDLColorTable ));

    if ( format->BitsPerPixel == 8 ) {
        m_Screen->SetPalette ( m_SDLColorTable, SIZE ( m_SDLColorTable ));
        m_BitmapScreen->SetPalette ( m_SDLColorTable, SIZE ( m_SDLColorTable ));
        m_BitmapSpriteScreen->SetPalette ( m_SDLColorTable, SIZE ( m_SDLColorTable ));
    } else {
        for ( unsigned i = 0; i < SIZE ( m_SDLColorTable ); i++ ) {
            sRGBQUAD *src = &m_RawColorTable [i];
            ULONG color = SDL_MapRGB ( format, src->r, src->g, src->b );
            * ( ULONG * ) ( m_SDLColorTable + i ) = color;
        }
    }

    m_ChangesMade    = true;
    m_SpritesChanged = true;
    memset ( m_ScreenChanged, true, sizeof ( m_ScreenChanged ));
    memset ( m_PatternChanged, true, sizeof ( m_PatternChanged ));

# if 0 //TO_BE_DONE
    SDL_mutexV ( m_Mutex );
# endif
}

void cSdlTMS9918A::Reset ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::Reset", true );

    cTMS9918A::Reset ();

    m_TextMode       = false;
    m_TableSize      = 1;
    m_ChangesMade    = true;
    m_BlankChanged   = true;
    m_ColorsChanged  = true;
    m_SpritesChanged = false;

    memset ( m_ScreenChanged, false, sizeof ( m_ScreenChanged ));
    memset ( m_PatternChanged, false, sizeof ( m_PatternChanged ));
    memset ( m_CharUse, 0, sizeof ( m_CharUse ));
    memset ( m_SpriteCharUse, 0, sizeof ( m_SpriteCharUse ));

    if ( m_Mode & VDP_M3 ) {
        m_CharUse [0x0000] = m_ImageTableSize / 3;
        m_CharUse [0x0100] = m_ImageTableSize / 3;
        m_CharUse [0x0200] = m_ImageTableSize / 3;
    } else {
        m_CharUse [0x0000] = m_ImageTableSize;
    }
    m_SpriteCharUse [0] = 32;
}

void cSdlTMS9918A::UpdateCharacterPatternGraphics ( int ch, UCHAR fore, UCHAR back, UCHAR *pattern )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateCharacterPatternGraphics", false );

    ASSERT ( ch < 256 );

    UCHAR *pData = m_CharacterPattern + ch * 8 * 8;

    for ( int y = 0; y < 8; y++ ) {
        UCHAR row = *pattern++;
        *pData++ = ( row & 0x80 ) ? fore : back;
        *pData++ = ( row & 0x40 ) ? fore : back;
        *pData++ = ( row & 0x20 ) ? fore : back;
        *pData++ = ( row & 0x10 ) ? fore : back;
        *pData++ = ( row & 0x08 ) ? fore : back;
        *pData++ = ( row & 0x04 ) ? fore : back;
        *pData++ = ( row & 0x02 ) ? fore : back;
        *pData++ = ( row & 0x01 ) ? fore : back;
    }
}

void cSdlTMS9918A::UpdateCharacterPatternBitMap ( int ch, UCHAR *pattern )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateCharacterPatternBitMap", false );

    ASSERT ( ch < 3 * 256 );

    UCHAR *pColor = m_ColorTable->data + ch * 8;
    UCHAR *pData  = m_CharacterPattern + ch * 8 * 8;

    for ( int y = 0; y < 8; y++ ) {
        UCHAR row  = *pattern++;
        UCHAR fore = ( UCHAR ) ( pColor [y] >> 4 );
        UCHAR back = ( UCHAR ) ( pColor [y] & 0x0F );
        *pData++ = ( row & 0x80 ) ? fore : back;
        *pData++ = ( row & 0x40 ) ? fore : back;
        *pData++ = ( row & 0x20 ) ? fore : back;
        *pData++ = ( row & 0x10 ) ? fore : back;
        *pData++ = ( row & 0x08 ) ? fore : back;
        *pData++ = ( row & 0x04 ) ? fore : back;
        *pData++ = ( row & 0x02 ) ? fore : back;
        *pData++ = ( row & 0x01 ) ? fore : back;
    }
}

void cSdlTMS9918A::UpdateScreenGraphics ( int x, int y, int ch )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateScreenGraphics", false );

    ASSERT ( x < 32 );
    ASSERT ( y < 24 );
    ASSERT ( ch < 3 * 256 );

    UCHAR *pSrcData = m_CharacterPattern + ch * 8 * 8;
    UCHAR *pDstData = m_BitmapScreen->GetData ();

    int dstPitch = m_BitmapScreen->Pitch ();

    ASSERT ( m_BytesPerPixel <= 4 );

    pDstData += y * 8 * dstPitch + x * 8 * m_BytesPerPixel;

    switch ( m_BytesPerPixel ) {
        case 1 :
            for ( y = 0; y < 8; y++ ) {
                UCHAR *pDst = ( UCHAR * ) pDstData;
                for ( x = 0; x < 8; x++ ) {
                    *pDst++ = *pSrcData++;
                }
                pDstData += dstPitch;
            }
            break;
        case 2 :
            for ( y = 0; y < 8; y++ ) {
                USHORT *pDst = ( USHORT * ) pDstData;
                for ( x = 0; x < 8; x++ ) {
                    *pDst++ = * ( USHORT * ) &m_SDLColorTable [ *pSrcData++ ];
                }
                pDstData += dstPitch;
            }
            break;
        case 4 :
            for ( y = 0; y < 8; y++ ) {
                ULONG *pDst = ( ULONG * ) pDstData;
                for ( x = 0; x < 8; x++ ) {
                    *pDst++ = * ( ULONG * ) &m_SDLColorTable [ *pSrcData++ ];
                }
                pDstData += dstPitch;
            }
            break;
    }
}

void cSdlTMS9918A::UpdateScreenText ( int x, int y, int ch )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateScreenText", false );

    ASSERT ( x < 40 );
    ASSERT ( y < 24 );
    ASSERT ( ch < 256 );

    UCHAR *pSrcData = m_CharacterPattern + ch * 8 * 8;
    UCHAR *pDstData = m_BitmapScreen->GetData ();

    int dstPitch = m_BitmapScreen->Pitch ();

    pDstData += y * 8 * dstPitch + x * 6 * m_BytesPerPixel + 8 * m_BytesPerPixel;

    switch ( m_BytesPerPixel ) {
        case 1 :
            for ( y = 0; y < 8; y++ ) {
                UCHAR *pDst = ( UCHAR * ) pDstData;
                for ( x = 0; x < 6; x++ ) {
                    *pDst++ = *pSrcData++;
                }
                pSrcData += 2;
                pDstData += dstPitch;
            }
            break;
        case 2 :
            for ( y = 0; y < 8; y++ ) {
                USHORT *pDst = ( USHORT * ) pDstData;
                for ( x = 0; x < 6; x++ ) {
                    *pDst++ = * ( USHORT * ) &m_SDLColorTable [ *pSrcData++ ];
                }
                pSrcData += 2;
                pDstData += dstPitch;
            }
            break;
        case 4 :
            for ( y = 0; y < 8; y++ ) {
                ULONG *pDst = ( ULONG * ) pDstData;
                for ( x = 0; x < 6; x++ ) {
                    *pDst++ = * ( ULONG * ) &m_SDLColorTable [ *pSrcData++ ];
                }
                pSrcData += 2;
                pDstData += dstPitch;
            }
            break;
    }
}

void cSdlTMS9918A::UpdateScreenMultiColor ( int x, int y, int ch )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateScreenMultiColor", false );

    ASSERT ( x < 32 );
    ASSERT ( y < 24 );
    ASSERT ( ch < 256 );

    UCHAR back = ( UCHAR ) ( m_Register [7] & 0x0F );

    UCHAR *pSrcData = &m_PatternTable->data [ch][( y & 0x03 ) * 2 ];
    UCHAR *pDstData = m_BitmapScreen->GetData ();

    int dstPitch = m_BitmapScreen->Pitch ();

    pDstData += y * 8 * dstPitch + x * 8 * m_BytesPerPixel;

    for ( int i = 0; i < 2; i++ ) {

        int leftColor = ( *pSrcData & 0xF0 ) >> 4;
        if ( leftColor == 0 ) leftColor = back;

        int rightColor = *pSrcData & 0x0F;
        if ( rightColor == 0 ) rightColor = back;

        switch ( m_BytesPerPixel ) {
            case 1 :
                for ( y = 0; y < 4; y++ ) {
                    UCHAR *pDst = ( UCHAR * ) pDstData;
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = ( UCHAR ) leftColor;
                    }
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = ( UCHAR ) rightColor;
                    }
                    pDstData += dstPitch;
                }
                break;
            case 2 :
                for ( y = 0; y < 4; y++ ) {
                    USHORT *pDst = ( USHORT * ) pDstData;
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = * ( USHORT * ) &m_SDLColorTable [ leftColor ];
                    }
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = * ( USHORT * ) &m_SDLColorTable [ rightColor ];
                    }
                    pDstData += dstPitch;
                }
                break;
            case 4 :
                for ( y = 0; y < 4; y++ ) {
                    ULONG *pDst = ( ULONG * ) pDstData;
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = * ( ULONG * ) &m_SDLColorTable [ leftColor ];
                    }
                    for ( x = 0; x < 4; x++ ) {
                        *pDst++ = * ( ULONG * ) &m_SDLColorTable [ rightColor ];
                    }
                    pDstData += dstPitch;
                }
                break;
        }

        pSrcData++;
    }
}

void cSdlTMS9918A::MarkScreenChanges ( int ch )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::MarkScreenChanges", false );

    ASSERT ( ch < 3 * 256 );

    UCHAR *screen = ( UCHAR * ) m_ImageTable;
    bool *changed = m_ScreenChanged;
    int count     = m_CharUse [ch];
    int max       = m_ImageTableSize;

    if ( m_Mode & VDP_M3 ) {
        int index = ch & 0xFF00;
        screen  += index;
        changed += index;
        ch  &= 0xFF;
        max /= 3;
    }

    for ( int i = max; i; i-- ) {
        if ( *screen++ == ch ) {
            *changed = true;
            if ( --count == 0 ) break;
        }
        changed++;
    }
}

void cSdlTMS9918A::DrawSprite ( int index )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::DrawSprite", false );

    sSpriteAttributeEntry *sprite = &m_SpriteAttrTable->data [index];

    UCHAR colorIndex = ( UCHAR ) ( sprite->earlyClock & 0x0F );
    if ( colorIndex == 0 ) return;

    int count = ( m_Register [1] & VDP_SPRITE_SIZE ) ? 4 : 1;
    int size  = ( m_Register [1] & VDP_SPRITE_MAGNIFY ) ? 16 : 8;

    UCHAR *pScreen = m_BitmapSpriteScreen->GetData ();
    int pitch = m_BitmapSpriteScreen->Pitch ();

    for ( int i = 0; i < count; i++ ) {

        int posX = ( int ) sprite->posX + ( i / 2 ) * size;
        if ( sprite->earlyClock & 0x80 ) posX -= 32;

        if ( posX >= VDP_WIDTH ) continue;

        UCHAR  row      = ( UCHAR ) ( sprite->posY + 1 + ( i % 2 ) * size );
        UCHAR *pDstData = pScreen + posX * m_BytesPerPixel;
        UCHAR *pattern  = m_SpriteDescTable->data [(sprite->patternIndex+i)%256];

        for ( int y = 0; y < size; y++, row++ ) {

            // Make sure the current row and sprite are visible
            if (( row < VDP_HEIGHT ) && ( index <= m_MaxSprite [row] )) {

                UCHAR bits   = *pattern;
                UCHAR *pData = pDstData + row * pitch;

                if ( bits ) for ( int x = 0, col = posX; x < size; x++, col++ ) {
                    if ( col >= VDP_WIDTH ) break;
                    if (( bits & 0x80 ) && ( col >= 0 )) {
                        switch ( m_BytesPerPixel ) {
                            case 1 :
                                *pData = colorIndex;
                                break;
                            case 2 :
                                * ( USHORT * ) pData = * ( USHORT * ) &m_SDLColorTable [ colorIndex ];
                                break;
                            case 4 :
                                * ( ULONG * ) pData = * ( ULONG * ) &m_SDLColorTable [ colorIndex ];
                                break;
                        }
                    }
                    pData += m_BytesPerPixel;
                    if (( size == 8 ) || ( x & 1 )) bits <<= 1;
                }
            }

            if (( size == 8 ) || ( y & 1 )) pattern++;
        }
    }
}

bool cSdlTMS9918A::RefreshGraphics ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::RefreshGraphics", false );

    // Check for changes in the character patterns
    UCHAR fore = ( UCHAR ) ( m_Register [7] >> 4 );
    UCHAR back = ( UCHAR ) ( m_Register [7] & 0x0F );
    for ( int ch = 0; ch < 256; ch++ ) {
        if ( ! m_TextMode && ( ch & 0x07 ) == 0 ) {
            fore = ( UCHAR ) ( m_ColorTable->data [ ch / 8 ] >> 4 );
            back = ( UCHAR ) ( m_ColorTable->data [ ch / 8 ] & 0x0F );
        }
        if ( m_PatternChanged [ch] && m_CharUse [ch] ) {
            UpdateCharacterPatternGraphics ( ch, fore, back, ( UCHAR * ) &m_PatternTable->data [ch] );
            m_PatternChanged [ch] = false;
            MarkScreenChanges ( ch );
        }
    }

    bool needsUpdate = false;
    bool *changed = m_ScreenChanged;
    UCHAR *chr = ( UCHAR * ) m_ImageTable;

    m_BitmapScreen->LockSurface ();

    int height = GetScreenHeight ();
    int width  = GetScreenWidth ();
    for ( int y = 0; y < height; y++ ) {
        for ( int x = 0; x < width; x++ ) {
            if ( *changed++ ) {
                if ( m_TextMode ) {
                    UpdateScreenText ( x, y, *chr++ );
                } else {
                    UpdateScreenGraphics ( x, y, *chr++ );
                }
                needsUpdate = true;
            } else {
                chr++;
            }
        }
    }

    if ( needsUpdate ) {
        memset ( m_ScreenChanged, false, sizeof ( m_ScreenChanged ));
    }

    if ( m_TextMode && m_ColorsChanged ) {

        int x;
        UCHAR *pDstData = ( UCHAR * ) m_BitmapScreen->GetData ();
        UCHAR *pLeft    = pDstData;
        UCHAR *pRight   = pDstData + ( 40 * 6 + 8 ) * m_BytesPerPixel;

        switch ( m_BytesPerPixel ) {
            case 1 :
                {
                    UCHAR *pDst = ( UCHAR * ) pDstData;
                    for ( x = 0; x < 8; x++ ) {
                        *pDst++ = ( UCHAR ) back;
                    }
                }
                break;
            case 2 :
                {
                    USHORT *pDst = ( USHORT * ) pDstData;
                    for ( x = 0; x < 8; x++ ) {
                        *pDst++ = * ( USHORT * ) &m_SDLColorTable [ back ];
                    }
                }
                break;
            case 4 :
                {
                    ULONG *pDst = ( ULONG * ) pDstData;
                    for ( x = 0; x < 8; x++ ) {
                        *pDst++ = * ( ULONG * ) &m_SDLColorTable [ back ];
                    }
                }
                break;
        }

        int pitch = m_BitmapScreen->Pitch ();

        for ( int y = 0; y < height * 8; y++ ) {
            memcpy ( pLeft, pDstData, 8 * m_BytesPerPixel );
            memcpy ( pRight, pDstData, 8 * m_BytesPerPixel );
            pLeft  += pitch;
            pRight += pitch;
        }

        m_ColorsChanged = false;
    }

    m_BitmapScreen->UnlockSurface ();

    return needsUpdate;
}

bool cSdlTMS9918A::RefreshBitMap ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::RefreshBitMap", false );

    // Check for changes in the character patterns
    for ( int ch = 0; ch < 3 * 256; ch++ ) {
        if ( m_PatternChanged [ch] && m_CharUse [ch] ) {
            UpdateCharacterPatternBitMap ( ch, ( UCHAR * ) &m_PatternTable->data [ch] );
            m_PatternChanged [ch] = false;
            MarkScreenChanges ( ch );
        }
    }

    bool needsUpdate = false;
    UCHAR *chr = ( UCHAR * ) m_ImageTable;

    m_BitmapScreen->LockSurface ();

    for ( int i = 0; i < m_ImageTableSize; i++ ) {
        if ( m_ScreenChanged [i] ) {
            UpdateScreenGraphics ( i % 32, i / 32, ( i & 0xFF00 ) + chr [i] );
            needsUpdate = true;
        }
    }

    m_BitmapScreen->UnlockSurface ();

    if ( needsUpdate ) {
        memset ( m_ScreenChanged, false, sizeof ( m_ScreenChanged ));
    }

    return needsUpdate;
}

bool cSdlTMS9918A::RefreshMultiColor ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::RefreshMultiColor", false );

    bool needsUpdate = false;

    m_BitmapScreen->LockSurface ();
    UCHAR *chr = ( UCHAR * ) m_ImageTable;

    for ( int i = 0; i < m_ImageTableSize; i++ ) {
        UCHAR index = chr [i];
        if ( m_ScreenChanged [i] || m_PatternChanged [index] ) {
            UpdateScreenMultiColor ( i % 32, i / 32, index );
            needsUpdate = true;
        }
    }

    m_BitmapScreen->UnlockSurface ();

    if ( needsUpdate ) {
        memset ( m_ScreenChanged, false, sizeof ( m_ScreenChanged ));
        memset ( m_PatternChanged, false, sizeof ( m_PatternChanged ));
    }

    return needsUpdate;
}

void cSdlTMS9918A::UpdateSprites ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateSprites", false );

    m_BitmapSpriteScreen->Copy ( m_BitmapScreen );

    sSpriteAttributeEntry *sprite = &m_SpriteAttrTable->data [0];

    int i;
    for ( i = 0; i < 32; i++ ) {
        if ( sprite [i].posY == 0xD0 ) break;
    }

    // Draw sprites in reverse order (ie: lowest numbered sprite is on top)
    while ( --i >= 0 ) {
        DrawSprite ( i );
    }

    m_SpritesChanged = false;
}

void cSdlTMS9918A::BlankScreen ( bool bForce )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::BlankScreen", true );

    if (( bForce == false ) && (( BlankEnabled () == false ) || ( m_BlankChanged == false ))) return;

    cBitMap *screen = m_Screen;

    screen->LockSurface ();

    UCHAR back      = ( UCHAR ) ( m_Register [7] & 0x0F );
    UCHAR *pDstData = ( UCHAR * ) screen->GetData ();
    int pitch       = screen->Pitch ();
    int width       = screen->Width ();
    int height      = screen->Height ();

    switch ( m_BytesPerPixel ) {
        case 1 :
            {
                for ( int y = 0; y < height; y++ ) {
                    for ( int x = 0; x < width; x++ ) {
                        *pDstData++ = back;
                    }
                    pDstData += pitch - width;
                }
            }
           break;
        case 2 :
            {
                USHORT color = * ( USHORT * ) &m_SDLColorTable [ back ];
                for ( int y = 0; y < height; y++ ) {
                    USHORT *pData = ( USHORT * ) pDstData;
                    for ( int x = 0; x < width; x++ ) {
                        *pData++ = color;
                    }
                    pDstData += pitch;
                }
            }
            break;
        case 4 :
            {
                ULONG color = * ( ULONG * ) &m_SDLColorTable [ back ];
                for ( int y = 0; y < height; y++ ) {
                    ULONG *pData = ( ULONG * ) pDstData;
                    for ( int x = 0; x < width; x++ ) {
                        *pData++ = color;
                    }
                    pDstData += pitch;
                }
            }
            break;
    }

    m_BlankChanged = false;

    screen->UnlockSurface ();

    if ( bForce == false ) {
        SDL_UpdateRect ( screen->GetSurface (), 0, 0, width, height );
    }
}

void cSdlTMS9918A::UpdateScreen ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::UpdateScreen", false );

    cBitMap *screen = m_BitmapScreen;

    if ( m_TextMode == false ) {
        screen = m_BitmapSpriteScreen;
        UpdateSprites ();
    }

    m_Screen->Copy ( screen );
}

void cSdlTMS9918A::Refresh ( bool force )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::Refresh", false );

    // See if we should skip a frame
    if (( force == false ) && ( m_FrameCycle <= 0 )) {
        m_FrameCycle += m_OnFrames;
        return;
    }

    m_FrameCycle -= m_OffFrames;

    if ( BlankEnabled ()) {
# if 0 //TO_BE_DONE
        SDL_mutexP ( m_Mutex );
# endif
        BlankScreen ( false );
# if 0 //TO_BE_DONE
        SDL_mutexV ( m_Mutex );
# endif
        return;
    }

# if 0 //LUDO:
    if ( ! m_ChangesMade && ! m_SpritesChanged ) return;
    bool colorsChanged = m_ColorsChanged;
# endif

    if ( m_Mode & VDP_M3 ) {
        m_NeedsUpdate = RefreshBitMap ();
    } else if ( m_Mode & VDP_M2 ) {
        m_NeedsUpdate = RefreshMultiColor ();
    } else {
        m_NeedsUpdate = RefreshGraphics ();
    }

    m_ChangesMade   = false;
    m_ColorsChanged = false;

# if 0 //LUDO: FOR_TEST 0
    if ( m_NeedsUpdate | m_SpritesChanged | m_BlankChanged ) 
# endif
    {
# if 0 //TO_BE_DONE
        SDL_mutexP ( m_Mutex );
        if ( colorsChanged == true ) BlankScreen ( true );
# endif
        UpdateScreen ();
        SDL_UpdateRect ( m_Screen->GetSurface (), 0, 0, m_Screen->Width (), m_Screen->Height ());
# if 0 //TO_BE_DONE
        SDL_mutexV ( m_Mutex );
# endif
        m_NeedsUpdate = false;
    }

    psp_sdl_render();
}

cBitMap *cSdlTMS9918A::GetScreen ()
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::GetScreen", true );

    return m_Screen;
}

void cSdlTMS9918A::WriteRegister ( int reg, UCHAR value )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::WriteRegister", true );

    UCHAR oldReg = m_Register [ reg ];
    cTMS9918A::WriteRegister ( reg, value );
    UCHAR newReg = m_Register [ reg ];

    if ( oldReg == newReg ) return;

    bool patternChanged = false;

    switch ( reg ) {

        case 0 :
            patternChanged = true;
            break;

        case 1 :
            if (( oldReg ^ newReg ) & VDP_BLANK_MASK ) {
                m_BlankChanged = true;
                if (( oldReg ^ newReg ) == VDP_BLANK_MASK ) break;
            }
            if (( oldReg ^ newReg ) & VDP_SPRITE_SIZE ) {
                memset ( m_SpriteCharUse, 0, sizeof ( m_SpriteCharUse ));
                int count = ( newReg & VDP_SPRITE_SIZE ) ? 4 : 1;
                for ( int s = 0; s < 32; s++ ) {
                    sSpriteAttributeEntry *sprite = &m_SpriteAttrTable->data [s];
                    for ( int i = 0; i < count; i++ ) {
                        m_SpriteCharUse [( i + sprite->patternIndex ) % 256 ]++;
                    }
                }
                m_SpritesChanged = true;
            }
            // Fall through

        case 2 :				// Image Table
            {
                m_ChangesMade = true;
                memset ( m_CharUse, 0, sizeof ( m_CharUse ));
                UCHAR *chr = ( UCHAR * ) m_ImageTable;
                for ( int i = 0; i < m_ImageTableSize; i++ ) {
                    int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
                    m_CharUse [ index + *chr++ ]++;
                }
                memset ( m_ScreenChanged, true, sizeof ( m_ScreenChanged ));
            }
            break;

        case 7 :				// Foreground / Background colors
            m_ColorsChanged = true;
            m_SDLColorTable [ 0] = m_SDLColorTable [ newReg & 0x0F ];
            m_SDLColorTable [16] = m_SDLColorTable [ newReg >> 4 ];
            if ( m_BytesPerPixel == 1 ) {
                SDL_SetColors ( m_Screen->GetSurface (), m_SDLColorTable, 0, SIZE ( m_SDLColorTable ));
                SDL_SetColors ( m_BitmapScreen->GetSurface (), m_SDLColorTable, 0, SIZE ( m_SDLColorTable ));
                SDL_SetColors ( m_BitmapSpriteScreen->GetSurface (), m_SDLColorTable, 0, SIZE ( m_SDLColorTable ));
            }
            // Fall through

        case 3 :				// Color Table
        case 4 :				// Pattern Table
            patternChanged = true;
            break;
    }

    if ( patternChanged ) {
        m_ChangesMade = true;
        memset ( m_PatternChanged, true, sizeof ( m_PatternChanged ));
    }
}

void cSdlTMS9918A::WriteData ( UCHAR data )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::WriteData", false );

    UCHAR *MemPtr = &m_Memory [ m_Address & 0x3FFF ];

    if ( data != *MemPtr ) {

        int type = m_MemoryType [ m_Address & 0x3FFF ];

        if ( type ) {

            if ( type & MEM_IMAGE_TABLE ) {
                unsigned offset = MemPtr - ( UCHAR * ) m_ImageTable;
                ASSERT ( offset < SIZE ( m_ScreenChanged ));
                unsigned index  = ( m_Mode & VDP_M3 ) ? offset & 0xFF00 : 0;
                ASSERT ( index <= 256 * 2 );

                m_ChangesMade              = true;
                m_ScreenChanged [ offset ] = true;
                m_CharUse [ index + *MemPtr ]--;
                m_CharUse [ index + data ]++;
            }

            if ( type & MEM_PATTERN_TABLE ) {
                unsigned offset = ( MemPtr - ( UCHAR * ) m_PatternTable ) / 8;
                ASSERT ( offset < SIZE ( m_PatternChanged ));

                m_ChangesMade               = true;
                m_PatternChanged [ offset ] = true;
            }

            if ( ! m_TextMode ) {

                if ( type & MEM_COLOR_TABLE ) {
                    unsigned offset = MemPtr - ( UCHAR * ) m_ColorTable;
                    m_ChangesMade = true;
                    if ( m_Mode & VDP_M3 ) {
                        ASSERT ( offset / 8 < SIZE ( m_PatternChanged ));
                        m_PatternChanged [offset/8] = true;
                    } else {
                        ASSERT (( offset + 1 ) * 8 <= SIZE ( m_PatternChanged ));
                        memset ( &m_PatternChanged [offset*8], true, 8 );
                    }
                }

                if ( type & MEM_SPRITE_ATTR_TABLE ) {
                    unsigned index = ( MemPtr - ( UCHAR * ) m_SpriteAttrTable ) / sizeof ( sSpriteAttributeEntry );
                    ASSERT ( index < SIZE ( m_SpriteAttrTable->data ));

                    sSpriteAttributeEntry *sprite = &m_SpriteAttrTable->data [ index ];
                    if ( MemPtr == &sprite->patternIndex ) {
                        int count = ( m_Register [1] & VDP_SPRITE_SIZE ) ? 4 : 1;
                        for ( int i = 0; i < count; i++ ) {
                            m_SpriteCharUse [( i + *MemPtr ) % 256 ]--;
                            m_SpriteCharUse [( i + data ) % 256 ]++;
                        }
                    }
                    m_SpritesChanged = true;
                }

                if ( type & MEM_SPRITE_DESC_TABLE ) {
                    unsigned index = ( MemPtr - ( UCHAR * ) m_SpriteDescTable ) / 8;
                    ASSERT ( index < SIZE ( m_SpriteCharUse ));

                    if ( m_SpriteCharUse [ index ] > 0 ) {
                        m_SpritesChanged = true;
                    }
                }
            }
        }
    }

    cTMS9918A::WriteData ( data );
}

bool cSdlTMS9918A::SetMode ( int mode )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::SetMode", true );

    if ( cTMS9918A::SetMode ( mode ) == false ) return false;

    m_TextMode = ( m_Mode & VDP_M1 ) ? true : false;

    // We changed video modes, force a refresh of the screen
    m_ChangesMade = true;

    memset ( m_PatternChanged, true, sizeof ( m_PatternChanged ));
    memset ( m_CharUse, 0, sizeof ( m_CharUse ));

    UCHAR *chr = ( UCHAR * ) m_ImageTable;

    for ( int i = 0; i < m_ImageTableSize; i++ ) {
        int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
        m_CharUse [ index + *chr++ ]++;
    }

    return true;
}

void cSdlTMS9918A::LoadImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cSdlTMS9918A::LoadImage", true );

    m_ChangesMade    = true;
    m_SpritesChanged = true;

    memset ( m_ScreenChanged, true, sizeof ( m_ScreenChanged ));
    memset ( m_PatternChanged, true, sizeof ( m_PatternChanged ));

    cTMS9918A::LoadImage ( file );
}
