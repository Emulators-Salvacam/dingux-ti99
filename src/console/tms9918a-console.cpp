//----------------------------------------------------------------------------
//
// File:        tms9918a-console.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2003 Marc Rousseau, All Rights Reserved.
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
#include "compress.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "tms9918a-console.hpp"
#include "screenio.hpp"

cConsoleTMS9918A::cConsoleTMS9918A ( int refresh ) :
    cTMS9918A ( refresh ),
    m_Bias ( 0 ),
    m_Width ( 32 )
{
}

cConsoleTMS9918A::~cConsoleTMS9918A ()
{
}

void cConsoleTMS9918A::Reset ()
{
    cTMS9918A::Reset ();

    m_Bias  = 0;
    m_Width = 32;

    char clear [] = "        ";
    for ( int y = 0; y < 24; y++ ) {
        PutXY ( 0, y, ( char * ) m_Memory, m_Width );
        if ( m_Width == 32 ) PutXY ( 32, y, clear, 8 );
    }
}

bool cConsoleTMS9918A::SetMode ( int mode )
{
    if ( cTMS9918A::SetMode ( mode ) == false ) return false;

    m_Width = ( m_Mode & VDP_M1 ) ? 40 : 32;

    if ( m_Width == 32 ) {
        char clear [] = "        ";
        for ( int y = 0; y < 24; y++ ) {
            PutXY ( 32, y, clear, 8 );
        }
    }

    // Now force a screen update
    int bias = m_Bias;

    m_Bias = ( UCHAR ) -1;

    SetBias (( UCHAR ) bias );

    return true;
}

void cConsoleTMS9918A::Refresh ( bool force )
{
    if ( force == false ) return;

    for ( unsigned i = ( UCHAR * ) m_ImageTable - m_Memory; i < sizeof ( sScreenImage ); i++ ) {
        UCHAR temp = ( UCHAR ) ( m_Memory [ i ] - m_Bias );
        if ( i / m_Width < 24 ) {
            PutXY ( i % m_Width, i / m_Width, ( char * ) &temp, 1 );
        }
    }
}

void cConsoleTMS9918A::WriteData ( UCHAR data )
{
    if ((( m_Address & 0x3FFF ) >= ( UCHAR * ) m_ImageTable - m_Memory ) && (( m_Address & 0x3FFF ) < ( UCHAR * ) ( m_ImageTable + 1 ) - m_Memory )) {
        int imageTableOffset = ( UCHAR * ) m_ImageTable - m_Memory;
        int loc = ( m_Address & 0x3FFF ) - imageTableOffset;
        if ( loc / m_Width < 24 ) {
            UCHAR temp = ( UCHAR ) ( data - m_Bias );
            PutXY ( loc % m_Width, loc / m_Width, ( char * ) &temp, 1 );
        }
    }

    cTMS9918A::WriteData ( data );
}

void cConsoleTMS9918A::WriteRegister ( int reg, UCHAR value )
{
    int x = 48 + ( reg % 4 ) * 9;
    int y = ( reg < 4 ) ? 9 : 10;
    GotoXY ( x, y );    outByte ( value );

    cTMS9918A::WriteRegister ( reg, value );
}

void cConsoleTMS9918A::SetBias ( UCHAR bias )
{
    if ( bias != m_Bias ) {
        m_Bias = bias;
        Refresh ( true );
    }
}
