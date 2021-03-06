//----------------------------------------------------------------------------
//
// File:        opcodes.cpp
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
//   25-Jun-2000    MAR Added hack provided by Jeremy Stanley for MSVC bug
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include "common.hpp"
#include "tms9900.hpp"
#include "opcodes.hpp"
#include "device.hpp"
#include "tms9901.hpp"

#include "psp_kbd.h"

#define WP  WorkspacePtr
#define PC  ProgramCounter
#define ST  Status

static int     runFlag;
static int     stopFlag;
static USHORT  curOpCode;

static USHORT  InterruptMask = 1;

extern "C" {

    extern UCHAR   CpuMemory[ 0x10000 ];
    extern UCHAR   MemFlags[ 0x10000 ];

    USHORT  InterruptFlag;
    USHORT  WorkspacePtr;
    USHORT  ProgramCounter;
    USHORT  Status;
    ULONG   InstructionCounter;
    ULONG   ClockCycleCounter;

    bool Step ();
    void Run ();
    void Stop ();
    bool IsRunning ();

};

extern cTMS9901 *pic;

extern "C" void   *CRU_Object;
extern "C" sLookUp LookUp [ 16 ];
extern "C" USHORT  parity [ 256 ];

extern "C" void InvalidOpcode ();
extern "C" ULONG (*TimerHook) ();
extern "C" USHORT CallTrapB ( bool read, int index, const ADDRESS address, USHORT value );
extern "C" USHORT CallTrapW ( bool read, int index, const ADDRESS address, USHORT value );
extern "C" USHORT ReadCRU ( void *, int, int );
extern "C" void WriteCRU ( void *, int, int, USHORT );

// ----------------------------------
//char argBuffer [132];
//char *argPtr = argBuffer;
// ----------------------------------

static inline USHORT ReadMemoryW ( USHORT address )
{
    UCHAR flags = MemFlags [ address ];

    if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4;

    // This is a hack to work around the scratch pad RAM memory addressing
    if (( flags & ( MEMFLG_8BIT | MEMFLG_ROM )) == 0 ) {
        flags = 0;
        address |= 0x8300;
    }

    USHORT retVal = ( USHORT ) (( CpuMemory [ address ] << 8 ) | CpuMemory [ address + 1 ] );

    if ( flags & MEMFLG_READ ) {
        retVal = CallTrapW ( true, ( flags & MEMFLG_INDEX_MASK ) >> MEMFLG_INDEX_SHIFT, address, retVal );
    }

//argPtr += sprintf ( argPtr, "  R>%04X=%04X", address, retVal );

    return retVal;
}

UCHAR ReadMemoryB ( USHORT address )
{
    UCHAR flags = MemFlags [ address ];

    if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4;

    // This is a hack to work around the scratch pad RAM memory addressing
    if (( flags & ( MEMFLG_8BIT | MEMFLG_ROM )) == 0 ) {
        flags = 0;
        address |= 0x8300;
    }

    UCHAR retVal = * ( UCHAR * ) &CpuMemory [ address ];

    if ( flags & MEMFLG_READ ) {
        retVal = ( UCHAR ) CallTrapB ( true, ( flags & MEMFLG_INDEX_MASK ) >> MEMFLG_INDEX_SHIFT, address, retVal );
    }

//argPtr += sprintf ( argPtr, "  R>%04X=%02X  ", address, retVal );

    return retVal;
}

void WriteMemoryW ( USHORT address, USHORT value, int penalty = 4 )
{
    UCHAR flags = MemFlags [ address ];

//argPtr += sprintf ( argPtr, "  W>%04X=%04X", address, value );

    if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4 + penalty;

    // This is a hack to work around the scratch pad RAM memory addressing
    if (( flags & ( MEMFLG_8BIT | MEMFLG_ROM )) == 0 ) {
        flags = 0;
        address |= 0x8300;
    }

    if ( flags & ( MEMFLG_WRITE | MEMFLG_ROM )) {
        if ( flags & MEMFLG_WRITE ) {
            value = CallTrapW ( false, ( flags & MEMFLG_INDEX_MASK ) >> MEMFLG_INDEX_SHIFT, address, value );
        }
        if ( flags & MEMFLG_ROM ) return;
    }

    UCHAR *ptr = &CpuMemory [ address];
    ptr [0] = ( UCHAR ) ( value >> 8 );
    ptr [1] = ( UCHAR ) value;
}

