//----------------------------------------------------------------------------
//
// File:        ti994a-sdl.cpp
// Date:        18-Apr-2000
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2000-2003 Marc Rousseau, All Rights Reserved.
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

#include <iostream>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "SDL/SDL.h"
#include "SDL/SDL_thread.h"
#include "common.hpp"
#include "logger.hpp"
#include "bitmap.hpp"
#include "compress.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "tms9918a-sdl.hpp"
#include "tms9919.hpp"
#include "ti994a.hpp"
#include "ti994a-sdl.hpp"
#include "support.hpp"
#include "device.hpp"
#include "tms9901.hpp"

DBG_REGISTER ( __FILE__ );

const char SAVE_IMAGE [] = "ti-994a.img";

extern int verbose;

static cSdlTI994A *pThis;

extern "C" UCHAR  CpuMemory [ 0x10000 ];

cSdlTI994A::cSdlTI994A ( cCartridge *ctg, cTMS9918A *vdp, cTMS9919 *sound, cTMS5220 *speech ) :
    cTI994A ( ctg, vdp, sound, speech ),
    m_StartTime ( 0 ),
    m_StopTime ( 0 ),
    m_LastClock ( 0 ),
    m_StartClock ( 0 ),
    m_pThread ( NULL ),
    m_SleepSem ( NULL ),
    m_WaitSem ( NULL ),
    m_pGramKracker ( NULL ),
    m_GK_WriteProtect ( WRITE_PROTECT_UNKNOWN ),
    m_GK_Enabled ( false ),
    m_GK_OpSys ( true ),
    m_GK_BASIC ( true ),
    m_GK_LoaderOn ( false )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A ctor", true );

    m_SleepSem = SDL_CreateSemaphore ( 0 );
    m_WaitSem  = SDL_CreateSemaphore ( 0 );

    if (( m_SleepSem == NULL ) || ( m_WaitSem == NULL )) {
        WARNING ( "Unable to create SDL semaphore" );
    }

    m_JoystickMap [0] = -1;
    m_JoystickMap [1] = -1;

    memset ( m_JoystickPosX, 0, sizeof ( m_JoystickPosX ));
    memset ( m_JoystickPosY, 0, sizeof ( m_JoystickPosY  ));

    pThis = this;

    cCartridge *pGK = new cCartridge ( LocateFile ( "Gram Kracker.ctg", "roms" ));

    if ( pGK->IsValid () == true ) {

        if ( verbose >= 1 ) fprintf ( stdout, "Gram Kracker functions enabled\n" );
        m_pGramKracker = pGK;

        // Write protect the GK's RAM
        SetWriteProtect ( WRITE_PROTECT_ENABLED );

        GK_ToggleEnabled ();

    } else {
        if ( verbose >= 1 ) fprintf ( stdout, "Unable to find Gram Kracker cartridge\n" );
        delete pGK;
    }
}

cSdlTI994A::~cSdlTI994A ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A dtor", true );

    // Make sure the Gram Kracker's CPU RAM is updated before we call the destructor
    RemoveCartridge ( m_Cartridge );

    if ( m_pGramKracker != NULL ) {
        // Make sure the RAM banks are marked as battery-backed before we call the destructor
        for ( unsigned i = 0; i < SIZE ( m_pGramKracker->GromMemory ); i++ ) {
            m_pGramKracker->GromMemory [i].Bank [0].Type = MEMORY_BATTERY_BACKED;
        }
        delete m_pGramKracker;
    }

    if ( m_SleepSem != NULL ) {
        SDL_DestroySemaphore ( m_SleepSem );
    }
    if ( m_WaitSem != NULL ) {
        SDL_DestroySemaphore ( m_WaitSem );
    }
}

void cSdlTI994A::Sleep ( int cycles, ULONG timeout )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::Sleep", false );

    m_CPU->AddClocks ( cycles );

    // Don't do anything fancy if we don't have both semaphores
    if (( m_SleepSem == NULL ) || ( m_WaitSem == NULL )) {
        SDL_Delay ( timeout );
        return;
    }

    // Signal to any waiting threads that we've run
    if ( SDL_SemValue ( m_WaitSem ) <= 0 ) {
        SDL_SemPost ( m_WaitSem );
    }

    // Put the CPU to sleep a bit, but wakeup up as soon as someone else starts to wait
    if ( SDL_SemWaitTimeout ( m_SleepSem, timeout ) != SDL_MUTEX_TIMEDOUT ) {
        TRACE ( "CPU woken up early" );
    }
}

