//----------------------------------------------------------------------------
//
// File:        screenio.cpp
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

#if defined ( __GNUC__ )
    #include <memory.h>
    #include <signal.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/time.h>
    #include <termios.h>
    #include <unistd.h>
#elif defined ( __WIN32__ )
    #include <conio.h>
    #include <signal.h>
    #include <windows.h>
#elif defined ( __OS2__ )
    #define INCL_SUB
    #define INCL_NOPMAPI
    #include <os2.h>
    #include <conio.h>
#else
    #error "This code must be compiled for either OS/2, Win32, or Linux"
#endif

#include "common.hpp"
#include "screenio.hpp"

static ULONG curX = 0;
static ULONG curY = 0;

#define toHex(x)	( char ) (((x) > 9 ) ? (x) + 'A' - 10 : (x) + '0' )

#if defined ( __OS2__ )

static HVIO hVio = 0;
static VIOCURSORINFO vioco;
static int oldAttr;

void SaveConsoleSettings ()
{
    VioGetCurType ( &vioco, hVio );
    oldAttr = vioco.attr;
    vioco.attr = 0xFFFF;
    VioSetCurType ( &vioco, hVio );
}

void RestoreConsoleSettings ()
{
    vioco.attr = oldAttr;
    VioSetCurType ( &vioco, hVio );
}

void GetXY ( ULONG *x, ULONG *y )
{
    VioGetCurPos ( y, x, hVio );
}

void GotoXY ( ULONG x, ULONG y )
{
    curX = x;
    curY = y;
    VioSetCurPos ( y, x, hVio );
}

ULONG CurrentTime ()
{
    ULONG time;
    DosQuerySysInfo ( QSV_MS_COUNT, QSV_MS_COUNT, &time, 4 );
    return time;
}

void PutXY ( ULONG x, ULONG y, char *ptr, int length )
{
    VioWrtCharStr ( ptr, length, y, x, hVio );
    curX = x + length;
    curY = y;
}

void Put ( char *ptr, int length )
{
    VioWrtCharStr ( ptr, length, curY, curX, hVio );
    curX += length;
}

#elif defined ( __GNUC__ )

static FILE *console;
static termios stored;
static termios term_getch;
static termios term_kbhit;
static int lastChar;
static int keyhit;
static bool cursor_visible = true;

int cprintf ( const char *fmt, ... )
{
    va_list args;
    va_start ( args, fmt );

    int ret = vfprintf ( console, fmt, args );

    va_end ( args );

    return ret;
}

void SignalHandler ( int signal )
{
    struct sigaction sa;
    memset ( &sa, 0, sizeof ( sa ));
    sa.sa_handler = SIG_DFL;
    switch ( signal ) {
        case SIGABRT :
            RestoreConsoleSettings ();
            break;
        case SIGSEGV :
            fprintf ( stderr, "Segmentation fault" );
        case SIGINT :
            RestoreConsoleSettings ();
            exit ( -1 );
            break;
        case SIGTSTP :
            RestoreConsoleSettings ();
            printf ( "\r\n" );
            sigaction ( SIGTSTP, &sa, NULL );
            kill ( getpid (), SIGTSTP );
            break;
        case SIGCONT :
            SaveConsoleSettings ();
            break;
    }
}

void SaveConsoleSettings ()
{
    tcgetattr ( 0, &stored );
    memcpy ( &term_getch, &stored, sizeof ( struct termios ));
    // Disable echo
    term_getch.c_lflag &= ~ECHO;
    // Disable canonical mode, and set buffer size to 1 byte
    term_getch.c_lflag &= ~ICANON;
    term_getch.c_cc[VMIN] = 1;

    memcpy ( &term_kbhit, &term_getch, sizeof ( struct termios ));
    term_kbhit.c_cc[VTIME] = 0;
    term_kbhit.c_cc[VMIN] = 0;

    // Create a 'console' device
    console = fopen ( "/dev/tty", "w" );
    if ( console == NULL ) {
        console = stdout;
    }

    HideCursor ();

    struct sigaction sa;
    memset ( &sa, 0, sizeof ( sa ));
    sa.sa_handler = SignalHandler;
    sigaction ( SIGINT,  &sa, NULL );
    sigaction ( SIGABRT, &sa, NULL );
    sigaction ( SIGSEGV, &sa, NULL );
    sigaction ( SIGTSTP, &sa, NULL );
    sigaction ( SIGCONT, &sa, NULL );

    atexit ( RestoreConsoleSettings );
}