void WriteMemoryB ( USHORT address, UCHAR value, int penalty = 4 )
{
    UCHAR flags = MemFlags [ address ];

//argPtr += sprintf ( argPtr, "  W>%04X=%02X  ", address, value );

    if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4 + penalty;

    // This is a hack to work around the scratch pad RAM memory addressing
    if (( flags & ( MEMFLG_8BIT | MEMFLG_ROM )) == 0 ) {
        flags = 0;
        address |= 0x8300;
    }

    if ( flags & ( MEMFLG_WRITE | MEMFLG_ROM )) {
        if ( flags & MEMFLG_WRITE ) {
            value = ( UCHAR ) CallTrapB ( false, ( flags & MEMFLG_INDEX_MASK ) >> MEMFLG_INDEX_SHIFT, address, value );
        }
        if ( flags & MEMFLG_ROM ) return;
    }

    CpuMemory [ address ] = value;
}

USHORT Fetch ()
{
    USHORT retVal = ReadMemoryW ( PC );
    PC += 2;
    return retVal;
}

void ContextSwitch ( USHORT newWP, USHORT newPC )
{
    USHORT oldWP = WP;
    USHORT oldPC = PC;
    WP = newWP;
    PC = newPC;
    WriteMemoryW ( WP + 2 * 13, oldWP, 0 );
    WriteMemoryW ( WP + 2 * 14, oldPC, 0 );
    WriteMemoryW ( WP + 2 * 15, ST,    0 );
}

// ----------------------------------
//#include <string.h>
//USHORT DisassembleASM ( USHORT PC, UCHAR *ptr, char *buffer );
// ----------------------------------

void ExecuteInstruction ( USHORT opCode )
{
    sLookUp *lookup = &LookUp [ opCode >> 12 ];
    sOpCode *op = lookup->opCode;
    int retries = lookup->size;

    do {
        if (( opCode & op->mask ) == op->opCode ) goto match;
        op++;
    } while ( retries-- > 0 );

    // Add minimum clock cycle count
    ClockCycleCounter += 6;

    InvalidOpcode ();
    return;

match:

    ClockCycleCounter += op->clocks;

// ----------------------------------
//bool log = ((( PC-2 >= 0x2000 ) && ( PC-2 < 0x4000 )) || ( PC-2 >= 0x6000 )) ? true : false;
//char buffer [80];
//argPtr = argBuffer;
//*argPtr = '\0';
//DisassembleASM ( PC-2, &CpuMemory [ PC-2 ], buffer );
// ----------------------------------

    ((void(*)()) op->function ) ();

// ----------------------------------
//if ( log ) fprintf ( stderr, "%-*.*s%s\n", 32, 32, buffer, argBuffer );
// ----------------------------------

    op->count++;
}

//             T   Clk Acc
// Rx          00   0   0          Register
// *Rx         01   4   1          Register Indirect
// *Rx+        11   6   2 (byte)   Auto-increment
// *Rx+        11   8   2 (word)   Auto-increment
// @>xxxx      10   8   1          Symbolic Memory
// @>xxxx(Rx)  10   8   2          Indexed Memory
//

USHORT GetAddress ( USHORT opCode, int size )
{
    USHORT address = 0;
    int reg = opCode & 0x0F;

    switch ( opCode & 0x0030 ) {
        case 0x0000 : address = ( USHORT ) ( WP + 2 * reg );
                      break;
        case 0x0010 : address = ReadMemoryW ( WP + 2 * reg );
                      ClockCycleCounter += 4;
                      break;
        case 0x0030 : address = ReadMemoryW ( WP + 2 * reg );
                      WriteMemoryW ( WP + 2 * reg, ( USHORT ) ( address + size ), 0 );
                      ClockCycleCounter += 4 + 2 * size;
                      break;
        case 0x0020 : if ( reg ) address = ReadMemoryW ( WP + 2 * reg );
                      address += Fetch ();
                      ClockCycleCounter += 8;
                      break;
    }

    if ( size != 1 ) {
        address &= 0xFFFE;
    }

    return address;
}

