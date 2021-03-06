//----------------------------------------------------------------------------
//
// File:        cartridge.cpp
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

#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "cartridge.hpp"
#include "support.hpp"

#if defined ( __GNUC__ )
#include <unistd.h>
#endif

DBG_REGISTER ( __FILE__ );

enum eMemoryRegion {
    ROM_0,		// 0x0000 - 0x0FFF
    ROM_1,		// 0x1000 - 0x1FFF
    ROM_2,		// 0x2000 - 0x2FFF
    ROM_3,		// 0x3000 - 0x3FFF
    ROM_4,		// 0x4000 - 0x4FFF
    ROM_5,		// 0x5000 - 0x5FFF
    ROM_6,		// 0x6000 - 0x6FFF
    ROM_7,		// 0x7000 - 0x7FFF
    ROM_8,		// 0x8000 - 0x8FFF
    ROM_9,		// 0x9000 - 0x9FFF
    ROM_A,		// 0xA000 - 0xAFFF
    ROM_B,		// 0xB000 - 0xBFFF
    ROM_C,		// 0xC000 - 0xCFFF
    ROM_D,		// 0xD000 - 0xDFFF
    ROM_E,		// 0xE000 - 0xEFFF
    ROM_F,		// 0xF000 - 0xFFFF
    GROM_0,		// 0x0000 - 0x1FFF
    GROM_1,		// 0x2000 - 0x3FFF
    GROM_2,		// 0x4000 - 0x5FFF
    GROM_3,		// 0x6000 - 0x7FFF
    GROM_4,		// 0x8000 - 0x9FFF
    GROM_5,		// 0xA000 - 0xBFFF
    GROM_6,		// 0xC000 - 0xDFFF
    GROM_7 		// 0xE000 - 0xFFFF
};

const int FILE_VERSION = 0x10;

const char *cCartridge::sm_Banner = "TI-99/4A Module - ";

cCartridge::cCartridge ( const char *filename ) :
    m_FileName ( NULL ),
    m_RamFileName ( NULL ),
    m_Title ( NULL ),
    m_BaseCRU ( 0 )
{
    FUNCTION_ENTRY ( this, "cCartridge ctor", true );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        CpuMemory[i].NumBanks = 0;
        CpuMemory[i].CurBank = NULL;
        for ( unsigned j = 0; j < 4; j++ ) {
            CpuMemory[i].Bank[j].Type    = MEMORY_ROM;
            CpuMemory[i].Bank[j].Data    = NULL;
        }
    }
    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        GromMemory[i].NumBanks = 0;
        GromMemory[i].CurBank = NULL;
        for ( unsigned j = 0; j < 4; j++ ) {
            GromMemory[i].Bank[j].Type    = MEMORY_ROM;
            GromMemory[i].Bank[j].Data    = NULL;
        }
    }

    LoadImage ( filename );
}

cCartridge::~cCartridge ()
{
    FUNCTION_ENTRY ( this, "cCartridge dtor", true );

    SaveRAM ();

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory[i].NumBanks; j++ ) {
            delete [] CpuMemory[i].Bank[j].Data;
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory[i].NumBanks; j++ ) {
            delete [] GromMemory[i].Bank[j].Data;
        }
    }

    memset ( CpuMemory, 0, sizeof ( CpuMemory ));
    memset ( GromMemory, 0, sizeof ( GromMemory ));

    delete [] m_Title;
    delete [] m_RamFileName;
    delete [] m_FileName;
}

void cCartridge::SetFileName ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::SetFileName", true );

    delete [] m_FileName;
    delete [] m_RamFileName;

    if ( filename == NULL ) {
        m_FileName    = NULL;
        m_RamFileName = NULL;
        return;
    }

    int length = strlen ( filename );
    m_FileName = new char [ length + 1 ];
    strcpy ( m_FileName, filename );

    const char *ptr = m_FileName + length - 1;
    while ( ptr > m_FileName ) {
        if ( *ptr == FILE_SEPERATOR ) {
            ptr++;
            break;
        }
        ptr--;
    }
    length = strlen ( ptr );

    m_RamFileName = new char [ strlen ( HOME_PATH ) + 1 + length + 1 ];
    sprintf ( m_RamFileName, "%s%c%*.*s.ram", HOME_PATH, FILE_SEPERATOR, length - 4, length - 4, ptr );
}