void RestoreConsoleSettings ()
{
    struct sigaction sa;
    memset ( &sa, 0, sizeof ( sa ));
    sa.sa_handler = SIG_DFL;
    sigaction ( SIGINT,  &sa, NULL );
    sigaction ( SIGABRT, &sa, NULL );
    sigaction ( SIGSEGV, &sa, NULL );
    sigaction ( SIGTSTP, &sa, NULL );
    sigaction ( SIGCONT, &sa, NULL );

    tcsetattr ( 0, TCSANOW, &stored );

    ShowCursor ();

    if ( console != stdout ) {
        fclose ( console );
        console = stdout;
    }

}

void HideCursor ()
{
    if ( cursor_visible == true ) {
        printf ( "\033[?25l" );
        fflush ( stdout );
        cursor_visible = false;
    }
}

void ShowCursor ()
{
    if ( cursor_visible == false ) {
        printf ( "\033[?25h" );
        fflush ( stdout );
        cursor_visible = true;
    }
}

int GetKey ()
{
    int key = lastChar;
    lastChar = 0;

    if ( keyhit == 0 ) {
        tcsetattr ( 0, TCSANOW, &term_getch );
        keyhit = read ( STDIN_FILENO, &key, sizeof ( key ));
    }

    UCHAR *ptr = ( UCHAR * ) &key;
    int retVal = ( ptr [3] << 24 ) | ( ptr [2] << 16 ) | ( ptr [1] << 8 ) | ptr [0];

    keyhit = 0;

    return retVal;
}

bool KeyPressed ()
{
    if ( keyhit == 0 ) {
        tcsetattr ( 0, TCSANOW, &term_kbhit );
        keyhit = read ( STDIN_FILENO, &lastChar, sizeof ( lastChar ));
    }

    return ( keyhit != 0 ) ? true : false;
}

ULONG CurrentTime ()
{
    timeval time;
    gettimeofday ( &time, NULL );
    return ( ULONG ) (( time.tv_sec * 1000 ) + ( time.tv_usec / 1000 ));
}

void ClearScreen ()
{
    GotoXY ( 0, 0 );
    printf ( "\033[2J" );
    fflush ( stdout );
}

void GetXY ( ULONG *x, ULONG *y )
{
    *x = 0;
    *y = 0;
}

void GotoXY ( ULONG x, ULONG y )
{
    fprintf ( console, "\033[%d;%dH", y + 1, x + 1 );
//    fprintf ( console, "\033[%dG", x + 1 );
    fflush ( console );
    curX = x + 1;
    curY = y + 1;
}

void PutXY ( ULONG x, ULONG y, char *ptr, int length )
{
    GotoXY ( x, y );
    Put ( ptr, length );
}

void Put ( char *ptr, int length )
{
    static char buffer [80];
    if ( length > 80 ) length = 80;
    strncpy ( buffer, ptr, length );
    for ( int i = 0; i < length; i++ ) {
        if (( buffer [i] < 32 ) || ( buffer [i] > 126 )) {
            buffer [i] = '.';
        }
    }

    printf ( "%*.*s", length, length, buffer );
    fflush ( stdout );
    curX += length;
}

#elif defined ( __WIN32__ )

static CONSOLE_SCREEN_BUFFER_INFO screenInfo;
static CONSOLE_CURSOR_INFO        cursorInfo;
static BOOL                       oldVisible;

static HANDLE                     hOutput;
static COORD                      currentPos;

static LARGE_INTEGER              timerFrequency;

long WINAPI myHandler ( PEXCEPTION_POINTERS )
{
    RestoreConsoleSettings ();

    return EXCEPTION_CONTINUE_SEARCH;
}

void SignalHandler ( int )
{
    RestoreConsoleSettings ();
    cprintf ( "\r\n" );
    exit ( -1 );
}

