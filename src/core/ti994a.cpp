//----------------------------------------------------------------------------
//
// File:        ti994a.cpp
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
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "tms9919.hpp"
#include "tms5220.hpp"
#include "cartridge.hpp"
#include "ti994a.hpp"
#include "device.hpp"
#include "tms9901.hpp"

DBG_REGISTER ( __FILE__ );

extern "C" UCHAR  CpuMemory [ 0x10000 ];

extern "C" {
    void *CRU_Object;
};

cTI994A::cTI994A ( cCartridge *_console, cTMS9918A *_vdp, cTMS9919 *_sound, cTMS5220 *_speech )
{
    FUNCTION_ENTRY ( this, "cTI994A ctor", true );

    // Define CRU_Object for _OpCodes.asm
    CRU_Object = this;

    m_CpuMemory = CpuMemory;

    m_GromMemory  = new UCHAR [ 0x10000 ];

    memset ( CpuMemory, 0, 0x10000 );
    memset ( m_GromMemory, 0, 0x10000 );

    memset ( m_CpuMemoryInfo, 0, sizeof ( m_CpuMemoryInfo ));
    memset ( m_GromMemoryInfo, 0, sizeof ( m_GromMemoryInfo ));

    m_CPU = new cTMS9900 ();
    m_PIC = new cTMS9901 ( m_CPU );

    m_VDP = ( _vdp != NULL ) ? _vdp : new cTMS9918A ();
    m_VDP->SetPIC ( m_PIC, 2 );

    m_VideoMemory = m_VDP->GetMemory ();

    m_SoundGenerator = ( _sound != NULL ) ? _sound : new cTMS9919;

    m_SpeechSynthesizer = _speech;
    if ( m_SpeechSynthesizer != NULL ) {
        m_SpeechSynthesizer->SetComputer ( this );
    }

    // Start off with all memory set as ROM - simulates no memory present
    m_CPU->SetMemory ( MEM_ROM, 0x0000, 0x10000 );

    // No Devices by default - derived classes can add Devices
    m_ActiveCRU = 0;
    memset ( m_Device, 0, sizeof ( m_Device ));

    // Add the TMS9901 programmable timer
    m_Device [0] = m_PIC;

    m_Console   = NULL;
    m_Cartridge = NULL;

    InsertCartridge ( _console );

    m_Cartridge = NULL;
    m_Console   = _console;

    m_RefreshInterval = 3000000 / m_VDP->GetRefreshRate ();

    m_GromPtr             = m_GromMemory;
    m_GromAddress         = 0;
    m_GromLastInstruction = 0;
    m_GromReadShift       = 8;
    m_GromWriteShift      = 8;
    m_GromCounter         = 0;

    UCHAR index;
    // Register the bank swap trap function here - not used until needed
    m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_BANK_SWITCH );

    index = m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_SCRATCH_PAD );
    for ( USHORT address = 0x8000; address < 0x8300; address += ( USHORT ) 1 ) {
        m_CPU->SetBreakpoint ( address, ( UCHAR ) MEMFLG_ACCESS, false, index );
    }

    index = m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_SOUND );
    m_CPU->SetBreakpoint ( 0x8400, MEMFLG_WRITE, true, index );						// Sound chip Port

    index = m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_VIDEO );
    for ( USHORT address = 0x8800; address < 0x8C00; address += ( USHORT ) 4 ) {
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0000 ), MEMFLG_READ, true, index );		// VDP Read Byte Port
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0002 ), MEMFLG_READ, true, index );		// VDP Read Status Port
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0400 ), MEMFLG_WRITE, false, index );		// VDP Write Byte Port
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0402 ), MEMFLG_WRITE, false, index );		// VDP Write (set) Address Port
    }

    // Make this bank look like ROM (writes aren't stored)
    m_CPU->SetMemory ( MEM_ROM, 0x9000, ROM_BANK_SIZE );

    index = m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_SPEECH );
    for ( USHORT address = 0x9000; address < 0x9400; address += ( USHORT ) 2 ) {
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0000 ), ( UCHAR ) MEMFLG_READ, true, index );	// Speech Read Port
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0400 ), ( UCHAR ) MEMFLG_WRITE, true, index );	// Speech Write Port
    }

    index  = m_CPU->RegisterBreakpoint ( TrapFunction, this, TRAP_GROM );
    for ( USHORT address = 0x9800; address < 0x9C00; address += ( USHORT ) 2 ) {
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0000 ), ( UCHAR ) MEMFLG_READ, true, index );	// GROM Read Port
        m_CPU->SetBreakpoint (( USHORT ) ( address | 0x0400 ), ( UCHAR ) MEMFLG_WRITE, true, index );	// GROM Write Port
    }
}

