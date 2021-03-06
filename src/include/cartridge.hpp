//----------------------------------------------------------------------------
//
// File:        cartridge.hpp
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

#ifndef CARTRIDGE_HPP_
#define CARTRIDGE_HPP_

#define ROM_BANK_SIZE	0x1000
#define GROM_BANK_SIZE	0x2000

enum MEMORY_TYPE_E {
    MEMORY_UNKNOWN,
    MEMORY_RAM,
    MEMORY_ROM,
    MEMORY_BATTERY_BACKED,
    MEMORY_MAX
};

struct sMemoryBank {
    MEMORY_TYPE_E  Type;
    UCHAR         *Data;
};

struct sMemoryRegion {
    int            NumBanks;
    sMemoryBank   *CurBank;
    sMemoryBank    Bank [4];
};

class cCartridge {

    static const char  *sm_Banner;

    char          *m_FileName;
    char          *m_RamFileName;
    char          *m_Title;
    USHORT         m_BaseCRU;

// Manufacturer
// Copyright/date
// Catalog Number
// Icon/Image

    void SetFileName ( const char * );

    void LoadRAM ();
    void SaveRAM ();

    bool LoadOldImage ( FILE * );

public:

    sMemoryRegion CpuMemory [16];
    sMemoryRegion GromMemory [8];

    cCartridge ( const char * = NULL );
    ~cCartridge ();

    void SetTitle ( const char * );

    void SetCRU ( USHORT cru )         { m_BaseCRU = cru; }
    USHORT GetCRU () const             { return m_BaseCRU; }

    const char *Title () const         { return m_Title; }
    const char *FileName () const      { return m_FileName; }

    bool IsValid () const;

    bool LoadImage ( const char * );
    bool SaveImage ( const char * );
};

#endif
