//----------------------------------------------------------------------------
//
// File:        list.cpp
// Date:        06-Aug-2001
// Programmer:  Marc Rousseau
//
// Description: A program to list a BASIC or EXTENDED BASIC program
//
// Copyright (c) 2001-2003 Marc Rousseau, All Rights Reserved.
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

#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "diskio.hpp"
#include "fileio.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

struct sToken {
    UCHAR       Token;
    const char *Text;
};

sToken tokens [] = {

    { 129, "ELSE " },           // 0x81

    { 130, " :: " },            // 0x82 *
    { 131, "! " },              // 0x83 *
    { 132, "IF " },             // 0x84
    { 133, "GO " },             // 0x85
    { 134, "GOTO " },           // 0x86
    { 135, "GOSUB " },          // 0x87
    { 136, "RETURN " },         // 0x88
    { 137, "DEF " },            // 0x89
    { 138, "DIM " },            // 0x8A
    { 139, "END " },            // 0x8B
    { 140, "FOR " },            // 0x8C
    { 141, "LET " },            // 0x8D
    { 142, "BREAK " },          // 0x8E
    { 143, "UNBREAK " },        // 0x8F
    { 144, "TRACE " },          // 0x90
    { 145, "UNTRACE " },        // 0x91
    { 146, "INPUT " },          // 0x92
    { 147, "DATA " },           // 0x93
    { 148, "RESTORE " },        // 0x94
    { 149, "RANDOMIZE " },      // 0x95
    { 150, "NEXT " },           // 0x96
    { 151, "READ " },           // 0x97
    { 152, "STOP " },           // 0x98
    { 153, "DELETE " },         // 0x99
    { 154, "REM " },            // 0x9A
    { 155, "ON " },             // 0x9B
    { 156, "PRINT " },          // 0x9C
    { 157, "CALL " },           // 0x9D
    { 158, "OPTION " },         // 0x9E
    { 159, "OPEN " },           // 0x9F
    { 160, "CLOSE " },          // 0xA0
    { 161, "SUB " },            // 0xA1
    { 162, "DISPLAY " },        // 0xA2
    { 163, "IMAGE " },          // 0xA3 *
    { 164, "ACCEPT " },         // 0xA4
    { 165, "ERROR " },          // 0xA5 *
    { 166, "WARNING " },        // 0xA6 *
    { 167, "SUBEXIT " },        // 0xA7 *
    { 168, "SUBEND " },         // 0xA8 *
    { 169, "RUN " },            // 0xA9
    { 170, "LINPUT " },         // 0xAA *

    { 176, " THEN " },          // 0xB0
    { 177, " TO " },            // 0xB1
    { 178, " STEP " },          // 0xB2
    { 179, "," },               // 0xB3
    { 180, ";" },               // 0xB4
    { 181, ":" },               // 0xB5
    { 182, ")" },               // 0xB6
    { 183, "(" },               // 0xB7
    { 184, "&" },               // 0xB8

    { 186, "OR " },             // 0xBA *
    { 187, "AND " },            // 0xBB *
    { 188, "XOR " },            // 0xBC *
    { 189, "NOT " },            // 0xBD *

    { 190, "=" },               // 0xBE
    { 191, "<" },               // 0xBF
    { 192, ">" },               // 0xC0
    { 193, "+" },               // 0xC1
    { 194, "-" },               // 0xC2
    { 195, "*" },               // 0xC3
    { 196, "/" },               // 0xC4
    { 197, "^" },               // 0xC5

    { 199, NULL },              // 0xC7 - "string"
    { 200, NULL },              // 0xC8 - string
    { 201, NULL },              // 0xC9 - statement #

    { 202, "EOF" },             // 0xCA
    { 203, "ABS" },             // 0xCB
    { 204, "ATN" },             // 0xCC
    { 205, "COS" },             // 0xCD
    { 206, "EXP" },             // 0xCE
    { 207, "INT" },             // 0xCF
    { 208, "LOG" },             // 0xD0
    { 209, "SGN" },             // 0xD1
    { 210, "SIN" },             // 0xD2
    { 211, "SQR" },             // 0xD3
    { 212, "TAN" },             // 0xD4
    { 213, "LEN" },             // 0xD5
    { 214, "CHR$" },            // 0xD6
    { 215, "RND" },             // 0xD7
    { 216, "SEG$" },            // 0xD8
    { 217, "POS" },             // 0xD9
    { 218, "VAL" },             // 0xDA
    { 219, "STR$" },            // 0xDB
    { 220, "ASC" },             // 0xDC

    { 221, "PI " },             // 0xDD *

    { 222, "REC " },            // 0xDE

    { 223, "MAX " },            // 0xDF *
    { 224, "MIN " },            // 0xE0 *

    { 225, "RPT$ " },           // 0xE1 *
    { 232, "NUMERIC " },        // 0xE8 *

    { 233, "DIGIT" },           // 0xE9 *
    { 234, "UALPHA" },          // 0xEA *
    { 235, "SIZE" },            // 0xEB *
    { 236, "ALL" },             // 0xEC *
    { 237, "USING" },           // 0xED *
    { 238, "BEEP" },            // 0xEE *
    { 239, "ERASE" },           // 0xEF *
    { 240, "AT" },              // 0xF0 *
    { 241, "BASE" },            // 0xF1

    { 243, "VARIABLE" },        // 0xF3
    { 244, "RELATIVE" },        // 0xF4
    { 245, "INTERNAL" },        // 0xF5
    { 246, "SEQUENTIAL" },      // 0xF6
    { 247, "OUTPUT" },          // 0xF7
    { 248, "UPDATE" },          // 0xF8
    { 249, "APPEND" },          // 0xF9
    { 250, "FIXED" },           // 0xFA
    { 251, "PERMANENT" },       // 0xFB
    { 252, "TAB" },             // 0xFC
    { 253, "#" },               // 0xFD
    { 254, "VALIDATE" }         // 0xFE *
};