void SaveConsoleSettings ()
{
    hOutput = GetStdHandle ( STD_OUTPUT_HANDLE );
    GetConsoleCursorInfo ( hOutput, &cursorInfo );
    GetConsoleScreenBufferInfo ( hOutput, &screenInfo );
    oldVisible = cursorInfo.bVisible;
    SetUnhandledExceptionFilter ( myHandler );
    signal ( SIGBREAK, SignalHandler );
    signal ( SIGINT, SignalHandler );
    atexit ( RestoreConsoleSettings );

    QueryPerformanceFrequency ( &timerFrequency );
}

void RestoreConsoleSettings ()
{
    cursorInfo.bVisible = oldVisible;
    SetConsoleCursorInfo ( hOutput, &cursorInfo );
}

void HideCursor ()
{
    CONSOLE_CURSOR_INFO info = cursorInfo;
    info.bVisible = FALSE;
    SetConsoleCursorInfo ( hOutput, &info );
}

void ShowCursor ()
{
    CONSOLE_CURSOR_INFO info = cursorInfo;
    info.bVisible = TRUE;
    SetConsoleCursorInfo ( hOutput, &info );
}

int GetKey ()
{
    int key = 0;
    UCHAR *ptr = ( UCHAR * ) &key;
    int ch = getch ();

    // "Eat" F1-F10, Home, End, Home, End, Delete, Insert, PgUp, PgDown, PageUp, PageDown, and the arrow keys
    if ( ch == 0x00 ) {
        ch = getch ();
        return 0;
    }

    if ( ch == 0xE0 ) {
        *ptr++ = 0x1B;
        *ptr++ = 0x5B;
        ch = getch ();
        switch ( ch ) {
            case 0x48 : ch = 0x41;  break;
            case 0x50 : ch = 0x42;  break;
            case 0x4D : ch = 0x43;  break;
            case 0x4B : ch = 0x44;  break;
        }
    }
    *ptr++ = ( UCHAR ) ch;

    return key;
}

bool KeyPressed ()
{
    return ( kbhit () != 0 ) ? true : false;
}

ULONG CurrentTime ()
{
    LARGE_INTEGER time;
    QueryPerformanceCounter ( &time );

    return ( ULONG ) ( 1000 * time.QuadPart / timerFrequency.QuadPart );
}

void ClearScreen ()
{
    DWORD dwActual;
    COORD origin;
    origin.X = 0;
    origin.Y = 0;
    FillConsoleOutputCharacter ( hOutput, ' ', screenInfo.dwSize.X * screenInfo.dwSize.Y, origin, &dwActual );

    curX = screenInfo.dwCursorPosition.X = 0;
    curY = screenInfo.dwCursorPosition.Y = 0;
}

void GetXY ( ULONG *x, ULONG *y )
{
    GetConsoleScreenBufferInfo ( hOutput, &screenInfo );
    *x = screenInfo.dwCursorPosition.X;
    *y = screenInfo.dwCursorPosition.Y;
}

void GotoXY ( ULONG x, ULONG y )
{
    curX = x;
    curY = y;
    if ( cursorInfo.bVisible == TRUE ) {
        COORD cursor;
        cursor.X = ( SHORT ) curX;
        cursor.Y = ( SHORT ) curY;
        SetConsoleCursorPosition ( hOutput, cursor );
    }
}

void PutXY ( ULONG x, ULONG y, char *ptr, int length )
{
    DWORD count;
    currentPos.X = ( SHORT ) x;
    currentPos.Y = ( SHORT ) y;
    WriteConsoleOutputCharacter ( hOutput, ptr, length, currentPos, &count );
    curX = x + length;
    curY = y;
}

void Put ( char *ptr, int length )
{
    DWORD count;
    currentPos.X = ( SHORT ) curX;
    currentPos.Y = ( SHORT ) curY;
    WriteConsoleOutputCharacter ( hOutput, ptr, length, currentPos, &count );
    curX += length;
}

#endif

void outByte ( UCHAR val )
{
    char buffer [ 3 ], *ptr = &buffer[3], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex ( val );
    Put ( ptr, 2 );
}

void outWord ( USHORT val )
{
    char buffer [5], *ptr = &buffer[5], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex (( char ) val );
    Put ( ptr, 4 );
}

void outLong ( ULONG val )
{
    char buffer [9], *ptr = &buffer[9], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex (( char ) val );
    Put ( ptr, 8 );
}
