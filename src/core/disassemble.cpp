//----------------------------------------------------------------------------
//
// File:        disassemble.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description: Simple disassembler for the TMS9900 processor
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
#include "tms9900.hpp"

static int     bUseR = 1;
static char   *pBuffer;
static USHORT  ByteTable [ 256 + 1 ];

extern "C" sLookUp LookUp [ 16 ];

static bool InitByteTable ()
{
    for ( unsigned  i = 0; i < SIZE ( ByteTable ) - 1; i++ ) {
        sprintf (( char * ) &ByteTable [i], "%02X", i );
    }

    return true;
}

static bool initialized = InitByteTable ();

static void AddReg ( int reg )
{
    *pBuffer = 'R';
    pBuffer += bUseR;
    if ( reg >= 10 ) {
        reg -= 10;
        *pBuffer++ = '1';
    }
    *pBuffer++ = ( char ) ( '0' + reg );
}

static void AddDigit ( int num )
{
    if ( num >= 10 ) {
        num -= 10;
        *pBuffer++ = '1';
    }
    *pBuffer++ = ( char ) ( '0' + num );
}

static void AddByte ( UCHAR data )
{
    memcpy ( pBuffer, ByteTable + data, 2 ), pBuffer += 2;
}

static void AddWord ( USHORT data )
{
    memcpy ( pBuffer, ByteTable + ( data >> 8 ), 2 ), pBuffer += 2;
    memcpy ( pBuffer, ByteTable + ( data & 0x00FF ), 2 ), pBuffer += 2;
}

static void GetRegs ( USHORT op, USHORT data )
{
    int reg = op & 0x0F;
    int mode = ( op >> 4 ) & 0x03;
    switch ( mode ) {
        case 0 : AddReg ( reg );
                 break;
        case 2 : *pBuffer++ = '@';
                 *pBuffer++ = '>';
                 AddWord ( data );
                 if ( reg != 0 ) {
                     *pBuffer++ = '(';
                     AddReg ( reg );
                     *pBuffer++ = ')';
                 }
                 break;
        case 1 :
        case 3 : *pBuffer++ = '*';
                 AddReg ( reg );
                 if ( mode == 3 ) *pBuffer++ = '+';
                 break;
    }
}

static void format_I ( USHORT opcode, USHORT arg1, USHORT arg2 )
{
    GetRegs ( opcode, arg1 );
    *pBuffer++ = ',';
    GetRegs (( USHORT ) ( opcode >> 6 ), arg2 );
}

static void format_II ( USHORT opcode, USHORT PC )
{
    if ( opcode == 0x1000 ) {
        strcpy ( pBuffer - 5, "NOP" );
    } else {
        char disp = ( char ) opcode;
        *pBuffer++ = '>';
        if ( opcode >= 0x1D00 ) {
            AddByte ( disp );
        } else {
            AddWord (( USHORT ) ( PC + disp * 2 ));
        }
    }
}

static void format_III ( USHORT opcode, USHORT arg1 )
{
    GetRegs ( opcode, arg1 );
    *pBuffer++ = ',';
    AddReg (( opcode >> 6 ) & 0xF );
}

static void format_IV ( USHORT opcode, USHORT arg1 )
{
    GetRegs ( opcode, arg1 );
    UCHAR disp = ( UCHAR ) (( opcode >> 6 ) & 0xF );
    *pBuffer++ = ',';
    AddDigit ( disp ? disp : 16 );
}

static void format_V ( USHORT opcode )
{
    AddReg ( opcode & 0xF );
    *pBuffer++ = ',';
    AddDigit (( opcode >> 4 ) & 0xF );
}

static void format_VI ( USHORT opcode, USHORT arg1 )
{
    GetRegs ( opcode, arg1 );
}

static void format_VIII ( USHORT opcode, USHORT arg1 )
{
    if ( opcode < 0x02A0 ) {
        AddReg ( opcode & 0x000F );
        *pBuffer++ = ',';
        *pBuffer++ = '>';
        AddWord ( arg1 );
    } else if ( opcode >= 0x02E0 ) {
        *pBuffer++ = '>';
        AddWord ( arg1 );
    } else {
        AddReg ( opcode & 0x000F );
    }
}

static inline USHORT GetWord ( UCHAR *ptr )
{
    return ( USHORT ) (( ptr [0] << 8 ) | ptr [1] );
}

static int GetArgs ( USHORT PC, USHORT *ptr, sOpCode *op, USHORT opcode )
{
    USHORT arg1 = 0, arg2 = 0;
    void (*format) ( USHORT, USHORT );
    int index = 0;
    switch ( op->format ) {
        case 1 :				// Two General Addresses
                 if (( opcode & 0x0030 ) == 0x0020 ) arg1 = GetWord (( UCHAR * ) &ptr [++index] );
                 if (( opcode & 0x0C00 ) == 0x0800 ) arg2 = GetWord (( UCHAR * ) &ptr [++index] );
                 format_I ( opcode, arg1, arg2 );
                 return index * 2;
        case 2 : format_II ( opcode, PC );
                 return 0;
        case 5 : format_V ( opcode );
                 return 0;
        case 3 : format = format_III;	break;	// Logical
        case 4 : format = format_IV;	break;	// CRU Multi-Bit
        case 6 : format = format_VI;	break;	// Single Address
        case 9 : format = format_III;	break;	// XOP, MULT, & DIV
        case 8 :				// Immediate
                 if (( opcode < 0x02A0 ) || ( opcode >= 0x02E0 )) arg1 = GetWord (( UCHAR * ) &ptr [++index] );
                 format_VIII ( opcode, arg1 );
                 return index * 2;
        case 7 : //format_VII ( opcode, arg1, arg2 );
                 return 0;
        default :
                 return 0;
    }

    if (( opcode & 0x0030 ) == 0x0020 ) arg1 = GetWord (( UCHAR * ) &ptr [++index] );
    format ( opcode, arg1 );

    return index * 2;
}

USHORT DisassembleASM ( USHORT PC, UCHAR *ptr, char *buffer )
{
    pBuffer = buffer;
    AddWord ( PC );
    *pBuffer++ = ' ';

    if ( PC & 1 ) {
        strcpy ( pBuffer, "<-- Illegal value in PC" );
        return ( USHORT ) ( PC + 1 );
    }

    USHORT curOpCode = GetWord ( ptr );
    PC += ( USHORT ) 2;

    sLookUp *lookup = &LookUp [ curOpCode >> 12 ];
    sOpCode *op = lookup->opCode;
    int retries = lookup->size;
    if ( retries ) {
        while (( curOpCode & op->mask ) != op->opCode ) {
            op++;
            if ( retries-- == 0 ) {
                strcpy ( pBuffer, "Invalid Op-Code" );
                return PC;
            }
        }
    }

    memcpy ( pBuffer, op->mnemonic, 4 ), pBuffer += 4;

    if ( op->format != 7 ) {
        *pBuffer++ = ' ';
        PC += GetArgs ( PC, ( USHORT * ) ptr, op, curOpCode );
    }

    *pBuffer = '\0';

    return PC;
}