/*

  Sample TI-Extended BASIC program

00000000: 00 FF 37 83  37 7C 37 D7  00 6E 37 85  00 64 37 A3      7â7|7+ n7à d7ú
00000010: 1D 9D C8 04  49 4E 49 54  82 9D C8 04  4C 4F 41 44    ?¥+?INITé¥+?LOAD
00000020: B7 C7 09 44  53 4B 31 2E  44 49 41 47  B6 00 35 9D    +¦?DSK1.DIAG¦ 5¥
00000030: C8 05 43 4C  45 41 52 82  A2 F0 B7 C8  02 31 32 B3    +?CLEARéó=++?12¦
00000040: C8 01 32 B6  B5 C7 1C 4C  6F 61 64 69  6E 67 20 41    +?2¦¦¦?Loading A
00000050: 44 56 41 4E  43 45 44 20  44 49 41 47  4E 4F 53 54    DVANCED DIAGNOST
00000060: 49 43 53 00                                           ICS

Listing:

 100 CALL CLEAR :: DISPLAY AT(12,2):"Loading ADVANCED DIAGNOSTICS"
 110 CALL INIT :: CALL LOAD("DSK1.DIAG")

  - Header

00000000: 00 FF
00000002: 37 83  -> 0007
00000004: 37 7C  -> 0000
00000006: 37 D7  -> 005B

  - Line Number Table

00000008: 00 6E - 110
0000000A: 37 85
0000000C: 00 64 - 100
0000000E: 37 A3

  - Statement List (Tokenized Lines)

00000010: 1D - 9D C8 04 49 4E 49 54 82 9D C8 04 4C 4F 41 44 B7
               C7 09 44 53 4B 31 2E 44 49 41 47 B6 - 00
0000002E: 35 - 9D C8 05 43 4C 45 41 52 82 A2 F0 B7 C8 02 31 32
               B3 C8 01 32 B6 B5 C7 1C 4C 6F 61 64 69 6E 67 20
               41 44 56 41 4E 43 45 44 20 44 49 41 47 4E 4F 53
               54 49 43 53 - 00


  Sample TI-Extended BASIC program 2

00000000: 00 03 37 c7 37 c4 37 d7  00 0a 37 c9 0f 9c c7 0b  |..7.7.7...7.....|
00000010: 48 65 6c 6c 6f 20 57 6f  72 6c 64 00              |Hello World.|

Listing:

  10 PRINT "Hello World"

  Sample TI-Extended BASIC program 2 - PROTECTED

00000000: ff fd 37 c7 37 c4 37 d7  00 0a 37 c9 0f 9c c7 0b  |..7.7.7...7.....|
00000010: 48 65 6c 6c 6f 20 57 6f  72 6c 64 00              |Hello World.|

Listing:

  10 PRINT "Hello World"

*/

static inline USHORT GetUSHORT ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUSHORT", true );

    const UCHAR *ptr = ( const UCHAR * ) _ptr;
    return ( USHORT ) (( ptr [0] << 8 ) | ptr [1] );
}

sToken *FindToken ( UCHAR token )
{
    for ( unsigned i = 0; i < SIZE ( tokens ); i++ ) {
        if ( tokens [i].Token == token ) {
            return &tokens [i];
        }
    }

    return NULL;
}

bool ParseLine ( const UCHAR *lineStart, char *buffer, int size )
{
    sToken *tok;
    int count = *lineStart, len;

    const UCHAR *ptr = lineStart + 1;
    const UCHAR *end = lineStart + count;

    // Simple sanity check
    if ( *end != 0 ) return false;

    while ( ptr < end ) {

        UCHAR token = *ptr++;

        switch ( token ) {
            case 0xC7 :
                len = *ptr++;
                buffer += sprintf ( buffer, "\"%*.*s\"", len, len, ptr );
                ptr += len;
                break;
            case 0xC8 :
                len = *ptr++;
                buffer += sprintf ( buffer, "%*.*s", len, len, ptr );
                ptr += len;
                break;
            case 0xC9 :
                len = ( ptr [0] << 8 ) | ptr [1];
                buffer += sprintf ( buffer, "%d", len );
                ptr += 2;
                break;
            default :
                tok = FindToken ( token );
                if ( tok == NULL ) {
                    *buffer++ = token;
                } else {
                    buffer += sprintf ( buffer, tok->Text );
                }
                break;
        }
    }

    *buffer = '\0';

    return true;
}