void cSdlTI994A::WakeCPU ( ULONG timeout )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::WakeCPU", true );

    // Don't do anything fancy if we don't have both semaphores
    if (( m_SleepSem == NULL ) || ( m_WaitSem == NULL )) {
        SDL_Delay ( timeout );
        return;
    }

    // Tell the CPU to wake up if it's sleeping
    if ( SDL_SemValue ( m_SleepSem ) <= 0 ) {
        SDL_SemPost ( m_SleepSem );
    }

    // Wait for the CPU to start running
    if ( SDL_SemWaitTimeout ( m_WaitSem, timeout ) != SDL_MUTEX_TIMEDOUT ) {
        TRACE ( "CPU has run - woke up early" );
    }
}

int cSdlTI994A::FindJoystick ( int index )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::FindJoystick", true );

    if ( index == m_JoystickMap [0] ) return 0;
    if ( index == m_JoystickMap [1] ) return 1;

    WARNING ( "Joystick " << index << " is not mapped" );

    return -1;
}

void cSdlTI994A::KeyPressed ( SDL_keysym keysym )
{
# if 0 //LUDO:
    FUNCTION_ENTRY ( this, "cSdlTI994A::KeyPressed", false );

    cTMS9901 *pic = m_PIC;

    USHORT ch = ( USHORT ) (( keysym.unicode & 0xFF80 ) ? 0 : keysym.unicode & 0x7F );

    if ( isalpha ( ch )) {
        pic->VKeysDown ( keysym.sym, ( VIRTUAL_KEY_E ) ( VK_A + ( tolower ( ch ) - 'a' )));
    } else if ( isdigit ( ch )) {
        pic->VKeysDown ( keysym.sym, ( VIRTUAL_KEY_E ) ( VK_0 + ( ch - '0' )));
    } else {
        switch ( ch ) {
            case '\'' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_O );         break;
            case  ',' : pic->VKeyUp ( SDLK_LSHIFT ); pic->VKeyUp ( SDLK_RSHIFT);
                        pic->VKeysDown ( keysym.sym, VK_COMMA );               break;
            case  '<' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_COMMA );     break;
            case  '.' : pic->VKeyUp ( SDLK_LSHIFT ); pic->VKeyUp ( SDLK_RSHIFT);
                        pic->VKeysDown ( keysym.sym, VK_PERIOD );              break;
            case  '>' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_PERIOD );    break;
            case  ';' : pic->VKeyUp ( SDLK_LSHIFT ); pic->VKeyUp ( SDLK_RSHIFT);
                        pic->VKeysDown ( keysym.sym, VK_SEMICOLON );           break;
            case  ':' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_SEMICOLON ); break;
            case  '_' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_U );         break;
            case  '|' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_A );         break;
            case  '=' : pic->VKeyUp ( SDLK_LSHIFT ); pic->VKeyUp ( SDLK_RSHIFT);
                        pic->VKeysDown ( keysym.sym, VK_EQUALS );              break;
            case  '+' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_EQUALS );    break;
            case  '~' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_W );         break;
            case '\"' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_P );         break;
            case  '?' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_I );         break;
            case  '/' : pic->VKeyUp ( SDLK_LSHIFT ); pic->VKeyUp ( SDLK_RSHIFT);
                        pic->VKeysDown ( keysym.sym, VK_DIVIDE );              break;
            case  '-' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_DIVIDE );    break;
            case  '[' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_R );         break;
            case  ']' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_T );         break;
            case  '{' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_F );         break;
            case  '}' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_G );         break;
            case  ' ' : pic->VKeysDown ( keysym.sym, VK_SPACE );               break;
            case  '!' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_1 );         break;
            case  '@' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_2 );         break;
            case  '#' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_3 );         break;
            case  '$' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_4 );         break;
            case  '%' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_5 );         break;
            case  '^' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_6 );         break;
            case  '&' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_7 );         break;
            case  '*' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_8 );         break;
            case  '(' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_9 );         break;
            case  ')' : pic->VKeysDown ( keysym.sym, VK_SHIFT, VK_0 );         break;
            case '\\' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_Z );         break;
            case  '`' : pic->VKeysDown ( keysym.sym, VK_FCTN,  VK_C );         break;

            default:
                switch ( keysym.sym ) {
                    case SDLK_TAB       : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_7 ); break;
                    case SDLK_BACKSPACE :
                    case SDLK_LEFT      : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_S ); break;
                    case SDLK_RIGHT     : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_D ); break;
                    case SDLK_UP        : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_E ); break;
                    case SDLK_DOWN      : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_X ); break;
                    case SDLK_DELETE    : pic->VKeysDown ( keysym.sym, VK_FCTN, VK_1 ); break;
                    case SDLK_RETURN    : pic->VKeysDown ( keysym.sym, VK_ENTER );      break;
                    case SDLK_LSHIFT    :
                    case SDLK_RSHIFT    : pic->VKeysDown ( keysym.sym, VK_SHIFT );      break;
                    case SDLK_LALT      :
                    case SDLK_RALT      : pic->VKeysDown ( keysym.sym, VK_FCTN );       break;
                    case SDLK_LMETA     :
                    case SDLK_RMETA     : pic->VKeysDown ( keysym.sym, VK_FCTN );       break;
                    case SDLK_LCTRL     :
                    case SDLK_RCTRL     : pic->VKeysDown ( keysym.sym, VK_CTRL );       break;
                    case SDLK_CAPSLOCK  : pic->VKeysDown ( keysym.sym, VK_CAPSLOCK );   break;
                    default : ;
                }
        }
    }

    // Emulate the joystick
    switch ( keysym.sym ) {
        case SDLK_KP0   : pic->SetJoystickButton ( 0, true ); break;
        case SDLK_KP7   : pic->SetJoystickY ( 0,  1 );
        case SDLK_LEFT  :
        case SDLK_KP4   : pic->SetJoystickX ( 0, -1 ); break;
        case SDLK_KP3   : pic->SetJoystickY ( 0, -1 );
        case SDLK_RIGHT :
        case SDLK_KP6   : pic->SetJoystickX ( 0,  1 ); break;
        case SDLK_KP1   : pic->SetJoystickX ( 0, -1 );
        case SDLK_DOWN  :
        case SDLK_KP2   : pic->SetJoystickY ( 0, -1 ); break;
        case SDLK_KP9   : pic->SetJoystickX ( 0,  1 );
        case SDLK_UP    :
        case SDLK_KP8   : pic->SetJoystickY ( 0,  1 ); break;
        default : ;
    }