bool CheckInterrupt ()
{
    // Tell the PIC to update it's timer and turn off old interrupts
    pic->UpdateTimer ( ClockCycleCounter );

    // Look for pending unmasked interrupts
    USHORT mask = ( USHORT ) (( 2 << ( ST & 0x0F )) - 1 );
    if (( InterruptFlag & InterruptMask ) == 0 ) return false;

    // Find the highest priority interrupt
    int level = 0;
    mask = 1;
    while (( InterruptFlag & mask ) == 0 ) {
        level++;
        mask <<= 1;
    }
    InterruptFlag &= ~mask;

    USHORT newWP = ReadMemoryW ( level * 4 );
    USHORT newPC = ReadMemoryW ( level * 4 + 2 );
    ContextSwitch ( newWP, newPC );

    InterruptMask = ( USHORT ) ((( 2 << level ) - 1 ) >> 1 );

    if ( level != 0 ) {
        ST &= 0xFFF0;
        ST |= level - 1;
    }

    return true;
}

bool Step ()
{
    runFlag++;

    if ( CheckInterrupt () == true ) return false;

    curOpCode = Fetch ();
    ExecuteInstruction ( curOpCode );
    InstructionCounter++;

    if ((( char ) InstructionCounter == 0 ) && ( TimerHook != NULL )) {
        TimerHook ();
    }

    runFlag--;
    if ( stopFlag ) {
        stopFlag--;
        return true;
    }

    return false;
}

void Run ()
{
    runFlag++;

    do {

        CheckInterrupt ();

        curOpCode = Fetch ();
        ExecuteInstruction ( curOpCode );
        InstructionCounter++;

        if ((( char ) InstructionCounter == 0 ) && ( TimerHook != NULL )) {
            TimerHook ();
        }

    } while ( stopFlag == 0 );

    stopFlag--;
    runFlag--;
}

void Stop ()
{
    stopFlag++;
}

bool IsRunning ()
{
    return ( runFlag != 0 ) ? true : false;
}

void SetFlags_LAE ( USHORT val )
{
    if (( short ) val > 0 ) {
        ST |= TMS_LOGICAL | TMS_ARITHMETIC;
    } else if (( short ) val < 0 ) {
        ST |= TMS_LOGICAL;
    } else {
        ST |= TMS_EQUAL;
    }
}

void SetFlags_LAE ( USHORT val1, USHORT val2 )
{
    if ( val1 == val2 ) {
        ST |= TMS_EQUAL;
    } else {
#if defined ( _MSC_VER ) && ( _MSC_VER < 1300 ) // Need to work around a bug in Visual C++
        if (( short ) val1 >= ( short ) val2 + 1 ) {
#else
        if (( short ) val1 > ( short ) val2 ) {
#endif
            ST |= TMS_ARITHMETIC;
        }
        if ( val1 > val2 ) {
            ST |= TMS_LOGICAL;
        }
    }
}

void SetFlags_difW ( USHORT val1, USHORT val2, ULONG res )
{
    if ( ! ( res & 0x00010000 )) ST |= TMS_CARRY;
    if (( val1 ^ val2 ) & ( val1 ^ res ) & 0x8000 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( USHORT ) res );
}

void SetFlags_difB ( UCHAR val1, UCHAR val2, ULONG res )
{
    if ( ! (  res & 0x0100 )) ST |= TMS_CARRY;
    if (( val1 ^ val2 ) & ( val1 ^ res ) & 0x80 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( char ) res );
    ST |= parity [ ( UCHAR ) res ];
}

void SetFlags_sumW ( USHORT val1, USHORT val2, ULONG res )
{
    if ( res & 0x00010000 ) ST |= TMS_CARRY;
    if (( res ^ val1 ) & ( res ^ val2 ) & 0x8000 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( USHORT ) res );
}

void SetFlags_sumB ( UCHAR val1, UCHAR val2, ULONG res )
{
    if ( res & 0x0100 ) ST |= TMS_CARRY;
    if (( res ^ val1 ) & ( res ^ val2 ) & 0x80 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( char ) res );
    ST |= parity [ ( UCHAR ) res ];
}

//-----------------------------------------------------------------------------
//   LI		Format: VIII	Op-code: 0x0200		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_LI ()
{
    USHORT value = Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), value, 0 );
}