cTI994A::~cTI994A ()
{
    FUNCTION_ENTRY ( this, "cTI994A dtor", true );

    for ( unsigned i = 0; i < SIZE ( m_Device ); i++ ) {
        if ( m_Device [i] != NULL ) {
            delete m_Device [i];
        }
    }

    delete [] m_GromMemory;

    delete m_SpeechSynthesizer;
    delete m_SoundGenerator;
    delete m_CPU;
    delete m_VDP;
    delete m_Console;
}

cDevice *cTI994A::GetDevice ( ADDRESS address )
{
    FUNCTION_ENTRY ( this, "cTI994A::GetDevice", false );

    return m_Device [ ( address >> 8 ) & 0x1F ];
}

USHORT cTI994A::TrapFunction ( void *ptr, int type, bool read, const ADDRESS address, USHORT value )
{
    FUNCTION_ENTRY ( ptr, "cTI994A::TrapFunction", false );

    cTI994A *pThis = ( cTI994A * ) ptr;
    USHORT retVal = value;

    if ( read == true ) {
        switch ( type ) {
            case TRAP_SCRATCH_PAD :
                retVal = pThis->ScratchPadRead (( USHORT ) ( address & 0x00FF ), value );
                break;
            case TRAP_SOUND :
                retVal = pThis->SoundBreakPoint ( address, value );
                break;
            case TRAP_SPEECH :
                retVal = pThis->SpeechReadBreakPoint ( address, value );
                break;
            case TRAP_VIDEO :
                retVal = pThis->VideoReadBreakPoint (( USHORT ) ( address & 0xFF02 ), value );
                break;
            case TRAP_GROM :
                retVal = pThis->GromReadBreakPoint (( USHORT ) ( address & 0xFF02 ), value );
                break;
            default :
                fprintf ( stderr, "Invalid index %d for read access in TrapFunction\n", type );
                break;
        };
    } else {
        switch ( type ) {
            case TRAP_BANK_SWITCH :
                retVal = pThis->BankSwitch ( address, value );
                break;
            case TRAP_SCRATCH_PAD :
                retVal = pThis->ScratchPadWrite (( USHORT ) ( address & 0x00FF ), value );
                break;
            case TRAP_SOUND :
                retVal = pThis->SoundBreakPoint ( address, value );
                break;
            case TRAP_SPEECH :
                retVal = pThis->SpeechWriteBreakPoint ( address, value );
                break;
            case TRAP_VIDEO :
                retVal = pThis->VideoWriteBreakPoint (( USHORT ) ( address & 0xFF02 ), value );
                break;
            case TRAP_GROM :
                retVal = pThis->GromWriteBreakPoint (( USHORT ) ( address & 0xFF02 ), value );
                break;
            default :
                fprintf ( stderr, "Invalid index %d for write access in TrapFunction\n", type );
                break;
        };
    }

    return retVal;
}

USHORT cTI994A::BankSwitch ( const ADDRESS address, USHORT )
{
    FUNCTION_ENTRY ( this, "cTI994A::BankSwitch", false );

    sMemoryRegion *region = m_CpuMemoryInfo [( address >> 12 ) & 0xFE ];
    int newBank = ( address >> 1 ) % region->NumBanks;
    region->CurBank = &region->Bank[newBank];
    ADDRESS baseAddress = ( ADDRESS ) ( address & 0xE000 );
    memcpy ( &CpuMemory [ baseAddress ], region->CurBank->Data, ROM_BANK_SIZE );
    region++;
    region->CurBank = &region->Bank[newBank];
    memcpy ( &CpuMemory [ baseAddress + ROM_BANK_SIZE ], region->CurBank->Data, ROM_BANK_SIZE );
    return CpuMemory [ address ];
}