# endif
}

void cSdlTI994A::KeyReleased ( SDL_keysym keysym )
{
# if 0 //LUDO:
    Uint8 *keystate = SDL_GetKeyState ( NULL );

    cTMS9901 *pic = m_PIC;

    pic->VKeyUp ( keysym.sym );

    if (( pic->GetKeyState ( VK_COMMA )     == 0 ) &&
        ( pic->GetKeyState ( VK_PERIOD )    == 0 ) &&
        ( pic->GetKeyState ( VK_SEMICOLON ) == 0 ) &&
        ( pic->GetKeyState ( VK_EQUALS )    == 0 ) &&
        ( pic->GetKeyState ( VK_DIVIDE )    == 0 ) &&
        ( pic->GetKeyState ( VK_SHIFT )     == 0 )) {
        if ( keystate [ SDLK_LSHIFT ]) {
            pic->VKeysDown ( SDLK_LSHIFT, VK_SHIFT );
        }
        if ( keystate [ SDLK_RSHIFT ]) {
            pic->VKeysDown ( SDLK_RSHIFT, VK_SHIFT );
        }
    }

    switch ( keysym.sym ) {
        case SDLK_KP0   : pic->SetJoystickButton ( 0, false ); break;
        case SDLK_KP7   :
        case SDLK_KP1   :
        case SDLK_KP9   :
        case SDLK_KP3   : pic->SetJoystickY ( 0, 0 );
        case SDLK_LEFT  :
        case SDLK_KP4   :
        case SDLK_RIGHT :
        case SDLK_KP6   : pic->SetJoystickX ( 0, 0 ); break;
        case SDLK_DOWN  :
        case SDLK_KP2   :
        case SDLK_UP    :
        case SDLK_KP8   : pic->SetJoystickY ( 0, 0 ); break;
        default : ;
    }
# endif
}

void cSdlTI994A::StartThread ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::StartThread", true );

    if ( m_CPU->IsRunning () == true ) return;

    m_StartTime = SDL_GetTicks () - m_StopTime;