//-----------------------------------------------------------------------------
//   AI		Format: VIII	Op-code: 0x0220		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_AI ()
{
    int reg = curOpCode & 0x0F;

    ULONG src = ReadMemoryW ( WP + 2 * reg );
    ULONG dst = Fetch ();
    ULONG sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( USHORT ) src, ( USHORT ) dst, sum );

    WriteMemoryW ( WP + 2 * reg, ( USHORT ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   ANDI	Format: VIII	Op-code: 0x0240		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_ANDI ()
{
    USHORT reg = ( USHORT ) ( curOpCode & 0x000F );
    USHORT value = ReadMemoryW ( WP + 2 * reg );
    value &= Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   ORI	Format: VIII	Op-code: 0x0260		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_ORI ()
{
    USHORT reg = ( USHORT ) ( curOpCode & 0x000F );
    USHORT value = ReadMemoryW ( WP + 2 * reg );
    value |= Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   CI		Format: VIII	Op-code: 0x0280		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_CI ()
{
    USHORT src = ReadMemoryW ( WP + 2 * ( curOpCode & 0x000F ));
    USHORT dst = Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src, dst );
}

//-----------------------------------------------------------------------------
//   STWP	Format: VIII	Op-code: 0x02A0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_STWP ()
{
    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), WP, 0 );
}

//-----------------------------------------------------------------------------
//   STST	Format: VIII	Op-code: 0x02C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_STST ()
{
    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), ST );
}

//-----------------------------------------------------------------------------
//   LWPI	Format: VIII	Op-code: 0x02E0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LWPI ()
{
    WP = Fetch ();
}

//-----------------------------------------------------------------------------
//   LIMI	Format: VIII	Op-code: 0x0300		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LIMI ()
{
    ST = ( USHORT ) (( ST & 0xFFF0 ) | ( Fetch () & 0x0F ));
    InterruptMask = ( USHORT ) (( 2 << ( ST & 0x0F )) - 1 );
}

//-----------------------------------------------------------------------------
//   IDLE	Format: VII	Op-code: 0x0340		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_IDLE ()
{
    for ( EVER ) {
        if ( CheckInterrupt () == true ) return;
        TimerHook ();
        ClockCycleCounter += 4;
    }
}

//-----------------------------------------------------------------------------
//   RSET	Format: VII	Op-code: 0x0360		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_RSET ()
{
    // Set the interrupt mask to 0
    ST &= 0xFFF0;
    InterruptMask = 1;
}

//-----------------------------------------------------------------------------
//   RTWP	Format: VII	Op-code: 0x0380		Status: L A E C O P X
//-----------------------------------------------------------------------------
void opcode_RTWP ()
{
    ST = ReadMemoryW ( WP + 2 * 15 );
    PC = ReadMemoryW ( WP + 2 * 14 );
    WP = ReadMemoryW ( WP + 2 * 13 );
    InterruptMask = ( USHORT ) (( 2 << ( ST & 0x0F )) - 1 );
}

//-----------------------------------------------------------------------------
//   CKON	Format: VII	Op-code: 0x03A0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CKON () {}

//-----------------------------------------------------------------------------
//   CKOF	Format: VII	Op-code: 0x03C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CKOF () {}

//-----------------------------------------------------------------------------
//   LREX	Format: VII	Op-code: 0x03E0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LREX () {}