bool VerifyProgram ( UCHAR *fileBuffer, int size )
{
    UCHAR *ptr = fileBuffer + 8;

    //    USHORT p1 = ( ptr [-8] << 8 ) | ptr [-7];
    USHORT p2 = ( ptr [-6] << 8 ) | ptr [-5];   // Last byte in line # table
    USHORT p3 = ( ptr [-4] << 8 ) | ptr [-3];
    //    USHORT p4 = ( ptr [-2] << 8 ) | ptr [-2];

    USHORT base    = p3;
    USHORT lineEnd = p2 - base;

    ULONG lastLine = 0x10000;

    while ( ptr < fileBuffer + lineEnd ) {
        ULONG  lineNo = ( ptr [0] << 8 ) | ptr [1];
        USHORT offset = (( ptr [2] << 8 ) | ptr [3] ) - base - 1 + 8;
        if ( lineNo > lastLine ) return false;
        if ( offset > size ) return false;
        lastLine = lineNo;
        ptr += 4;
    }

    return true;
}

bool ListProgram ( UCHAR *dataPtr )
{
    //    USHORT p1 = ( dataPtr [0] << 8 ) | dataPtr [1];
    USHORT p2 = ( dataPtr [2] << 8 ) | dataPtr [3];   // Last byte in line # table
    USHORT p3 = ( dataPtr [4] << 8 ) | dataPtr [5];
    //    USHORT p4 = ( dataPtr [6] << 8 ) | dataPtr [7];

    USHORT baseOffset = p3;
    USHORT lineEnd    = p2;

    UCHAR *basePtr = dataPtr + 8 - baseOffset - 1;
    UCHAR *linePtr = basePtr + lineEnd + 2;

    char output [1024];

    fprintf ( stdout, "\n" );

    while ( linePtr > dataPtr + 8 ) {

        linePtr -= 4;

        int lineNumber = ( linePtr [0] << 8 ) | linePtr [1];
        int offset     = ( linePtr [2] << 8 ) | linePtr [3];

        UCHAR *lineStart = basePtr + offset;
        if ( ParseLine ( lineStart, output, sizeof ( output )) == false ) {
            fprintf ( stderr, "**ERROR**: Unable to parse line %d\n", lineNumber );
            return false;
        }
        fprintf ( stdout, "%5d %s\n", lineNumber, output );
    }

    fprintf ( stdout, "\n" );

    return true;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

//    fprintf ( stdout, "Usage: list [options] file\n" );
    fprintf ( stdout, "Usage: list file\n" );
    fprintf ( stdout, "\n" );
}

double three = 3.0;

int main ( int argc, char *argv[] )
{
//    bool verboseMode  = false;

//    sOption optList [] = {
//        { 'v', "verbose",            OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &verboseMode,  NULL,           "" }
//    };

    if ( argc == 1 ) {
//        PrintHelp ( SIZE ( optList ), optList );
        PrintHelp ( 0, NULL );
        return 0;
    }

    printf ( "TI-99/4A BASIC Program List utility\n" );

    int index = 1;
//    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );
    index = ParseArgs ( index, argc, argv, 0, NULL );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    const char *fileName  = argv [index++];

    // Try to find and open the file
    cFile *file = cFile::Open ( fileName, "disks" );
    if ( file == NULL ) {
        fprintf ( stderr, "Unable to open file \"%s\"\n", fileName );
        return -1;
    }

    // Make sure the file is a PROGRAM file
    sFileDescriptorRecord *fdr = file->GetFDR ();
    if (( fdr->FileStatus & PROGRAM_TYPE ) == 0 ) {
        fprintf ( stderr, "File \"%s\" is not a PROGRAM type\n", fileName );
        file->Release ( NULL );
        return -1;
    }

    // Allocate a buffer large enough to hold the entire file
    int totalSectors = GetUSHORT ( &fdr->TotalSectors );
    int maxSize      = totalSectors * DEFAULT_SECTOR_SIZE;
    UCHAR *fileBuffer = new UCHAR [ maxSize ];
    int size = 0;
    for ( int i = 0; ; i++ ) {
        int count = file->ReadRecord ( fileBuffer + size, DEFAULT_SECTOR_SIZE );
        if ( count <= 0 ) break;
        size += count;
    }

    file->Release ( NULL );

    if ( VerifyProgram ( fileBuffer, size ) == false ) {
        fprintf ( stderr, "The file \"%s\" does not appear to be a BASIC program.\n", fileName );
    } else {
        ListProgram ( fileBuffer );
    }

    delete [] fileBuffer;

    return 0;
}