//    m_StartTime = SDL_GetTicks ();

    m_pThread = SDL_CreateThread ( _RunThreadProc, this );
}

void cSdlTI994A::StopThread ()
{
# if 0
    FUNCTION_ENTRY ( this, "cSdlTI994A::StopThread", true );

    if ( m_CPU->IsRunning () == false ) return;
# endif
    m_CPU->Stop ();
# if 0
    m_StopTime = SDL_GetTicks ();

    SDL_WaitThread ( m_pThread, NULL );
# endif
}

void cSdlTI994A::Reset ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::Reset", true );

    bool isRunning = m_CPU->IsRunning ();

    if ( isRunning ) StopThread ();

    cTI994A::Reset ();

    if ( isRunning ) StartThread ();
}

void cSdlTI994A::SetWriteProtect ( WRITE_PROTECT_E protect )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::SetWriteProtect", true );

    if ( m_pGramKracker == NULL ) return;

    if ( m_GK_WriteProtect != protect ) {
        if ( m_CpuMemoryInfo [6] == &m_pGramKracker->CpuMemory [6] ) {
            if (( m_GK_WriteProtect == WRITE_PROTECT_BANK1 ) || ( m_GK_WriteProtect == WRITE_PROTECT_BANK2 )) {
                // Save the contents of battery-backed RAM
                memcpy ( m_CpuMemoryInfo [6]->CurBank->Data, &CpuMemory [ 0x6000 ], ROM_BANK_SIZE );
                memcpy ( m_CpuMemoryInfo [7]->CurBank->Data, &CpuMemory [ 0x7000 ], ROM_BANK_SIZE );
            }
        }
    }

    m_GK_WriteProtect = protect;

    switch ( m_GK_WriteProtect ) {
        case WRITE_PROTECT_BANK1 :
            TRACE ( "BANK 1 Selected" );
            m_pGramKracker->CpuMemory [6].CurBank = &m_pGramKracker->CpuMemory [6].Bank [0];
            m_pGramKracker->CpuMemory [7].CurBank = &m_pGramKracker->CpuMemory [7].Bank [0];
            break;
        case WRITE_PROTECT_BANK2 :
            TRACE ( "BANK 2 Selected" );
            m_pGramKracker->CpuMemory [6].CurBank = &m_pGramKracker->CpuMemory [6].Bank [1];
            m_pGramKracker->CpuMemory [7].CurBank = &m_pGramKracker->CpuMemory [7].Bank [1];
            break;
        case WRITE_PROTECT_ENABLED :
            TRACE ( "Write-Protect Enabled" );
        default:
            break;
    }

    MEMORY_TYPE_E memoryType = ( m_GK_WriteProtect == WRITE_PROTECT_ENABLED ) ? MEMORY_ROM : MEMORY_BATTERY_BACKED;

    // Mark the GROM banks as RAM/ROM - no need to mark CPU RAM (it's handled differently)
    for ( unsigned i = 0; i < SIZE ( m_pGramKracker->GromMemory ); i++ ) {
        m_pGramKracker->GromMemory [i].Bank [0].Type = memoryType;
    }

    // If Gram Kracker memory is not currently active we're done here
    if ( m_CpuMemoryInfo [6] != &m_pGramKracker->CpuMemory [6] ) return;

    UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );

    if ( m_GK_WriteProtect == WRITE_PROTECT_ENABLED ) {
        USHORT address = 0x6000;
        for ( unsigned j = 0; j < 2 * ROM_BANK_SIZE; j++ ) {
            m_CPU->SetBreakpoint ( address++, MEMFLG_WRITE, true, bkp );
        }
        m_CPU->SetMemory ( MEM_ROM, 0x6000, 2 * ROM_BANK_SIZE );
    } else {
        m_CPU->ClearBreakpoint ( bkp );
        m_CPU->SetMemory ( MEM_RAM, 0x6000, 2 * ROM_BANK_SIZE );
    }

    // Load the contents of battery-backed RAM
    memcpy ( &CpuMemory [ 0x6000 ], m_CpuMemoryInfo [6]->CurBank->Data, ROM_BANK_SIZE );
    memcpy ( &CpuMemory [ 0x7000 ], m_CpuMemoryInfo [7]->CurBank->Data, ROM_BANK_SIZE );
}