USHORT cTI994A::ScratchPadRead ( const ADDRESS address, USHORT )
{
    FUNCTION_ENTRY ( this, "cTI994A::ScratchPadRead", false );

    UCHAR *ptr = &CpuMemory [ 0x8300 + address ];

    return ( USHORT ) (( ptr [1] << 8 ) | ptr [0] );
}

USHORT cTI994A::ScratchPadWrite ( const ADDRESS address, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::ScratchPadWrite", false );

    CpuMemory [ 0x8300 + address ] = ( UCHAR ) data;
    CpuMemory [ 0x8300 + address + 1 ] = ( UCHAR ) ( data >> 8 );

    return 0;
}

USHORT cTI994A::SoundBreakPoint ( const ADDRESS, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SoundBreakPoint", false );

    m_SoundGenerator->WriteData (( UCHAR ) data );

    return data;
}

USHORT cTI994A::SpeechWriteBreakPoint ( const ADDRESS, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SpeechWriteBreakPoint", false );

    if ( m_SpeechSynthesizer != NULL ) {
        m_SpeechSynthesizer->WriteData (( UCHAR ) data );
    }

    return data;
}

USHORT cTI994A::SpeechReadBreakPoint ( const ADDRESS, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SpeechReadBreakPoint", false );

    if ( m_SpeechSynthesizer != NULL ) {
        data = m_SpeechSynthesizer->ReadData (( UCHAR ) data );
    }

    return data;
}

USHORT cTI994A::VideoReadBreakPoint ( const ADDRESS address, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::VideoReadBreakPoint", false );

    switch ( address ) {
        case 0x8800 :
            data = m_VDP->ReadData ();
            break;
        case 0x8802 :
            data = m_VDP->ReadStatus ();
            break;
        default :
            FATAL ( "Unexpected address " << hex << address );
            break;
    }
    return data;
}

USHORT cTI994A::VideoWriteBreakPoint ( const ADDRESS address, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::VideoWriteBreakPoint", false );

    switch ( address ) {
        case 0x8C00 :
            m_VDP->WriteData (( UCHAR ) data );
            break;
        case 0x8C02 :
            m_VDP->WriteAddress (( UCHAR ) data );
            break;
        default :
            FATAL ( "Unexpected address " << hex << address );
            break;
    }
    return data;
 }

USHORT cTI994A::GromReadBreakPoint ( const ADDRESS address, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::GromReadBreakPoint", false );

    m_GromWriteShift = 8;

    switch ( address ) {
        case 0x9800 :			// GROM/GRAM Read Byte Port
            data = *m_GromPtr;
            m_GromAddress = ( USHORT ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
            break;
        case 0x9802 :			// GROM/GRAM Read Address Port
            data = ( USHORT ) ((( m_GromAddress + 1 ) >> m_GromReadShift ) & 0x00FF );
            m_GromReadShift  = 8 - m_GromReadShift;
            break;
        default :
            FATAL ( "Unexpected address " << hex << address );
            break;
    }
    m_GromPtr = &m_GromMemory [ m_GromAddress ];
    return data;
}

USHORT cTI994A::GromWriteBreakPoint ( const ADDRESS address, USHORT data )
{
    FUNCTION_ENTRY ( this, "cTI994A::GromWriteBreakPoint", false );

    sMemoryRegion *memory;
    switch ( address ) {
        case 0x9C00 :			// GROM/GRAM Write Byte Port
            memory = m_GromMemoryInfo [ m_GromAddress >> 13 ];
            if ( memory && ( memory->CurBank->Type != MEMORY_ROM )) *m_GromPtr = ( UCHAR ) data;
            m_GromAddress = ( USHORT ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
            m_GromWriteShift = 8;
            break;
        case 0x9C02 :			// GROM/GRAM Write (set) Address Port
            m_GromAddress &= ( ADDRESS ) ( 0xFF00 >> m_GromWriteShift );
            m_GromAddress |= ( ADDRESS ) ( data << m_GromWriteShift );
            m_GromWriteShift = 8 - m_GromWriteShift;
            m_GromReadShift  = 8;
            break;
        default :
            FATAL ( "Unexpected address " << hex << address );
            break;
    }
    m_GromPtr = &m_GromMemory [ m_GromAddress ];
    return data;
}

extern "C" void WriteCRU ( cTI994A *ti, ADDRESS address, int count, USHORT value )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::WriteCRU", false );

    while ( count-- ) {
        ti->WriteCRU (( ADDRESS ) ( address++ & 0x1FFF ), ( USHORT ) ( value & 1 ));
        value >>= 1;
    }
}

extern "C" int ReadCRU ( cTI994A *ti, ADDRESS address, int count )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::ReadCRU", false );

    int value = 0;
    address += ( USHORT ) count;
    while ( count-- ) {
        value <<= 1;
        value |= ti->ReadCRU (( ADDRESS ) ( --address & 0x1FFF ));
    }
    return value;
}