//-----------------------------------------------------------------------------
//   BLWP	Format: VI	Op-code: 0x0400		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_BLWP ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    USHORT newWP = ReadMemoryW ( address );
    USHORT newPC = ReadMemoryW ( address + 2 );
    ContextSwitch ( newWP, newPC );
}

//-----------------------------------------------------------------------------
//   B		Format: VI	Op-code: 0x0440		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_B ()
{
    PC = GetAddress ( curOpCode, 2 );
}

//-----------------------------------------------------------------------------
//   X		Format: VI	Op-code: 0x0480		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_X ()
{
    curOpCode = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    ExecuteInstruction ( curOpCode );
}

//-----------------------------------------------------------------------------
//   CLR	Format: VI	Op-code: 0x04C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CLR ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( address, ( USHORT ) 0, 4 );
}

//-----------------------------------------------------------------------------
//   NEG	Format: VI	Op-code: 0x0500		Status: L A E - O - -
//-----------------------------------------------------------------------------
void opcode_NEG ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( address );

    ULONG dst = 0 - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW );
    SetFlags_LAE (( USHORT ) dst );
    if (( src ^ dst ) & 0x8000 ) ST |= TMS_OVERFLOW;

    WriteMemoryW ( address, ( USHORT ) dst, 0 );
}

//-----------------------------------------------------------------------------
//   INV	Format: VI	Op-code: 0x0540		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_INV ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    USHORT value = ~ ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( address, value, 0 );
}

//-----------------------------------------------------------------------------
//   INC	Format: VI	Op-code: 0x0580		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_INC ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( address );

    ULONG sum = src + 1;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( USHORT ) src, 1, sum );

    WriteMemoryW ( address, ( USHORT ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   INCT	Format: VI	Op-code: 0x05C0		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_INCT ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( address );

    ULONG sum = src + 2;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( USHORT ) src, 2, sum );

    WriteMemoryW ( address, ( USHORT ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   DEC	Format: VI	Op-code: 0x0600		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_DEC ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    ULONG src = ReadMemoryW ( address );

    ULONG dif = src - 1;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( USHORT ) src, 1, dif );

    WriteMemoryW ( address, ( USHORT ) dif, 0 );
}

//-----------------------------------------------------------------------------
//   DECT	Format: VI	Op-code: 0x0640		Status: L A E C O - -
    //-----------------------------------------------------------------------------
void opcode_DECT ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    ULONG src = ReadMemoryW ( address );

    ULONG dif = src - 2;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( USHORT ) src, 2, dif );

    WriteMemoryW ( address, ( USHORT ) dif, 0 );
}

//-----------------------------------------------------------------------------
//   BL		Format: VI	Op-code: 0x0680		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_BL ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( WP + 2 * 11, PC, 4 );
    PC = address;
}

//-----------------------------------------------------------------------------
//   SWPB	Format: VI	Op-code: 0x06C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SWPB ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    USHORT value = ReadMemoryW ( address );
    value = ( USHORT ) (( value << 8 ) | ( value >> 8 ));
    WriteMemoryW ( address, ( USHORT ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SETO	Format: VI	Op-code: 0x0700		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SETO ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( address, ( USHORT ) -1, 4 );
}

//-----------------------------------------------------------------------------
//   ABS	Format: VI	Op-code: 0x0740		Status: L A E - O - -
//-----------------------------------------------------------------------------
void opcode_ABS ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    USHORT dst = ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW );
    SetFlags_LAE ( dst );

    if ( dst & 0x8000 ) {
        ClockCycleCounter += 2;
        WriteMemoryW ( address, -dst, 0 );
        ST |= TMS_OVERFLOW;
    }
}

