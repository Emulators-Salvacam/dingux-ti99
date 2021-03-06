//----------------------------------------------------------------------------
//
// File:        tms9900.hpp
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

#ifndef TMS9900_HPP_
#define TMS9900_HPP_

#if defined ( ADDRESS )
    #undef ADDRESS
#endif

typedef unsigned short ADDRESS;

typedef USHORT (*TRAP_FUNCTION) ( void *, int, bool, const ADDRESS, USHORT );

#define MEMFLG_ROM		0x08
#define MEMFLG_8BIT		0x04
#define MEMFLG_READ		0x02
#define MEMFLG_WRITE		0x01
#define MEMFLG_ACCESS		0x03
#define MEMFLG_INDEX_MASK	0xF0
#define MEMFLG_INDEX_SHIFT	4

enum MEMORY_ACCESS_E { MEM_ROM, MEM_RAM };

struct sOpCode {
    char        mnemonic [8];
    USHORT      opCode;
    USHORT      mask;
    USHORT      format;
    USHORT      unused;
    void        (*function) ();
    ULONG       clocks;
    ULONG       count;
};

struct sLookUp {
    sOpCode    *opCode;
    int         size;
};

struct sTrapInfo {
    void          *ptr;
    int            data;
    TRAP_FUNCTION  function;
};

#define TMS_LOGICAL	0x8000
#define TMS_ARITHMETIC	0x4000
#define TMS_EQUAL	0x2000
#define TMS_CARRY	0x1000
#define TMS_OVERFLOW	0x0800
#define TMS_PARITY	0x0400
#define TMS_XOP		0x0200

class cTMS9900 {

    static int sortFunction ( const sOpCode *p1, const sOpCode *p2 );

public:

    cTMS9900 ();
    ~cTMS9900 ();

    void SetPC ( ADDRESS address );
    void SetWP ( ADDRESS address );
    void SetST ( USHORT  address );

    ADDRESS GetPC ();
    ADDRESS GetWP ();
    USHORT  GetST ();

    void Run ();
    void Stop ();
    bool Step ();

    bool IsRunning ();

    void Reset ();
    void SignalInterrupt ( UCHAR );
    void ClearInterrupt ( UCHAR );

    ULONG GetClocks ();
    void  AddClocks ( int );
    void  ResetClocks ();

    ULONG GetCounter ();
    void  ResetCounter ();

    void SaveImage ( FILE * );
    void LoadImage ( FILE * );

    UCHAR RegisterBreakpoint ( TRAP_FUNCTION, void *, int );
    void  DeRegisterBreakpoint ( UCHAR );

    UCHAR GetBreakpoint ( TRAP_FUNCTION, int );
    int   SetBreakpoint ( ADDRESS, UCHAR, bool, UCHAR );
    void  SetMemory ( MEMORY_ACCESS_E, ADDRESS, long );

    void  ClearBreakpoint ( UCHAR );

};

#endif