int cTI994A::ReadCRU ( ADDRESS address )
{
    FUNCTION_ENTRY ( this, "cTI994A::ReadCRU", false );

    address <<= 1;
    cDevice *dev = GetDevice ( address );
    return ( dev != NULL ) ? dev->ReadCRU (( ADDRESS ) (( address - dev->GetCRU ()) >> 1 )) : 1;
}

void cTI994A::WriteCRU ( ADDRESS address, USHORT val )
{
    FUNCTION_ENTRY ( this, "cTI994A::WriteCRU", false );

    address <<= 1;
    cDevice *dev = GetDevice ( address );

    if ( dev == NULL ) return;

    // See if we need to swap in/out the DSR ROM routines
    if (( address != 0 ) && ( dev->GetCRU () == address )) {
        cCartridge *ctg = m_Cartridge;
        if ( val == 1 ) {
            m_ActiveCRU = address;
            m_Cartridge = NULL;
            InsertCartridge ( dev->GetROM (), false );
            dev->Activate ();
        } else {
            m_ActiveCRU = 0;
            m_Cartridge = dev->GetROM ();
            dev->DeActivate ();
            RemoveCartridge ( dev->GetROM (), false );
        }
        m_Cartridge = ctg;
    } else {
        dev->WriteCRU (( USHORT ) (( address - dev->GetCRU ()) >> 1 ), val );
    }
}

void cTI994A::Run ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Run", true );

    m_CPU->Run ();
}

bool cTI994A::Step ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Step", true );

    return m_CPU->Step ();
}

void cTI994A::Stop ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Stop", true );

    m_CPU->Stop ();
}

bool cTI994A::IsRunning ()
{
    FUNCTION_ENTRY ( this, "cTI994A::IsRunning", true );

    return m_CPU->IsRunning ();
}

static const char ImageFileHeader[] = "TI-994/A Memory Image File\n\x1A";

bool cTI994A::OpenImageFile ( const char *filename, sImageFileState *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::OpenImageFile", true );

    if ( filename == NULL ) return false;

    info->file = fopen ( filename, "rb" );
    if ( info->file == NULL ) return false;

    char buffer [ sizeof ( ImageFileHeader ) - 1 ];
    fread ( buffer, 1, sizeof ( buffer ), info->file );

    // Make sure it's a proper memory image file
    if ( memcmp ( buffer, ImageFileHeader, sizeof ( buffer )) != 0 ) {
        ERROR ( "Inavlid memory image file" );
        fclose ( info->file );
        return false;
    }

    info->start = info->next = ftell ( info->file );

    return true;
}

bool cTI994A::FindHeader ( sImageFileState *info, HEADER_SECTION_E section )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::FindHeader", true );

    // Do a simple sanity check
    ULONG offset = ftell ( info->file );
    if ( offset != info->next ) {
        WARNING ( "Incorrect offset " << hex << offset << " - expected " << info->next );
    }

    // Go to the 1st 'unclaimed' section
    fseek ( info->file, info->start, SEEK_SET );

    sStateHeader header;
    memset ( &header, -1, sizeof ( header ));

    do {
        ULONG offset = ftell ( info->file );
        if ( fread ( &header, sizeof ( header ), 1, info->file ) != 1 ) break;
        if ( header.id == section ) {
            info->next = offset + sizeof ( header ) + header.length;
            if ( offset == info->start ) info->start = info->next;
            return true;
        }
        fseek ( info->file, header.length, SEEK_CUR );
    } while ( ! feof ( info->file ));

    ERROR ( "Header " << section << " not found" );

    fseek ( info->file, info->next, SEEK_SET );

    return false;
}