void cSdlTI994A::GK_ToggleEnabled ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::GK_ToggleEnabled", true );

    if ( m_pGramKracker == NULL ) return;

    TRACE ( "Turning Gram Kracker " << (( m_GK_Enabled == true ) ? "Off" : "On" ));

    m_GK_Enabled = ( m_GK_Enabled == true ) ? false : true;

    if ( m_Cartridge != NULL ) return;

    if ( m_GK_Enabled == true ) {
        for ( unsigned i = 3; i < SIZE ( m_GromMemoryInfo ); i++ ) {
            m_GromMemoryInfo [i] = &m_pGramKracker->GromMemory [i];
            memcpy ( &m_GromMemory [ i << 13 ], m_GromMemoryInfo [i]->CurBank->Data, GROM_BANK_SIZE );
        }
        m_CpuMemoryInfo [6] = &m_pGramKracker->CpuMemory [6];
        m_CpuMemoryInfo [7] = &m_pGramKracker->CpuMemory [7];

        // Use SetWriteProtect to fixup the Bank switch interrupt correctly
        SetWriteProtect ( m_GK_WriteProtect );
    } else {
        // Save the contents of battery-backed RAM
        if ( m_GK_WriteProtect != WRITE_PROTECT_ENABLED ) {
            memcpy ( m_CpuMemoryInfo [6]->CurBank->Data, &CpuMemory [ 0x6000 ], ROM_BANK_SIZE );
            memcpy ( m_CpuMemoryInfo [7]->CurBank->Data, &CpuMemory [ 0x7000 ], ROM_BANK_SIZE );
        }

        for ( unsigned i = 3; i < SIZE ( m_GromMemoryInfo ); i++ ) {
            m_GromMemoryInfo [i] = NULL;
            memset ( &m_GromMemory [ i << 13 ], 0, GROM_BANK_SIZE );
        }
        m_CpuMemoryInfo [6] = NULL;
        m_CpuMemoryInfo [7] = NULL;

        memset (( USHORT * ) &CpuMemory [ 0x6000 ], 0, 2 * ROM_BANK_SIZE );

        // Clear the bankswitch breakpoint for ALL regions!
        UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );
        m_CPU->ClearBreakpoint ( bkp );
    }
}

void cSdlTI994A::GK_ToggleOpSys ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::GK_ToggleOpSys", true );

    if ( m_pGramKracker == NULL ) return;

    TRACE ( "Enabling " << (( m_GK_OpSys == true ) ? "GROM 0" : "Operating System" ));

    if ( m_GK_OpSys == true ) {
        m_GK_OpSys = false;
        m_GromMemoryInfo [0] = &m_pGramKracker->GromMemory [0];
    } else {
        m_GK_OpSys = true;
        m_GromMemoryInfo [0] = &m_Console->GromMemory [0];
    }

    memcpy ( &m_GromMemory [ 0x0000 ], m_GromMemoryInfo [0]->CurBank->Data, GROM_BANK_SIZE );
}

void cSdlTI994A::GK_ToggleBASIC ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::GK_ToggleBASIC", true );

    if ( m_pGramKracker == NULL ) return;

    TRACE ( "Enabling " << (( m_GK_BASIC == true ) ? "GROMS 1&2" : "TI BASIC" ));

    m_GK_BASIC = ( m_GK_BASIC == true ) ? false : true;

    // Don't do anything else if the Loader is enabled
    if ( m_GK_LoaderOn == true ) return;

    if ( m_GK_BASIC == true ) {
        m_GromMemoryInfo [1] = &m_Console->GromMemory [1];
        m_GromMemoryInfo [2] = &m_Console->GromMemory [2];
    } else {
        m_GromMemoryInfo [1] = &m_pGramKracker->GromMemory [1];
        m_GromMemoryInfo [2] = &m_pGramKracker->GromMemory [2];
    }

    memcpy ( &m_GromMemory [ 0x2000 ], m_GromMemoryInfo [1]->CurBank->Data, GROM_BANK_SIZE );
    memcpy ( &m_GromMemory [ 0x4000 ], m_GromMemoryInfo [2]->CurBank->Data, GROM_BANK_SIZE );
}

