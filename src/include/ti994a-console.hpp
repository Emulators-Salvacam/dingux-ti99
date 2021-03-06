//----------------------------------------------------------------------------
//
// File:        ti994a-console.hpp
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

#ifndef TI994A_CONSOLE_HPP_
#define TI994A_CONSOLE_HPP_

#if ! defined ( TI994A_HPP_ )
    #error You must include TI994A.hpp before TI994A-console.hpp
#endif

#define FCTN_KEY	0x0100
#define SHIFT_KEY	0x0200
#define CTRL_KEY	0x0400
#define CAPS_LOCK_KEY	0x0800

class cConsoleTI994A : public cTI994A {

    bool                m_CapsLock;
    int                 m_ColumnSelect;
    int                 m_KeyHead;
    int                 m_KeyTail;
    int                 m_KeyBuffer [ 50 ];

    void KeyPressed ( int ch );
    void EditRegisters ();

    void  SaveImage ( const char * );
    bool  LoadImage ( const char * );

    USHORT VideoReadBreakPoint ( const ADDRESS address, USHORT data );
    USHORT VideoWriteBreakPoint ( const ADDRESS address, USHORT data );
    USHORT GromReadBreakPoint ( const ADDRESS address, USHORT data );
    USHORT GromWriteBreakPoint ( const ADDRESS address, USHORT data );

public:

    cConsoleTI994A ( cCartridge *ctg, cTMS9918A * = NULL );
    ~cConsoleTI994A ();

    virtual int   ReadCRU ( ADDRESS );
    virtual void  WriteCRU ( ADDRESS, USHORT );

    virtual void  Run ();
    virtual bool  Step ();
    virtual void  Refresh ( bool );

};

#endif