void cTI994A::MarkHeader ( FILE *file, HEADER_SECTION_E section, sStateHeaderInfo *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::MarkHeader", true );

    info->header.id     = ( USHORT ) section;
    info->header.length = ( USHORT ) -section;

    info->offset = ftell ( file );

    fwrite ( &info->header, sizeof ( info->header ), 1, file );

}

void cTI994A::SaveHeader ( FILE *file, sStateHeaderInfo *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::SaveHeader", true );

    if (( UCHAR ) info->header.id != ( UCHAR ) -info->header.length ) {
        ERROR ( "Invalid header" );
        return;
    }

    ULONG offset = ftell ( file );
    info->header.length = ( USHORT ) ( offset - info->offset - sizeof ( info->header ));

    fseek ( file, info->offset, SEEK_SET );
    fwrite ( &info->header, sizeof ( info->header ), 1, file );
    fseek ( file, offset, SEEK_SET );
}

void cTI994A::SaveImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cTI994A::SaveImage", true );

    FILE *file = fopen ( filename, "wb" );
    if ( file == NULL ) {
        ERROR ( "Unable to open file '"<< filename << "'" );
        return;
    }

    fwrite ( ImageFileHeader, 1, sizeof ( ImageFileHeader ) - 1, file );

    sStateHeaderInfo info;
    MarkHeader ( file, SECTION_BASE, &info );

    // Name of inserted Cartridge
    if ( m_Cartridge && m_Cartridge->Title ()) {
        short len = ( short ) strlen ( m_Cartridge->Title ());
        fwrite ( &len, sizeof ( len ), 1, file );
        fwrite ( m_Cartridge->Title (), len, 1, file );
    } else {
        short len = 0;
        fwrite ( &len, sizeof ( len ), 1, file );
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_CPU, &info );

    m_CPU->SaveImage ( file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_VDP, &info );

    m_VDP->SaveImage ( file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_GROM, &info );

    fwrite ( &m_GromAddress, sizeof ( m_GromAddress ), 1, file );
    fwrite ( &m_GromLastInstruction, sizeof ( m_GromLastInstruction ), 1, file );
    fwrite ( &m_GromReadShift, sizeof ( m_GromReadShift ), 1, file );
    fwrite ( &m_GromWriteShift, sizeof ( m_GromWriteShift ), 1, file );
    fwrite ( &m_GromCounter, sizeof ( m_GromCounter ), 1, file );

    for ( unsigned i = 0; i < 8; i++ ) {
        sMemoryRegion *memory = m_GromMemoryInfo [ i ];
        if ( memory == NULL ) continue;
        fputc ( memory->CurBank - memory->Bank, file );
        // Save the current bank of GRAM
        if ( memory->CurBank->Type != MEMORY_ROM ) {
            SaveBuffer ( GROM_BANK_SIZE, &m_GromMemory [ i * GROM_BANK_SIZE ], file );
        }
        // Save any other banks of GRAM
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if ( memory->Bank[j].Type == MEMORY_ROM ) continue;
            if ( memory->Bank[j].Data == NULL ) continue;
            if ( memory->CurBank == &memory->Bank [j] ) continue;
            SaveBuffer ( GROM_BANK_SIZE, memory->Bank[j].Data, file );
        }
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_CRU, &info );

    for ( unsigned i = 0; i < SIZE ( m_Device ); i++ ) {
        cDevice *dev = m_Device [i];
        if ( dev != NULL ) {
            fputc ( i, file );
            ULONG oldOffset = ftell ( file );
            USHORT size = 0;
            fwrite ( &size, sizeof ( size ), 1 , file );
            dev->SaveImage ( file );
            ULONG newOffset = ftell ( file );
            size = ( USHORT ) ( newOffset - oldOffset - sizeof ( size ));
            fseek ( file, oldOffset, SEEK_SET );
            fwrite ( &size, sizeof ( size ), 1 , file );
            fseek ( file, newOffset, SEEK_SET );
        }
    }
    fputc (( UCHAR ) -1, file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_DSR, &info );

    fwrite ( &m_ActiveCRU, sizeof ( m_ActiveCRU ), 1, file );
    if ( m_ActiveCRU != 0 ) {
        cDevice *dev = GetDevice ( m_ActiveCRU );
        dev->SaveImage ( file );
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_ROM, &info );

    for ( unsigned i = 0; i < 16; i++ ) {
        sMemoryRegion *memory = m_CpuMemoryInfo [ i ];
        if ( memory == NULL ) {
            fputc ( 0, file );
            continue;
        }
        fputc ( memory->NumBanks, file );
        fputc ( memory->CurBank - memory->Bank, file );

        if ( memory->CurBank->Type == MEMORY_ROM ) {
            fputc ( 0, file );
        } else {
            fputc ( 1, file );
            SaveBuffer ( ROM_BANK_SIZE, &CpuMemory [ i << 12 ], file );
        }

        // Save any other banks of RAM
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if (( memory->Bank[j].Type == MEMORY_ROM ) || ( memory->Bank[j].Data == NULL )) {
                fputc ( 0, file );
            } else {
                fputc ( 1, file );
                SaveBuffer ( ROM_BANK_SIZE, memory->Bank[j].Data, file );
            }
        }
    }

    SaveHeader ( file, &info );

    fclose ( file );
}