void cSdlTI994A::GK_ToggleLoader ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::GK_ToggleLoader", true );

    if ( m_pGramKracker == NULL ) return;

    TRACE ( "Turning Gram Kracker Loader " << (( m_GK_LoaderOn == true ) ? "Off" : "On" ));

    m_GK_LoaderOn = ( m_GK_LoaderOn == true ) ? false : true;

    if ( m_GK_LoaderOn == true ) {
        // Switch CurBank to point to the GK Loader
        m_pGramKracker->GromMemory [1].CurBank = &m_pGramKracker->GromMemory [1].Bank [1];
        m_pGramKracker->GromMemory [2].CurBank = &m_pGramKracker->GromMemory [2].Bank [1];
        m_GromMemoryInfo [1] = &m_pGramKracker->GromMemory [1];
        m_GromMemoryInfo [2] = &m_pGramKracker->GromMemory [2];
    } else {
        m_pGramKracker->GromMemory [1].CurBank = &m_pGramKracker->GromMemory [1].Bank [0];
        m_pGramKracker->GromMemory [2].CurBank = &m_pGramKracker->GromMemory [2].Bank [0];
        if ( m_GK_BASIC == true ) {
            m_GromMemoryInfo [1] = &m_Console->GromMemory [1];
            m_GromMemoryInfo [2] = &m_Console->GromMemory [2];
        } else {
            m_GromMemoryInfo [1] = &m_pGramKracker->GromMemory [1];
            m_GromMemoryInfo [2] = &m_pGramKracker->GromMemory [2];
        }
    }

    memcpy ( &m_GromMemory [ 0x2000 ], m_GromMemoryInfo [1]->CurBank->Data, GROM_BANK_SIZE );
    memcpy ( &m_GromMemory [ 0x4000 ], m_GromMemoryInfo [2]->CurBank->Data, GROM_BANK_SIZE );
}

int cSdlTI994A::_TimerHookProc ()
{
    FUNCTION_ENTRY ( NULL, "cSdlTI994A::_TimerHookProc", false );

    return pThis->TimerHookProc ();
}

extern "C" void psp_update_keys(void);

int cSdlTI994A::TimerHookProc ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::TimerHookProc", false );

    ULONG clockCycles    = m_CPU->GetClocks ();

    ULONG ellapsedCycles = clockCycles - m_StartClock;
    ULONG ellapsedTime   = SDL_GetTicks () - m_StartTime;

    psp_update_keys();

    if ( ellapsedTime > 0 ) {
        // Limit the emulated speed to 3.0MHz
        if ( ellapsedCycles / ellapsedTime > 3.0 * 1000.0 ) {
            Sleep ( 0, 1 );
        }

        // Reset base time/clocks every 5 minutes to avoid wrap
        if ( ellapsedTime > 5 * 60 * 1000.0 ) {
            m_StartClock = clockCycles;
            m_StartTime  = SDL_GetTicks ();
        }
    }

    // Simulate a 50/60Hz VDP interrupt
    if ( clockCycles - m_LastClock > m_RefreshInterval ) {
        m_LastClock = clockCycles;
        m_VDP->Retrace ();
    }

    return 0;
}

int cSdlTI994A::_RunThreadProc ( void *ptr )
{
    FUNCTION_ENTRY ( NULL, "cSdlTI994A::_RunThreadProc", true );

    cSdlTI994A *pThis = ( cSdlTI994A * ) ptr;

    return pThis->RunThreadProc ();
}

extern "C" ULONG TimerHook;

int cSdlTI994A::RunThreadProc ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::RunThreadProc", true );

    TimerHook = ( ULONG ) _TimerHookProc;

    m_CPU->Run ();

    return 0;
}

void cSdlTI994A::InsertCartridge ( cCartridge *pCartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::InsertCartridge", true );

    // Save the contents of battery-backed RAM
    if (( m_GK_WriteProtect != WRITE_PROTECT_ENABLED ) && ( m_CpuMemoryInfo [6] == &m_pGramKracker->CpuMemory [6] )) {
        memcpy ( m_CpuMemoryInfo [6]->CurBank->Data, &CpuMemory [ 0x6000 ], ROM_BANK_SIZE );
        memcpy ( m_CpuMemoryInfo [7]->CurBank->Data, &CpuMemory [ 0x7000 ], ROM_BANK_SIZE );
    }

    // Clear the bank swap trap if the catridge has CPU ROM/RAM
    if (( pCartridge != NULL ) &&
        (( pCartridge->CpuMemory [6].NumBanks > 0 ) ||
         ( pCartridge->CpuMemory [7].NumBanks > 0 ))) {
        UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );
        m_CPU->ClearBreakpoint ( bkp );
    }

    cTI994A::InsertCartridge ( pCartridge, reset );

    // Override any console banks here
}

