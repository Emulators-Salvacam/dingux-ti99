//----------------------------------------------------------------------------
//
// File:        fs.cpp
// Date:        05-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A base class for TI filesystem classes
//
// Copyright (c) 2003 Marc Rousseau, All Rights Reserved.
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
#include "fs.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "arcfs.hpp"
#include "pseudofs.hpp"
#include "fileio.hpp"

DBG_REGISTER ( __FILE__ );

static inline USHORT GetUSHORT ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUSHORT", true );

    const UCHAR *ptr = ( const UCHAR * ) _ptr;
    return ( USHORT ) (( ptr [0] << 8 ) | ptr [1] );
}

static inline USHORT GetUSHORT_LE ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUSHORT_LE", true );

    const UCHAR *ptr = ( const UCHAR * ) _ptr;
    return ( USHORT ) (( ptr [1] << 8 ) | ptr [0] );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFileSystem *cFileSystem::Open ( const char *filename )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::Open", true );
    
    cFileSystem *disk = cDiskFileSystem::Open ( filename );
    if ( disk == NULL ) {
        disk = cArchiveFileSystem::Open ( filename );
        if ( disk == NULL ) {
            disk = cPseudoFileSystem::Open ( filename );
        }
    }

    return disk;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidName ( const char *name )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::IsValidName", true );

    int i = 0;

    for ( ; i < MAX_FILENAME; i++ ) {
        if ( name [i] == '.' ) return false;
        if ( name [i] == ' ' ) break;
        if ( ! isprint ( name [i] )) return false;
    }
    
    for ( i++; i < MAX_FILENAME; i++ ) {
        if ( name [i] != ' ' ) return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidFDR
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidFDR ( const sFileDescriptorRecord *fdr )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::IsValidFDR", true );

    int totalSectors = GetUSHORT ( &fdr->TotalSectors );

    // Make sure the filename valid
    if ( IsValidName ( fdr->FileName ) == false ) return false;
    
    // Simple sanity checks
    if (( fdr->FileStatus & PROGRAM_TYPE ) != 0 ) {
        if (( fdr->FileStatus & ( INTERNAL_TYPE | VARIABLE_TYPE )) != 0 ) return false;
        if ( fdr->RecordsPerSector != 0 ) return false;
    } else {
        if ( fdr->RecordsPerSector != DEFAULT_SECTOR_SIZE / fdr->RecordLength ) return false;
        int recordCount = GetUSHORT_LE ( &fdr->NoFixedRecords );
        if (( fdr->FileStatus & VARIABLE_TYPE ) == 0 ) {
            if ( fdr->EOF_Offset != 0 ) return false;
        } else {
            if ( recordCount != totalSectors ) return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cFileSystem::CreateFile ( sFileDescriptorRecord *fdr )
{
    FUNCTION_ENTRY ( this, "cFileSystem::CreateFile", true );

    return new cFile ( this, fdr );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::VerboseHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cFileSystem::VerboseHeader ( int ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::VerboseHeader", false );
    
    return "";
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::PrintVerboseInformation
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::PrintVerboseInformation ( const sFileDescriptorRecord * ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::PrintVerboseInformation", false );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::ShowDirectory
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::ShowDirectory ( bool verbose ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::ShowDirectory", true );

    if ( IsValid () == false ) {
        printf ( "Unrecognized media format\n" );
        return;
    }

    char name [MAX_FILENAME+1];
    GetName ( name, sizeof ( name ));

    printf ( "\n" );
    printf ( "Directory of %10.10s\n", name );
    printf ( "\n" );
    printf ( "  Filename   Size Type        P%s\n", ( verbose == true ) ? VerboseHeader (0) : "" );
    printf ( " ==========  ==== =========== =%s\n", ( verbose == true ) ? VerboseHeader (1) : "" );

    int sectorsUsed = 0;

    int fileCount = FileCount ();

    for ( int i = 0; i < fileCount; i++ ) {

        const sFileDescriptorRecord *FDR = GetFileDescriptor ( i );

        printf ( "  %10.10s", FDR->FileName );
        int size = GetUSHORT ( &FDR->TotalSectors ) + 1;
        sectorsUsed += size;
        printf ( " %4d", size );
        if ( FDR->FileStatus & PROGRAM_TYPE ) {
            printf ( " PROGRAM    " );
        } else {
            printf ( " %s/%s %3d", ( FDR->FileStatus & INTERNAL_TYPE ) ? "INT" : "DIS",
                                   ( FDR->FileStatus & VARIABLE_TYPE ) ? "VAR" : "FIX",
                                   FDR->RecordLength ? FDR->RecordLength : DEFAULT_SECTOR_SIZE );
        }
        printf (( FDR->FileStatus & WRITE_PROTECTED_TYPE ) ? " Y" : "  " );
        if ( verbose == true ) {
            PrintVerboseInformation ( FDR );
        }

        printf ( "\n" );
    }

    int totalSectors   = TotalSectors ();
    int totalAvailable = FreeSectors ();

    printf ( "\n" );
    printf ( "  Available: %4d  Used: %4d\n", totalAvailable, sectorsUsed );
    printf ( "      Total: %4d   Bad: %4d\n", totalSectors, totalSectors - sectorsUsed - totalAvailable );
    printf ( "\n" );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::GetFilenames
// Purpose:     Return a list of all files on the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cFileSystem::GetFilenames ( char *names[] ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::GetFilenames", true );

    int fileCount = FileCount ();

    for ( int i = 0; i < fileCount; i++ ) {

        const sFileDescriptorRecord *FDR = GetFileDescriptor ( i );

        names [i] = new char [11];
        memcpy ( names [i], FDR->FileName, 10 );
        names [i][10] = '\0';
        for ( int j = 9; j > 0; j-- ) {
            if ( names [i][j] != ' ' ) break;
            names [i][j] = '\0';
        }

        TRACE ( names [i] );
    }

    return fileCount;
}