bool cTI994A::LoadImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cTI994A::LoadImage", true );

    sImageFileState info;
    if ( OpenImageFile ( filename, &info ) == false ) {
        return false;
    }

    FindHeader ( &info, SECTION_BASE );

    // Make sure cartridge(s) match those currently loaded
    short len;
    fread ( &len, sizeof ( len ), 1, info.file );
    bool ok = true;
    bool haveCartridge = ( m_Cartridge && m_Cartridge->Title ()) ? true : false;
    if ( len != 0 ) {
        char *nameBuffer = new char [ len + 1 ];
        fread ( nameBuffer, len, 1, info.file );
        nameBuffer [len] = '\0';
        if (( haveCartridge == false ) || strnicmp ( nameBuffer, m_Cartridge->Title (), len )) ok = false;
        delete [] nameBuffer;
    } else {
        if ( haveCartridge == true ) ok = false;
    }
    if ( ! ok ) {
        fclose ( info.file );
        return false;
    }

    FindHeader ( &info, SECTION_CPU );

    m_CPU->LoadImage ( info.file );

    FindHeader ( &info, SECTION_VDP );

    m_VDP->LoadImage ( info.file );

    FindHeader ( &info, SECTION_GROM );

    fread ( &m_GromAddress, sizeof ( m_GromAddress ), 1, info.file );
    fread ( &m_GromLastInstruction, sizeof ( m_GromLastInstruction ), 1, info.file );
    fread ( &m_GromReadShift, sizeof ( m_GromReadShift ), 1, info.file );
    fread ( &m_GromWriteShift, sizeof ( m_GromWriteShift ), 1, info.file );
    fread ( &m_GromCounter, sizeof ( m_GromCounter ), 1, info.file );

    m_GromPtr = &m_GromMemory [ m_GromAddress ];

    for ( unsigned i = 0; i < 8; i++ ) {
        sMemoryRegion *memory = m_GromMemoryInfo [ i ];
        if ( memory == NULL ) continue;
        UCHAR bank = ( UCHAR ) fgetc ( info.file );
        memory->CurBank = &memory->Bank [ bank ];
        if ( memory->CurBank->Type != MEMORY_ROM ) {
            LoadBuffer ( GROM_BANK_SIZE, &m_GromMemory [ i * GROM_BANK_SIZE ], info.file );
        }
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if ( memory->Bank[j].Type == MEMORY_ROM ) continue;
            if ( memory->Bank[j].Data == NULL ) continue;
            if ( memory->CurBank == &memory->Bank [j] ) continue;
            LoadBuffer ( GROM_BANK_SIZE, memory->Bank[j].Data, info.file );
        }
    }

    FindHeader ( &info, SECTION_CRU );

    for ( EVER ) {
        int i = fgetc ( info.file );
        if ( i == ( UCHAR ) -1 ) break;
        USHORT size;
        fread ( &size, sizeof ( size ), 1, info.file );
        ULONG offset = ftell ( info.file );
        cDevice *dev = m_Device [i];
        if ( dev != NULL ) {
            dev->LoadImage ( info.file );
        }
        fseek ( info.file, offset + size, SEEK_SET );
    }

    FindHeader ( &info, SECTION_DSR );

    if ( m_ActiveCRU ) {
        cDevice *dev = GetDevice ( m_ActiveCRU );
        dev->DeActivate ();
        cCartridge *ctg = m_Cartridge;
        m_Cartridge = dev->GetROM ();
        RemoveCartridge ( dev->GetROM (), false );
        m_Cartridge = ctg;
    }
    fread ( &m_ActiveCRU, sizeof ( m_ActiveCRU ), 1, info.file );
    if ( m_ActiveCRU ) {
        cDevice *dev = GetDevice ( m_ActiveCRU );
        dev->LoadImage ( info.file );
        cCartridge *ctg = m_Cartridge;
        m_Cartridge = NULL;
        InsertCartridge ( dev->GetROM (), false );
        dev->Activate ();
        m_Cartridge = ctg;
    }

    FindHeader ( &info, SECTION_ROM );

    for ( unsigned i = 0; i < 16; i++ ) {
        fgetc ( info.file );
        sMemoryRegion *memory = m_CpuMemoryInfo [ i ];
        if ( memory == NULL ) continue;
        UCHAR bank = ( UCHAR ) fgetc ( info.file );
        memory->CurBank = &memory->Bank [ bank ];

        if ( fgetc ( info.file ) == 1 ) {
            LoadBuffer ( ROM_BANK_SIZE, &CpuMemory [ i << 12 ], info.file );
        }

        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if ( fgetc ( info.file ) == 0 ) continue;
            LoadBuffer ( ROM_BANK_SIZE, memory->Bank[j].Data, info.file );
        }
    }

    fclose ( info.file );

    Refresh ( true );

    return true;
}