//-----------------------------------------------------------------------------
//   SRA	Format: V	Op-code: 0x0800		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRA ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY );

    short value = ( short ) ((( short ) ReadMemoryW ( WP + 2 * reg )) >> --count );
    if ( value & 1 ) ST |= TMS_CARRY;
    value >>= 1;
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, ( USHORT ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SRL	Format: V	Op-code: 0x0900		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRL ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY );

    USHORT value = ( USHORT ) ( ReadMemoryW ( WP + 2 * reg ) >> --count );
    if ( value & 1 ) ST |= TMS_CARRY;
    value >>= 1;
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   SLA	Format: V	Op-code: 0x0A00		Status: L A E C O - -
//
// Comments: The overflow bit is set if the sign changes during the shift
//-----------------------------------------------------------------------------
void opcode_SLA ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );

    long value = ReadMemoryW ( WP + 2 * reg ) << count;

    ULONG mask = (( USHORT ) -1 << count ) & 0xFFFF8000;
    int bits = value & mask;

    if ( value & 0x00010000 ) ST |= TMS_CARRY;
    if ( bits && ( bits ^ mask )) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( USHORT ) value );

    WriteMemoryW ( WP + 2 * reg, ( USHORT ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SRC	Format: V	Op-code: 0x0B00		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRC ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );

    int value = ReadMemoryW ( WP + 2 * reg );
    value = (( value << 16 ) | value ) >> count;
    if ( value & 0x8000 ) ST |= TMS_CARRY;
    SetFlags_LAE (( USHORT ) value );

    WriteMemoryW ( WP + 2 * reg, ( USHORT ) value, 0 );
}

//-----------------------------------------------------------------------------
//   JMP	Format: II	Op-code: 0x1000		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JMP ()
{
    ClockCycleCounter += 2;
    PC += 2 * ( char ) curOpCode;
}