void cCartridge::SetTitle ( const char *title )
{
    FUNCTION_ENTRY ( this, "cCartridge::SetTitle", true );

    delete [] m_Title;

    if ( title == NULL ) title = "<Unknown>";
    m_Title = new char [ strlen ( title ) + 1 ];
    strcpy ( m_Title, title );
}

bool cCartridge::IsValid () const
{
    FUNCTION_ENTRY ( this, "cCartridge::IsValid", true );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        if ( CpuMemory[i].NumBanks != 0 ) return true;
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        if ( GromMemory[i].NumBanks != 0 ) return true;
    }

    return false;
}

void cCartridge::LoadRAM ()
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadRAM", true );

    FILE *file = m_RamFileName ? fopen ( m_RamFileName, "rb" ) : NULL;
    if ( file == NULL ) return;

    EVENT ( "Loading module RAM: " << Title ());

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            // If this bank is RAM & Battery backed - update the cartridge
            if ( CpuMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                LoadBuffer ( ROM_BANK_SIZE, CpuMemory [i].Bank [j].Data, file );
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            // If this bank is RAM & Battery backed - update the cartridge
            if ( CpuMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                LoadBuffer ( GROM_BANK_SIZE, GromMemory [i].Bank [j].Data, file );
            }
        }
    }

    fclose ( file );
}

void cCartridge::SaveRAM ()
{
    FUNCTION_ENTRY ( this, "cCartridge::SaveRAM", true );

    // Don't bother creating a .ram file if there is nothing stored in the RAM

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            if ( CpuMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                for ( unsigned k = 0; k < ROM_BANK_SIZE; k++ ) {
                    if ( CpuMemory[i].Bank[j].Data[k] != 0 ) goto save;
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            if ( GromMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                for ( unsigned k = 0; k < GROM_BANK_SIZE; k++ ) {
                    if ( GromMemory[i].Bank[j].Data[k] != 0 ) goto save;
                }
            }
        }
    }

    if ( m_RamFileName != NULL ) {
        unlink ( m_RamFileName );
    }

    return;

save:

    FILE *file = m_RamFileName ? fopen ( m_RamFileName, "wb" ) : NULL;
    if ( file == NULL ) return;

    EVENT ( "Saving module RAM: " << Title ());

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            // If this bank is battery-backed RAM then update the .ram file
            if ( CpuMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                SaveBuffer ( ROM_BANK_SIZE, CpuMemory[i].Bank[j].Data, file );
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            // If this bank is battery-backed RAM then update the .ram file
            if ( GromMemory[i].Bank[j].Type == MEMORY_BATTERY_BACKED ) {
                SaveBuffer ( GROM_BANK_SIZE, GromMemory[i].Bank[j].Data, file );
            }
        }
    }

    fclose ( file );
}

bool cCartridge::LoadOldImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadOldImage", true );

    UCHAR tag = ( UCHAR ) fgetc ( file );

    while ( ! feof ( file )) {

        bool dsr = ( tag & 0x40 ) ? true : false;
        UCHAR index = ( UCHAR ) ( tag & 0x3F );

        sMemoryRegion *memory = NULL;
        USHORT size = 0;

        if ( index < GROM_0 ) {
            memory = &CpuMemory [ index ];
            size   = ROM_BANK_SIZE;
        } else {
            index -= GROM_0;
            memory = &GromMemory [ index ];
            size   = GROM_BANK_SIZE;
        }

        if ( dsr ) {
            m_BaseCRU = ( USHORT ) fgetc ( file );
            m_BaseCRU = ( USHORT ) ( m_BaseCRU | ( fgetc ( file ) << 8 ));
            EVENT ( "  CRU Base: " << hex << m_BaseCRU );
        }

        MEMORY_TYPE_E type = ( MEMORY_TYPE_E ) ( fgetc ( file ) + 1 );

        memory->NumBanks = ( USHORT ) fgetc ( file );
        USHORT NumBytes [4];
        fread ( NumBytes, 1, sizeof ( NumBytes ), file );

        EVENT ( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( USHORT ) ( index * size ));

        for ( int i = 0; i < memory->NumBanks; i++ ) {
            memory->Bank[i].Type = type;
            memory->Bank[i].Data = new UCHAR [ size ];
            memset ( memory->Bank[i].Data, 0, size );
            if ( type == MEMORY_ROM ) {
                LoadBuffer ( NumBytes [i], memory->Bank[i].Data, file );
            }
        }
        memory->CurBank = &memory->Bank[0];

        tag = ( UCHAR ) fgetc ( file );
    }

    fclose ( file );

    LoadRAM ();

    return true;
}