void cTI994A::Reset ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Reset", true );

    if ( m_CPU != NULL ) m_CPU->Reset ();
    if ( m_VDP != NULL ) m_VDP->Reset ();
    if ( m_SpeechSynthesizer != NULL ) m_SpeechSynthesizer->Reset ();
}

void cTI994A::AddDevice ( cDevice *dev )
{
    FUNCTION_ENTRY ( this, "cTI994A::AddDevice", true );

    int index = ( dev->GetCRU () >> 8 ) & 0x1F;
    if ( m_Device [index] != NULL ) {
        WARNING ( "A CRU device already exists at address " << index );
        return;
    }
    m_Device [index] = dev;
    dev->SetCPU ( m_CPU );
}

void cTI994A::InsertCartridge ( cCartridge *cartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cTI994A::InsertCartridge", true );

    if ( m_Cartridge != NULL ) {
        cCartridge *ctg = m_Cartridge;
        RemoveCartridge ( ctg );
        delete ctg;
    }

    if ( cartridge == NULL ) return;
    m_Cartridge = cartridge;

    for ( unsigned i = 0; i < SIZE ( m_CpuMemoryInfo ); i++ ) {
        if ( m_Cartridge->CpuMemory [i].NumBanks > 0 ) {
            m_CpuMemoryInfo [i] = &m_Cartridge->CpuMemory [i];
            m_CpuMemoryInfo [i]->CurBank = &m_CpuMemoryInfo [i]->Bank[0];
            if ( m_CpuMemoryInfo [i]->NumBanks > 1 ) {
                UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );
                USHORT address = ( USHORT ) ( i << 12 );
                for ( unsigned j = 0; j < ROM_BANK_SIZE; j++ ) {
                    m_CPU->SetBreakpoint ( address++, MEMFLG_WRITE, true, bkp );
                }
            }
            MEMORY_ACCESS_E memType = ( m_CpuMemoryInfo [i]->CurBank->Type == MEMORY_ROM ) ? MEM_ROM : MEM_RAM;
            m_CPU->SetMemory ( memType, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            memcpy ( &CpuMemory [ i << 12 ], m_CpuMemoryInfo [i]->CurBank->Data, ROM_BANK_SIZE );
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
        if ( m_Cartridge->GromMemory[i].NumBanks > 0 ) {
            m_GromMemoryInfo [i] = &m_Cartridge->GromMemory [i];
            m_GromMemoryInfo [i]->CurBank = &m_GromMemoryInfo [i]->Bank[0];
            memcpy ( &m_GromMemory [ i << 13 ], m_GromMemoryInfo [i]->CurBank->Data, GROM_BANK_SIZE );
        }
    }

    if ( reset ) Reset ();
}

void cTI994A::RemoveCartridge ( cCartridge *cartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cTI994A::RemoveCartridge", true );

    if ( cartridge != m_Cartridge ) return;

    // Save any battery-backed RAM to the cartridge and disabel bank-switched regions
    if ( m_Cartridge != NULL ) {
        for ( unsigned i = 0; i < SIZE ( m_CpuMemoryInfo ); i++ ) {
            if ( m_Cartridge->CpuMemory [i].NumBanks == 0 ) continue;
            // If this bank is RAM & Battery backed - update the cartridge
            if ( m_Cartridge->CpuMemory [i].CurBank->Type == MEMORY_BATTERY_BACKED ) {
                memcpy ( m_CpuMemoryInfo [i]->CurBank->Data, &CpuMemory [ i << 12 ], ROM_BANK_SIZE );
            }
            if ( m_Cartridge->CpuMemory [i].NumBanks > 1 ) {
                // Clears bankswitch breakpoint for ALL regions!
                UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );
                m_CPU->ClearBreakpoint ( bkp );
            }
        }

        for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
            if ( m_Cartridge->GromMemory [i].NumBanks == 0 ) continue;
            // If this bank is RAM & Battery backed - update the cartridge
            if ( m_Cartridge->GromMemory [i].CurBank->Type == MEMORY_BATTERY_BACKED ) {
                memcpy ( m_GromMemoryInfo [i]->CurBank->Data, &m_GromMemory [ i << 13 ], GROM_BANK_SIZE );
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_Cartridge->CpuMemory ); i++ ) {
        if (( m_Cartridge != NULL ) && ( m_Cartridge->CpuMemory [i].NumBanks == 0 )) continue;
        if (( m_Console != NULL ) && ( m_Console->CpuMemory[i].CurBank != NULL )) {
            MEMORY_ACCESS_E memType = ( m_Console->CpuMemory[i].CurBank->Type == MEMORY_ROM ) ? MEM_ROM : MEM_RAM;
            m_CPU->SetMemory ( memType, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            m_CpuMemoryInfo [i] = &m_Console->CpuMemory [i];
            // Don't clear out memory!?
            memcpy ( &CpuMemory [ i << 12 ], m_CpuMemoryInfo [i]->CurBank->Data, ROM_BANK_SIZE );
        } else {
            m_CpuMemoryInfo [i] = NULL;
            m_CPU->SetMemory ( MEM_ROM, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            memset ( &CpuMemory [ i << 12 ], 0, ROM_BANK_SIZE );
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
        if (( m_Cartridge != NULL ) && ( m_Cartridge->GromMemory [i].NumBanks == 0 )) continue;
        if (( m_Console != NULL ) && ( m_Console->GromMemory[i].CurBank != NULL )) {
            m_GromMemoryInfo [i] = &m_Console->GromMemory [i];
            memcpy ( &m_GromMemory [ i << 13 ], m_GromMemoryInfo [i]->CurBank->Data, GROM_BANK_SIZE );
        } else {
            m_GromMemoryInfo [i] = NULL;
            memset ( &m_GromMemory [ i << 13 ], 0, GROM_BANK_SIZE );
        }
    }

    m_Cartridge = NULL;

    if ( reset ) Reset ();
}