//-----------------------------------------------------------------------------
//   JLT	Format: II	Op-code: 0x1100		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JLT ()
{
    if ( ! ( ST & ( TMS_ARITHMETIC | TMS_EQUAL ))) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JLE	Format: II	Op-code: 0x1200		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JLE ()
{
    if (( ! ( ST & TMS_LOGICAL )) | ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JEQ	Format: II	Op-code: 0x1300		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JEQ ()
{
    if ( ST & TMS_EQUAL ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JHE	Format: II	Op-code: 0x1400		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JHE ()
{
    if ( ST & ( TMS_LOGICAL | TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JGT	Format: II	Op-code: 0x1500		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JGT ()
{
    if ( ST & TMS_ARITHMETIC ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNE	Format: II	Op-code: 0x1600		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNE ()
{
    if ( ! ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNC	Format: II	Op-code: 0x1700		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNC ()
{
    if ( ! ( ST & TMS_CARRY )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JOC	Format: II	Op-code: 0x1800		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JOC ()
{
    if ( ST & TMS_CARRY ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNO	Format: II	Op-code: 0x1900		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNO ()
{
    if ( ! ( ST & TMS_OVERFLOW )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JL		Format: II	Op-code: 0x1A00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JL ()
{
    if ( ! ( ST & ( TMS_LOGICAL | TMS_EQUAL ))) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JH		Format: II	Op-code: 0x1B00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JH ()
{
    if (( ST & TMS_LOGICAL ) && ! ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JOP	Format: II	Op-code: 0x1C00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JOP ()
{
    if ( ST & TMS_PARITY ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   SBO	Format: II	Op-code: 0x1D00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SBO ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    WriteCRU ( CRU_Object, cru, 1, 1 );
}

//-----------------------------------------------------------------------------
//   SBZ	Format: II	Op-code: 0x1E00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SBZ ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    WriteCRU ( CRU_Object, cru, 1, 0 );
}

//-----------------------------------------------------------------------------
//   TB		Format: II	Op-code: 0x1F00		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_TB ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    if ( ReadCRU ( CRU_Object, cru, 1 ) & 1 ) ST &= ~ TMS_EQUAL;
    else ST |= TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   COC	Format: III	Op-code: 0x2000		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_COC ()
{
    USHORT src = ReadMemoryW ( WP + 2 * (( curOpCode >> 6 ) & 0x000F ));
    USHORT dst = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    if (( src & dst ) == dst ) ST |= TMS_EQUAL;
    else ST &= ~ TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   CZC	Format: III	Op-code: 0x2400		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_CZC ()
{
    USHORT src = ReadMemoryW ( WP + 2 * (( curOpCode >> 6 ) & 0x000F ));
    USHORT dst = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    if (( ~ src & dst ) == dst ) ST |= TMS_EQUAL;
    else ST &= ~ TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   XOR	Format: III	Op-code: 0x2800		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_XOR ()
{
    int reg = ( curOpCode >> 6 ) & 0x000F;
    USHORT address = GetAddress ( curOpCode, 2 );
    USHORT value = ReadMemoryW ( WP + 2 * reg );
    value ^= ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   XOP	Format: IX	Op-code: 0x2C00		Status: - - - - - - X
//-----------------------------------------------------------------------------
void opcode_XOP ()
{
    USHORT address = GetAddress ( curOpCode, 2 );
    int level = (( curOpCode >> 4 ) & 0x003C ) + 64;
    USHORT newWP = ReadMemoryW ( level );
    USHORT newPC = ReadMemoryW ( level + 2 );
    ContextSwitch ( newWP, newPC );
    WriteMemoryW ( WP + 2 * 11, address );
    ST |= TMS_XOP;
}

//-----------------------------------------------------------------------------
//   LDCR	Format: IV	Op-code: 0x3000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_LDCR ()
{
    USHORT value;
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) & 0x0FFF;
    int count = ( curOpCode >> 6 ) & 0x000F;
    if ( count == 0 ) count = 16;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW | TMS_PARITY );

    ClockCycleCounter += 2 * count;
    if ( count < 9 ) {
        USHORT address = GetAddress ( curOpCode, 1 );
        value = ReadMemoryB ( address );
        ST |= parity [ ( UCHAR ) value ];
        SetFlags_LAE (( char ) value );
    } else {
        USHORT address = GetAddress ( curOpCode, 2 );
        value = ReadMemoryW ( address );
        SetFlags_LAE ( value );
    }

    WriteCRU ( CRU_Object, cru, count, value );
}

//-----------------------------------------------------------------------------
//   STCR	Format: IV	Op-code: 0x3400		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_STCR ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) & 0x0FFF;
    int count = ( curOpCode >> 6 ) & 0x000F;
    if ( count == 0 ) count = 16;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW | TMS_PARITY );

    ClockCycleCounter += 2 * count;
    USHORT value = ReadCRU ( CRU_Object, cru, count );
    if ( count < 9 ) {
        ST |= parity [ ( UCHAR ) value ];
        SetFlags_LAE (( char ) value );
        USHORT address = GetAddress ( curOpCode, 1 );
        WriteMemoryB ( address, ( UCHAR ) value );
    } else {
        ClockCycleCounter += 58 - 42;
        SetFlags_LAE ( value );
        USHORT address = GetAddress ( curOpCode, 2 );
        WriteMemoryW ( address, value );
    }
}

//-----------------------------------------------------------------------------
//   MPY	Format: IX	Op-code: 0x3800		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_MPY ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress (( curOpCode >> 6 ) & 0x0F, 2 );
    ULONG  dst = ReadMemoryW ( dstAddress );

    dst *= src;

    WriteMemoryW ( dstAddress, ( USHORT ) ( dst >> 16 ));
    WriteMemoryW ( dstAddress + 2, ( USHORT ) dst );
}

//-----------------------------------------------------------------------------
//   DIV	Format: IX	Op-code: 0x3C00		Status: - - - - O - -
//-----------------------------------------------------------------------------
void opcode_DIV ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress (( curOpCode >> 6 ) & 0x0F, 2 );
    ULONG  dst = ReadMemoryW ( dstAddress );

    if ( dst < src ) {
        ST &= ~ TMS_OVERFLOW;
        dst = ( dst << 16 ) | ReadMemoryW ( dstAddress + 2 );
        ClockCycleCounter += ( 92 + 124 ) / 2 - 16;
        WriteMemoryW ( dstAddress, ( USHORT ) ( dst / src ));
        WriteMemoryW ( dstAddress + 2, ( USHORT ) ( dst % src ));
    } else {
        ST |= TMS_OVERFLOW;
    }
}

//-----------------------------------------------------------------------------
//   SZC	Format: I	Op-code: 0x4000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_SZC ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    USHORT src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 2 );
    USHORT dst = ReadMemoryW ( dstAddress );

    src = ~ src & dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   SZCB	Format: I	Op-code: 0x5000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_SZCB ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 1 );
    UCHAR  src = ReadMemoryB ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UCHAR  dst = ReadMemoryB ( dstAddress );

    src = ~ src & dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   S		Format: I	Op-code: 0x6000		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_S ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 2 );
    ULONG  dst = ReadMemoryW ( dstAddress );

    ULONG sum = dst - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( USHORT ) src, ( USHORT ) dst, sum );

    WriteMemoryW ( dstAddress, ( USHORT ) sum );
}

//-----------------------------------------------------------------------------
//   SB		Format: I	Op-code: 0x7000		Status: L A E C O P -
//-----------------------------------------------------------------------------
void opcode_SB ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 1 );
    ULONG  src = ReadMemoryB ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 1 );
    ULONG  dst = ReadMemoryB ( dstAddress );

    ULONG sum = dst - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW | TMS_PARITY );
    SetFlags_difB (( UCHAR ) src, ( UCHAR ) dst, sum );

    WriteMemoryB ( dstAddress, ( UCHAR ) sum );
}

//-----------------------------------------------------------------------------
//   C		Format: I	Op-code: 0x8000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_C ()
{
    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );

    USHORT src = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    USHORT dst = ReadMemoryW ( GetAddress ( curOpCode >> 6 , 2 ));

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src, dst );
}

//-----------------------------------------------------------------------------
//   CB		Format: I	Op-code: 0x9000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_CB ()
{
    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );

    UCHAR src = ReadMemoryB ( GetAddress ( curOpCode, 1 ));
    UCHAR dst = ReadMemoryB ( GetAddress ( curOpCode >> 6 , 1 ));

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src, ( char ) dst );
}

//-----------------------------------------------------------------------------
//   A		Format: I	Op-code: 0xA000		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_A ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    ULONG  src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 2 );
    ULONG  dst = ReadMemoryW ( dstAddress );

    ULONG sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( USHORT ) src, ( USHORT ) dst, sum );

    WriteMemoryW ( dstAddress, ( USHORT ) sum );
}

//-----------------------------------------------------------------------------
//   AB		Format: I	Op-code: 0xB000		Status: L A E C O P -
//-----------------------------------------------------------------------------
void opcode_AB ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 1 );
    ULONG  src = ReadMemoryB ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 1 );
    ULONG  dst = ReadMemoryB ( dstAddress );

    ULONG sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW | TMS_PARITY );
    ST |= parity [ ( UCHAR ) sum ];
    SetFlags_sumB (( UCHAR ) src, ( UCHAR ) dst, sum );

    WriteMemoryB ( dstAddress, ( UCHAR ) sum );
}

//-----------------------------------------------------------------------------
//   MOV	Format: I	Op-code: 0xC000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_MOV ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    USHORT src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 2 );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src, 4 );
}

//-----------------------------------------------------------------------------
//   MOVB	Format: I	Op-code: 0xD000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_MOVB ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 1 );
    UCHAR  src = ReadMemoryB ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 1 );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src, 4 );
}

//-----------------------------------------------------------------------------
//   SOC	Format: I	Op-code: 0xE000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_SOC ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 2 );
    USHORT src = ReadMemoryW ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 2 );
    USHORT dst = ReadMemoryW ( dstAddress );

    src = src | dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   SOCB	Format: I	Op-code: 0xF000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_SOCB ()
{
    USHORT srcAddress = GetAddress ( curOpCode, 1 );
    UCHAR  src = ReadMemoryB ( srcAddress );
    USHORT dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UCHAR  dst = ReadMemoryB ( dstAddress );

    src = src | dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src );
}
