//----------------------------------------------------------------------------
//
// File:        decodelzw.cpp
// Date:        15-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to decode an LZW compressed ARK file (Barry Boone's Archive format)
//
// Copyright (c) 2003 Marc Rousseau, All Rights Reserved.
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
#include "common.hpp"
#include "logger.hpp"
#include "decodelzw.hpp"

DBG_REGISTER ( __FILE__ );

static inline USHORT GetUSHORT ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUSHORT", true );

    const UCHAR *ptr = ( const UCHAR * ) _ptr;
    return ( USHORT ) (( ptr [0] << 8 ) | ptr [1] );
}

cDecodeLZW::cDecodeLZW ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW ctor", true );
    
}

cDecodeLZW::~cDecodeLZW ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW dtor", true );
}

void cDecodeLZW::Reset ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Reset", true );
    
    m_nBits    = 9;
    m_nBitsB   = 7;
    m_MaxCode  = 512;
    m_FreeCode = CODE_FIRST_FREE;
}

void cDecodeLZW::Init ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Init", true );
    
    Reset ();
    m_BitOff = 0;
}

bool cDecodeLZW::WriteChar ( UCHAR ch )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::WriteChar", false );
    
    *m_OutPtr++ = ch;

    bool retVal = true;
    
    if ( m_OutPtr == m_WriteBuffer + m_MaxWriteSize ) {
        retVal = m_WriteCallback ( m_WriteBuffer, m_MaxWriteSize, m_CallbackToken );
        m_OutPtr = m_WriteBuffer;
    }

    return retVal;
}

void cDecodeLZW::Done ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Done", true );

    if ( m_OutPtr != m_WriteBuffer ) {
        m_WriteCallback ( m_WriteBuffer, m_OutPtr - m_WriteBuffer, m_CallbackToken );
        m_OutPtr = m_WriteBuffer;
    }
}

USHORT cDecodeLZW::ReadCode ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::ReadCode", false );
    
    USHORT code = 0;

    if ( m_BitOff != 0 ) {
        if ( m_BitOff <= m_nBitsB ) {
            code      = (( USHORT ) ( m_CurWord << m_BitOff )) >> m_nBitsB;
            m_BitOff += m_nBits;
        } else {
            code      = (( USHORT ) ( m_CurWord << m_BitOff )) >> m_nBitsB;
            m_CurWord = GetUSHORT ( m_InPtr++ );
            m_BitOff  = ( m_BitOff + m_nBits ) - 16;
            code     |= m_CurWord >> ( 16 - m_BitOff );
        }
    } else {
        m_CurWord = GetUSHORT ( m_InPtr++ );
        m_BitOff += m_nBits;
        code      = m_CurWord >> m_nBitsB;
    }

    ASSERT ( code <= m_FreeCode );

    return code;
}

void cDecodeLZW::AddCode ( UCHAR ch, USHORT code )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::AddCode", false );
    
    ASSERT ( m_FreeCode < 4096 );

    USHORT index       = m_FreeCode++;
    m_CHAR [index]     = ch;
    m_NextCode [index] = code;
}

void cDecodeLZW::SetWriteCallback ( bool (*callback) ( void *, size_t, void * ), void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::SetWriteCallback", true );
    
    m_WriteCallback = callback;
    m_CallbackToken = token;
    m_MaxWriteSize  = size;
    
    m_WriteBuffer = ( UCHAR * ) buffer;
    m_OutPtr      = m_WriteBuffer;
}

int cDecodeLZW::ParseBuffer ( void *buffer, size_t size )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::ParseBuffer", true );
    
    UCHAR   stackBuffer [CODE_MAX];
    UCHAR  *stack = stackBuffer + CODE_MAX;

    UCHAR   K = 0;
    USHORT  oldCode = 0;
    USHORT  curCode = 0;

    m_InPtr = ( USHORT * ) buffer;

    Init ();

    while (( char * ) m_InPtr < ( char * ) buffer + size ) {

        USHORT code = ReadCode ();

        switch ( code ) {

            case CODE_EOF :

                Done ();
                return 1;

            case CODE_CLEAR :

                Reset ();
                stack   = stackBuffer + CODE_MAX;
                if ( WriteChar ( K = oldCode = ReadCode ()) == false ) return 0;
                break;

            default:

                curCode = code;

                if ( code >= m_FreeCode ) {
                    curCode  = oldCode;
                    *--stack = K;
                }

                while ( curCode > 0xFF ) {
                    *--stack = m_CHAR [curCode];
                    curCode  = m_NextCode [curCode];
                }

                K = curCode;

                if ( WriteChar ( curCode ) == false ) return 0;
                while ( stack != stackBuffer + CODE_MAX ) {
                    if ( WriteChar ( *stack++ ) == false ) return 0;
                }

                AddCode ( K, oldCode );
                oldCode = code;

                if (( m_FreeCode >= m_MaxCode ) && ( m_nBits != 12 )) {
                    m_nBits++;
                    m_nBitsB--;
                    m_MaxCode *= 2;
                }
                break;
        }
    }

    return -1;
}
