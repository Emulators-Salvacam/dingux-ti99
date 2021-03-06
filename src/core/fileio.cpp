//----------------------------------------------------------------------------
//
// File:        fileio.cpp
// Date:        19-Sep-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains startup code for Linux/SDL
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
//   15-Jan-2003    Moved old code to support.cpp and added new cFile classes
//
//----------------------------------------------------------------------------

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "arcfs.hpp"
#include "pseudofs.hpp"
#include "fileio.hpp"
#include "support.hpp"

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

cFile *cFile::Open ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "cFile::Open", true );

    // Make a local copy of the filename and make sure it's bounded
    char baseFileName [ MAXPATH ];
    strncpy ( baseFileName, filename, MAXPATH );
    baseFileName [MAXPATH-1] = '\0';

    // See if it contains a disk:file type specification
    char *ptr = strrchr ( baseFileName, ':' );
    if ( ptr != NULL ) {
	// Seperate the diskname from the filename
        *ptr++ = '\0';
    }

    // Look for the file using the specified path
    const char *actualFileName = LocateFile ( baseFileName, path );
    if ( actualFileName == NULL ) {
        WARNING ( "Unable to locate file \"" << filename << "\"" );
        return NULL;
    }
/*
    // OK, we found the file - now we need to figure out what type it is

    // Look for the obvious first (diskname:filename)
    if ( ptr != NULL ) {
        cFileSystem *disk = cFileSystem::Open ( actualFileName );
        if ( disk != NULL ) {
            cFile *file = disk->OpenFile ( ptr );
            disk->Release ( NULL );
            return file;
        }
    }

    // See if we can detect either a TIFILES or v9t9 FIAD header
    cPseudoFileSystem *disk = cPseudoFileSystem::Open ( actualFileName );
    if ( disk != NULL ) {
        cFile *file = disk->OpenFile ( NULL );
        disk->Release ( NULL );
        return file;
    }

    disk->Release ( NULL );

fprintf ( stderr, "-- Function OpenFile is not implemented yet! --\n" );

    return NULL;
*/

    cFileSystem *disk = cFileSystem::Open ( actualFileName );

    if ( disk != NULL ) {
        cFile *file = disk->OpenFile ( ptr );
        disk->Release ( NULL );
        return file;
    }

fprintf ( stderr, "-- Function OpenFile is not implemented yet! --\n" );

    return NULL;

}

cFile::cFile ( cFileSystem *disk, sFileDescriptorRecord *fdr ) :
    cBaseObject ( "cFile" ),
    m_FileSystem ( disk ),
    m_FDR ( fdr ),
    m_TotalRecordsLeft ( 0 ),
    m_RecordsLeft ( 0 ),
    m_SectorIndex ( -1 ),
    m_RecordPtr ( NULL )
{
    FUNCTION_ENTRY ( this, "cFile ctor", true );

    ASSERT ( disk != NULL );
    ASSERT ( fdr != NULL );

    m_FileSystem->AddRef ( this );

    m_TotalRecordsLeft = ( m_FDR->FileStatus & PROGRAM_TYPE ) ? GetUSHORT ( &m_FDR->TotalSectors ) : GetUSHORT_LE ( &m_FDR->NoFixedRecords );

    memset ( m_SectorBuffer, 0, sizeof ( m_SectorBuffer ));
}

cFile::~cFile ()
{
    FUNCTION_ENTRY ( this, "cFile dtor", true );

    m_FileSystem->Release ( this );
}

bool cFile::ReadNextSector ()
{
    FUNCTION_ENTRY ( this, "cFile::ReadNextSector", true );

    int totalSectors = GetUSHORT ( &m_FDR->TotalSectors );

    if ( m_SectorIndex >= totalSectors - 1 ) {
        return false;
    }

    ReadSector ( ++m_SectorIndex, m_SectorBuffer );

    m_RecordPtr   = m_SectorBuffer;
    m_RecordsLeft = m_FDR->RecordsPerSector;

    return true;
}

int cFile::FileSize ()
{
    FUNCTION_ENTRY ( this, "cFile::FileSize", true );

    sFileDescriptorRecord *fdr = GetFDR ();

    int totalSectors = GetUSHORT ( &fdr->TotalSectors );
    int fileSize     = ( totalSectors - 1 ) * DEFAULT_SECTOR_SIZE;
    fileSize        += ( fdr->EOF_Offset != 0 ) ? fdr->EOF_Offset : DEFAULT_SECTOR_SIZE;

    return fileSize;
}