bool cCartridge::LoadImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadImage", true );

    TRACE ( "Opening file " << filename );

    FILE *file = filename ? fopen ( filename, "rb" ) : NULL;
    if ( file == NULL ) {
        WARNING ( "Unable to locate file " << filename );
        return false;
    }

    SetFileName ( filename );

    // Make sure this is really a TI-99/4A cartridge file
    char buffer [ 80 ];
    fread ( buffer, sizeof ( buffer ), 1, file );
    if ( strncmp ( buffer, sm_Banner, strlen ( sm_Banner ))) return false;
    char *ptr = &buffer [ strlen ( sm_Banner )];
    ptr [ strlen ( ptr ) - 2 ] = '\0';
    SetTitle ( ptr );

    EVENT ( "Loading module: " << Title ());

    int version = ( UCHAR ) fgetc ( file );
    if (( version & 0x80 ) != 0 ) {
        ungetc ( version, file );
        return LoadOldImage ( file );
    }

    if ( version > FILE_VERSION ) {
        ERROR ( "Unrecognized file version" );
        return false;
    }

    m_BaseCRU = ( USHORT ) ( fgetc ( file ) << 8 );
    m_BaseCRU = ( USHORT ) ( m_BaseCRU | ( UCHAR ) fgetc ( file ));

    UCHAR index = ( UCHAR ) fgetc ( file );

    while ( ! feof ( file )) {

        sMemoryRegion *memory = NULL;
        USHORT size = 0;

        if ( index < GROM_0 ) {
            memory = &CpuMemory [ index ];
            size   = ROM_BANK_SIZE;
        } else {
            index -= GROM_0;
            memory = &GromMemory [ index ];
            size   = GROM_BANK_SIZE;
        }

        memory->NumBanks = ( int ) fgetc ( file );

        EVENT ( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( USHORT ) ( index * size ));

        for ( int i = 0; i < memory->NumBanks; i++ ) {
            memory->Bank[i].Type = ( MEMORY_TYPE_E ) fgetc ( file );
            memory->Bank[i].Data = new UCHAR [ size ];
            memset ( memory->Bank[i].Data, 0, size );
            if ( memory->Bank[i].Type == MEMORY_ROM ) {
                LoadBuffer ( size, memory->Bank[i].Data, file );
            }
        }
        memory->CurBank = &memory->Bank[0];

        index = ( UCHAR ) fgetc ( file );
    }

    fclose ( file );

    LoadRAM ();

    return true;
}

bool cCartridge::SaveImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::SaveImage", true );

    FILE *file = filename ? fopen ( filename, "wb" ) : NULL;
    if ( file == NULL ) return false;

    SetFileName ( filename );

    char buffer [ 80 ];
    memset ( buffer, 0, sizeof ( buffer ));
    sprintf ( buffer, "%s%s\n%c", sm_Banner, Title (), 0x1A );
    fwrite ( buffer, sizeof ( buffer ), 1, file );

    EVENT ( "Saving module: " << Title ());

    fputc ( FILE_VERSION, file );

    fputc ( m_BaseCRU >> 8, file );
    fputc ( m_BaseCRU & 0xFF, file );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        if ( CpuMemory[i].NumBanks != 0 ) {
            sMemoryRegion *memory = &CpuMemory[i];
            fputc ( ROM_0 + i, file );
            fputc ( memory->NumBanks, file );
            for ( int j = 0; j < memory->NumBanks; j++ ) {
                fputc ( memory->Bank[j].Type, file );
                if ( memory->Bank[j].Type == MEMORY_ROM ) {
                    SaveBuffer ( ROM_BANK_SIZE, memory->Bank[j].Data, file );
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        if ( GromMemory[i].NumBanks != 0 ) {
            sMemoryRegion *memory = &GromMemory[i];
            fputc ( GROM_0 + i, file );
            fputc ( memory->NumBanks, file );
            for ( int j = 0; j < memory->NumBanks; j++ ) {
                fputc ( memory->Bank[j].Type, file );
                if ( memory->Bank[j].Type == MEMORY_ROM ) {
                    SaveBuffer ( GROM_BANK_SIZE, memory->Bank[j].Data, file );
                }
            }
        }
    }

    fclose ( file );

    return true;
}
