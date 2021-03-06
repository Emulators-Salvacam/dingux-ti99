//----------------------------------------------------------------------------
//
// File:        decodelzw.hpp
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

#ifndef DECODELZW_HPP_
#define DECODELZW_HPP_

const int CODE_CLEAR      = 256;    // clear code
const int CODE_EOF        = 257;    // EOF marker
const int CODE_FIRST_FREE = 258;    // first free code
const int CODE_MAX        = 4096;   // Max codes + 1

class cDecodeLZW {

    // Used for reading the next token
    USHORT  m_nBits;
    USHORT  m_nBitsB;
    USHORT  m_BitOff;
    USHORT  m_CurWord;
    USHORT  m_MaxCode;

    // History tables
    UCHAR   m_CHAR [CODE_MAX];
    USHORT  m_NextCode [CODE_MAX];
    USHORT  m_FreeCode;

    // Input buffer related
    USHORT  *m_InPtr;

    // Output buffer related
    bool   (*m_WriteCallback) ( void *, size_t, void * );
    void    *m_CallbackToken;
    size_t   m_MaxWriteSize;
    UCHAR   *m_WriteBuffer;
    UCHAR   *m_OutPtr;

    void Init ();
    void Reset ();
    void Done ();

    USHORT ReadCode ();

    void AddCode ( UCHAR, USHORT );
    bool WriteChar ( UCHAR ch );

public:

    cDecodeLZW ();
    ~cDecodeLZW ();

    void SetWriteCallback ( bool (*) ( void *, size_t, void * ), void *, size_t, void * );
    int  ParseBuffer ( void *, size_t );

};

#endif
