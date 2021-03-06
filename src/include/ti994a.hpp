//----------------------------------------------------------------------------
//
// File:        ti994a.hpp
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

#ifndef TI994A_HPP_
#define TI994A_HPP_

#if ! defined ( TMS9900_HPP_ )
    #error You must include TMS9900.hpp before TI994A.hpp	// TRAP_FUNCTION
#endif

class  cTMS5220;
class  cTMS9900;
class  cTMS9901;
class  cTMS9918A;
class  cTMS9919;
class  cCartridge;
class  cDevice;

struct sMemoryRegion;

enum HEADER_SECTION_E {
    SECTION_BASE,
    SECTION_CPU,
    SECTION_VDP,
    SECTION_GROM,
    SECTION_ROM,
    SECTION_SOUND,
    SECTION_SPEECH,
    SECTION_CRU,
    SECTION_DSR
};

struct sStateHeader {
    USHORT          id;
    USHORT          length;
};

struct sStateHeaderInfo {
    sStateHeader    header;
    ULONG           offset;
};

struct sImageFileState {
    FILE           *file;
    ULONG           start;
    ULONG           next;
};

class cTI994A {

protected:

    enum TRAP_TYPE_E {
        TRAP_BANK_SWITCH,
        TRAP_SCRATCH_PAD,
        TRAP_SOUND,
        TRAP_SPEECH,
        TRAP_VIDEO,
        TRAP_GROM
    };

    cTMS9900           *m_CPU;
    cTMS9901           *m_PIC;
    cTMS9918A          *m_VDP;
    cTMS9919           *m_SoundGenerator;
    cTMS5220           *m_SpeechSynthesizer;

    ULONG               m_RefreshInterval;

    cCartridge         *m_Console;
    cCartridge         *m_Cartridge;

    USHORT              m_ActiveCRU;
    cDevice            *m_Device [32];

    UCHAR              *m_GromPtr;
    ADDRESS             m_GromAddress;
    ADDRESS             m_GromLastInstruction;
    int                 m_GromReadShift;
    int                 m_GromWriteShift;
    int                 m_GromCounter;

    sMemoryRegion      *m_CpuMemoryInfo [16];	// Pointers 4K banks of CPU RAM
    sMemoryRegion      *m_GromMemoryInfo [8];	// Pointers 8K banks of Graphics RAM

    UCHAR              *m_CpuMemory;		// Pointer to 64K of System ROM/RAM
    UCHAR              *m_GromMemory;		// Pointer to 64K of Graphics ROM/RAM
    UCHAR              *m_VideoMemory;		// Pointer to 16K of Video RAM

    cDevice *GetDevice ( ADDRESS );

    static USHORT TrapFunction ( void *, int, bool, const ADDRESS, USHORT );

    virtual USHORT BankSwitch            ( const ADDRESS, USHORT );
    virtual USHORT ScratchPadRead        ( const ADDRESS, USHORT );
    virtual USHORT ScratchPadWrite       ( const ADDRESS, USHORT );
    virtual USHORT SoundBreakPoint       ( const ADDRESS, USHORT );
    virtual USHORT SpeechWriteBreakPoint ( const ADDRESS, USHORT );
    virtual USHORT SpeechReadBreakPoint  ( const ADDRESS, USHORT );
    virtual USHORT VideoWriteBreakPoint  ( const ADDRESS, USHORT );
    virtual USHORT VideoReadBreakPoint   ( const ADDRESS, USHORT );
    virtual USHORT GromWriteBreakPoint   ( const ADDRESS, USHORT );
    virtual USHORT GromReadBreakPoint    ( const ADDRESS, USHORT );

public:

    cTI994A ( cCartridge *, cTMS9918A * = NULL, cTMS9919 * = NULL, cTMS5220 * = NULL );
    virtual ~cTI994A ();

    cTMS9900  *GetCPU ()			{ return m_CPU; }
    cTMS9918A *GetVDP ()			{ return m_VDP; }
    cTMS9919  *GetSoundGenerator ()		{ return m_SoundGenerator; }

    UCHAR   *GetCpuMemory () const		{ return m_CpuMemory; }
    UCHAR   *GetGromMemory () const		{ return m_GromMemory; }
    UCHAR   *GetVideoMemory () const		{ return m_VideoMemory; }

    ADDRESS  GetGromAddress () const		{ return m_GromAddress; }
    void     SetGromAddress ( ADDRESS addr )	{ m_GromAddress = addr; m_GromPtr = m_GromMemory + addr; }

    virtual void Sleep ( int, ULONG )		{}
    virtual void WakeCPU ( ULONG )	        {}

    virtual int  ReadCRU ( ADDRESS );
    virtual void WriteCRU ( ADDRESS, USHORT );

    virtual void AddDevice ( cDevice * );

    virtual void InsertCartridge ( cCartridge *, bool = true );
    virtual void RemoveCartridge ( cCartridge *, bool = true );

    virtual void Reset ();

    virtual void Run ();
    virtual void Stop ();
    virtual bool Step ();
    virtual bool IsRunning ();

    virtual void Refresh ( bool )		{}

    static bool OpenImageFile ( const char *, sImageFileState * );
    static bool FindHeader ( sImageFileState *, HEADER_SECTION_E );
    static void MarkHeader ( FILE *, HEADER_SECTION_E, sStateHeaderInfo * );
    static void SaveHeader ( FILE *, sStateHeaderInfo * );

    virtual void SaveImage ( const char * );
    virtual bool LoadImage ( const char * );

};

#endif