void cSdlTI994A::RemoveCartridge ( cCartridge *pCartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::RemoveCartridge", true );

    // Save any Gram Kracker CPU RAM here

    cTI994A::RemoveCartridge ( pCartridge, reset );

    // Swap in Gram Kracker ROM/GROM banks here

    // Set the bank swap trap if GK Write Protect is active
    if (( pCartridge != NULL ) && ( m_GK_WriteProtect == WRITE_PROTECT_ENABLED ) &&
        (( pCartridge->CpuMemory [6].NumBanks > 0 ) ||
         ( pCartridge->CpuMemory [7].NumBanks > 0 ))) {
        UCHAR bkp = m_CPU->GetBreakpoint ( TrapFunction, TRAP_BANK_SWITCH );
        USHORT address = 0x6000;
        for ( unsigned j = 0; j < 2 * ROM_BANK_SIZE; j++ ) {
            m_CPU->SetBreakpoint ( address++, MEMFLG_WRITE, true, bkp );
        }
        m_CPU->SetMemory ( MEM_ROM, 0x6000, 2 * ROM_BANK_SIZE );
    }
}

void cSdlTI994A::Run ()
{
    FUNCTION_ENTRY ( this, "cSdlTI994A::Run", true );

    //StartThread ();
    //int joystick;

    //cTMS9901 *pic = m_PIC;
    //LUDO: cSdlTMS9918A *vdp = dynamic_cast < cSdlTMS9918A * > ( m_VDP );
    /* cSdlTMS9918A *vdp = */ (cSdlTMS9918A *)( m_VDP );

    // Loop waiting for SDL_QUIT
# if 0 //LUDO: TO_BE_DONE 
    Uint8 *keystate = SDL_GetKeyState ( NULL );
    SDL_Event event;
    while ( SDL_WaitEvent ( &event ) >= 0 ) {
        switch ( event.type ) {
            case SDL_VIDEORESIZE :
                vdp->ResizeWindow ( event.resize.w, event.resize.h );
                break;
            case SDL_JOYAXISMOTION :
                joystick = FindJoystick ( event.jaxis.which );
                if ( joystick != -1 ) {
                    int state = 0;
                    int mag = ( event.jaxis.value < 0 ) ? -event.jaxis.value : event.jaxis.value;
                    if ( event.jaxis.value < -8192 ) state = -1;
                    if ( event.jaxis.value >  8192 ) state = 1;
                    switch ( event.jaxis.axis ) {
                        case 0 :
                            if (( state == 0 ) || ( 2 * mag > m_JoystickPosY [joystick] )) {
                                pic->SetJoystickX ( joystick, state );
                            }
                            m_JoystickPosX [joystick] = mag;
                            break;
                        case 1 :
                            if (( state == 0 ) || ( 2 * mag > m_JoystickPosX [joystick] )) {
                                pic->SetJoystickY ( joystick, -state );
                            }
                            m_JoystickPosY [joystick] = mag;
                            break;
                    }
                }
                break;
            case SDL_JOYBUTTONDOWN :
                joystick = FindJoystick ( event.jaxis.which );
                if ( joystick != -1 ) {
                    pic->SetJoystickButton ( joystick, true );
                }
                break;
            case SDL_JOYBUTTONUP :
                joystick = FindJoystick ( event.jaxis.which );
                if ( joystick != -1 ) {
                    pic->SetJoystickButton ( joystick, false );
                }
                break;
            case SDL_KEYDOWN :
                if ( event.key.keysym.sym == SDLK_ESCAPE ) goto done;
                if (( keystate [ SDLK_LCTRL ] != 0 ) || ( keystate [ SDLK_RCTRL ] != 0 )) {
                    switch ( event.key.keysym.sym ) {
                        case SDLK_F1 :
                            GK_ToggleEnabled ();
                            break;
                        case SDLK_F2 :
                            GK_ToggleOpSys ();
                            break;
                        case SDLK_F3 :
                            GK_ToggleBASIC ();
                            break;
                        case SDLK_F4 :
                            SetWriteProtect ( WRITE_PROTECT_BANK1 );
                            break;
                        case SDLK_F5 :
                            SetWriteProtect ( WRITE_PROTECT_ENABLED );
                            break;
                        case SDLK_F6 :
                            SetWriteProtect ( WRITE_PROTECT_BANK2 );
                            break;
                        case SDLK_F7 :
                            GK_ToggleLoader ();
                            break;
                        default :
                            KeyPressed ( event.key.keysym );
                            break;
                    }
                } else {
                    switch ( event.key.keysym.sym ) {
                        case SDLK_F2 :
                            SaveImage ( SAVE_IMAGE );
                            break;
                        case SDLK_F3 :
                            LoadImage ( SAVE_IMAGE );
                            break;
                        case SDLK_F10 :
                            Reset ();
                            break;
                        default :
                            KeyPressed ( event.key.keysym );
                            break;
                    }
                }
                break;
            case SDL_KEYUP :
                KeyReleased ( event.key.keysym );
                break;
            case SDL_QUIT:
                goto done;
        }
    }
# else
    TimerHook = ( ULONG ) _TimerHookProc;

    m_CPU->Run ();
# endif

# if 0 //TO_BE_DONE
done:
    StopThread ();
# endif

}

