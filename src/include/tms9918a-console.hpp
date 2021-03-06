//----------------------------------------------------------------------------
//
// File:        tms9918a-console.hpp
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

#ifndef TMS9918A_CONSOLE_HPP_
#define TMS9918A_CONSOLE_HPP_

#if ! defined ( TMS9918A_HPP_ )
    #error You must include TMS9918A.hpp before TMS9918A-console.hpp
#endif

class cConsoleTMS9918A : public cTMS9918A {

protected:

    UCHAR               m_Bias;
    int                 m_Width;

    // cTMS9918A protected methods
    virtual bool SetMode ( int );
    virtual void Refresh ( bool );

public:

    cConsoleTMS9918A ( int );
    ~cConsoleTMS9918A ();

    void SetBias ( UCHAR );

    // cTMS9918A public methods
    virtual void Reset ();
    virtual void WriteData ( UCHAR );
    virtual void WriteRegister ( int, UCHAR );

};

#endif