bool cFile::GetPath ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cFile::GetPath", true );

    // Get the base path from the underlying filesystem
    if ( m_FileSystem->GetPath ( buffer, maxLen ) == false ) return false;

    // Do we need to add the filename?
    if ( m_FileSystem->IsCollection () == true ) {
        maxLen -= strlen ( buffer );
        if ( maxLen < 1 ) return false;
        strcat ( buffer, ":" );
        buffer += strlen ( buffer );
        return GetName ( buffer, maxLen - 1 );
    }

    return true;
}

bool cFile::GetName ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cFile::GetName", true );

    size_t length = MAX_FILENAME;

    while (( length > 0 ) && (  m_FDR->FileName [ length - 1 ] == ' ' )) {
        length--;
    }

    if ( maxLen < length + 1 ) {
	ERROR ( "Destination buffer is too short" );
        return false;
    }

    sprintf ( buffer, "%*.*s", length, length, m_FDR->FileName );

    return true;
}

bool cFile::SeekRecord ( int index )
{
    FUNCTION_ENTRY ( this, "cFile::SeekRecord", true );

    if (( m_FDR->FileStatus & ( PROGRAM_TYPE | VARIABLE_TYPE )) != FIXED_TYPE ) {
        return false;
    }

    if ( index >= GetUSHORT_LE ( &m_FDR->NoFixedRecords )) {
        return false;
    }

    int sector = index / m_FDR->RecordsPerSector;
    ReadSector ( sector, m_SectorBuffer );

    int record = index - sector * m_FDR->RecordsPerSector;
    m_RecordPtr    = m_SectorBuffer + record * m_FDR->RecordLength;
    m_RecordsLeft -= record;

    return false;
}

int cFile::ReadRecord ( void *buffer, int max )
{
    FUNCTION_ENTRY ( this, "cFile::ReadRecord", true );

    if ( m_TotalRecordsLeft == 0 ) return -1;

    if ( m_RecordsLeft == 0 ) {
        ReadNextSector ();
    }

    int count = 0;

    if (( m_FDR->FileStatus & PROGRAM_TYPE ) == DATA_TYPE ) {

        int length = 0;

        if (( m_FDR->FileStatus & VARIABLE_TYPE ) == FIXED_TYPE ) {

            length = m_FDR->RecordLength;
            m_TotalRecordsLeft--;
            m_RecordsLeft--;

        } else {

            length = *m_RecordPtr++ + 1;
            if ( m_RecordPtr [length-1] == 0xFF ) {
                m_TotalRecordsLeft--;
                m_RecordsLeft = 0;
            }

        }

        count = ( length < max ) ? length : max;
        memcpy ( buffer, m_RecordPtr, count );
        m_RecordPtr += length;

        if (( m_FDR->FileStatus & VARIABLE_TYPE ) == VARIABLE_TYPE ) {
            char *end = ( char * ) buffer + count;
            *end = '\0';
        }

    } else {
        count = (( m_TotalRecordsLeft == 1 ) && ( m_FDR->EOF_Offset != 0 )) ? m_FDR->EOF_Offset : DEFAULT_SECTOR_SIZE;
        memcpy ( buffer, m_RecordPtr, count );
        m_TotalRecordsLeft--;
        m_RecordsLeft = 0;
    }

    return count;
}

int cFile::WriteRecord ( void *, int )
{
    FUNCTION_ENTRY ( this, "cFile::WriteRecord", true );

    // TBD

    return 0;
}

int cFile::ReadSector ( int index, void *buffer )
{
    FUNCTION_ENTRY ( this, "cFile::ReadSector", true );

    int totalSectors = GetUSHORT ( &m_FDR->TotalSectors );
    if ( index >= totalSectors ) return -1;

    const sSector *sector = m_FileSystem->GetFileSector ( m_FDR, index );
    memcpy ( buffer, sector->Data, DEFAULT_SECTOR_SIZE );

    return 0;
}

int cFile::WriteSector ( int index, void *buffer )
{
    FUNCTION_ENTRY ( this, "cFile::WriteSector", true );

    int totalSectors = GetUSHORT ( &m_FDR->TotalSectors );
    if ( index >= totalSectors ) {
        int count = index - totalSectors + 1;
        if ( m_FileSystem->ExtendFile ( m_FDR, count ) != count ) return -1;
    }

    sSector *sector = m_FileSystem->GetFileSector ( m_FDR, index );
    if ( memcmp ( sector->Data, buffer, DEFAULT_SECTOR_SIZE ) != 0 ) {
        memcpy ( sector->Data, buffer, DEFAULT_SECTOR_SIZE );
        m_FileSystem->DiskModified ();
    }

    return 0;
}