void cSdlTI994A::SaveImage ( const char *filename )
{
# if 0 //LUDO:
    FUNCTION_ENTRY ( this, "cSdlTI994A::SaveImage", true );

    char buffer [256];

    sprintf ( buffer, "%s%c%s", HOME_PATH, FILE_SEPERATOR, filename );

    bool isRunning = m_CPU->IsRunning ();

    if ( isRunning ) StopThread ();
# endif
    cTI994A::SaveImage ( filename );
# if 0
    if ( isRunning ) StartThread ();
# endif
}

bool cSdlTI994A::LoadImage ( const char *filename )
{
# if 0 //LUDO:
    FUNCTION_ENTRY ( this, "cSdlTI994A::LoadImage", true );

    char buffer [256];

    sprintf ( buffer, "%s%c%s", HOME_PATH, FILE_SEPERATOR, filename );
    bool isRunning = m_CPU->IsRunning ();
    if ( isRunning ) StopThread ();
# endif
    StopThread();
    bool retVal = cTI994A::LoadImage ( filename );
# if 0 //LUDO:
    if ( isRunning ) StartThread ();
# endif
    return retVal;
}

void cSdlTI994A::SetJoystick ( int index, SDL_Joystick *joystick )
{
    m_JoystickMap [index] = SDL_JoystickIndex ( joystick );
}

extern "C" {
  void 
  ti99_HandleKeyPress(int sym_id, int key1_id, int key2_id, int press) 
  {
    extern cTMS9901 *pic;

    if (press) {
      if (key1_id && key2_id) {
        pic->VKeysDown( sym_id, (VIRTUAL_KEY_E)key1_id, (VIRTUAL_KEY_E)key2_id );
      } else {
        pic->VKeyDown( sym_id, (VIRTUAL_KEY_E)key1_id );
      }

    } else {
      pic->VKeyUp( sym_id );
    }
  }
  void
  ti99_HandleJoystick(int joy_mask) 
  {
    extern cTMS9901 *pic;
    /* 1<< 0 -> UP  
       1<< 1 -> DOWN
       1<< 2 -> LEFT
       1<< 3 -> RIGHT
       1<< 4 -> FIRE */
# define JOY_UP_MASK     (1 << 0)
# define JOY_DOWN_MASK   (1 << 1)
# define JOY_LEFT_MASK   (1 << 2)
# define JOY_RIGHT_MASK  (1 << 3)
# define JOY_FIRE_MASK   (1 << 4)

    if (joy_mask & JOY_FIRE_MASK) {
      pic->SetJoystickButton( 0, true );
    } else {
      pic->SetJoystickButton( 0, false );
    }
    int x_axis = 0;
    int y_axis = 0;
    if (joy_mask & JOY_UP_MASK   ) y_axis =  1;
    if (joy_mask & JOY_DOWN_MASK ) y_axis = -1;
    if (joy_mask & JOY_LEFT_MASK ) x_axis = -1;
    if (joy_mask & JOY_RIGHT_MASK) x_axis =  1;
    pic->SetJoystickX ( 0, x_axis );
    pic->SetJoystickY ( 0, y_axis );
  }

  void
  ti99_reset_keyboard()
  {
    extern cTMS9901 *pic;
    pic->HardwareReset();
  }

  int
  ti99_save_img(const char* filename)
  {
    pThis->SaveImage(filename);
    return 0;
  }

  int
  ti99_load_img(const char* filename)
  {
    return pThis->LoadImage(filename);
  }
}
