//----------------------------------------------------------------------------
//
// File:        arcfs.cpp
// Date:        15-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to simulate a filesystem for an ARC file (Barry Boone's Archive format)
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
#include "diskio.hpp"
#include "arcfs.hpp"
#include "pseudofs.hpp"
#include "fileio.hpp"
#include "support.hpp"
#include "decodelzw.hpp"

DBG_REGISTER ( __FILE__ );

// 18-byte structure used to hold file descriptors in .ark files

struct sArcFileDescriptorRecord {
    char      FileName [ MAX_FILENAME ];
    UCHAR     FileStatus;
    UCHAR     RecordsPerSector;
    USHORT    TotalSectors;
    UCHAR     EOF_Offset;
    UCHAR     RecordLength;
    USHORT    NoFixedRecords;               // For some strange reason, this is little-endian!
};

const int DIRS_PER_SECTOR = ( DEFAULT_SECTOR_SIZE + sizeof ( sArcFileDescriptorRecord ) - 1 ) / sizeof ( sArcFileDescriptorRecord );

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
// Procedure:   cArchiveFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem *cArchiveFileSystem::Open ( const char *filename )
{
    FUNCTION_ENTRY ( NULL, "cArchiveFileSystem::Open", true );
    
    cPseudoFileSystem *container = cPseudoFileSystem::Open ( filename );

    if ( container != NULL ) {
        cArchiveFileSystem *disk = new cArchiveFileSystem ( container );
        container->Release ( NULL );
        if ( disk->IsValid () == true ) {
            return disk;
        }
        disk->Release ( NULL );
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::cArchiveFileSystem ( cPseudoFileSystem *container ) :
    m_Container ( container ),
    m_Decoder ( NULL ),
    m_FileCount ( 0 ),
    m_FileIndex ( 0 ),
    m_TotalSectors ( 0 )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem ctor", true );

    memset ( m_Directory, 0, sizeof ( m_Directory ));

    m_Container->AddRef ( this );

    LoadFile ();
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::~cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::~cArchiveFileSystem ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem dtor", true );

    for ( int i = 0; i < m_FileCount; i++ ) {
        sFileDescriptorRecord *fdr = &m_Directory [i];
        delete [] * ( UCHAR ** ) &fdr->reserved2;
        delete * ( sSector ** ) &fdr->DataChain;
    }
    
    m_Container->Release ( this );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DirectoryCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DirectoryCallback ( void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( token, "cArchiveFileSystem::DirectoryCallback", false );

    cArchiveFileSystem *me = ( cArchiveFileSystem * ) token;

    sArcFileDescriptorRecord *arc = ( sArcFileDescriptorRecord * ) buffer;

    // Populate the directory
    for ( int i = 0; i < 14; i++ ) {

        if ( IsValidName ( arc->FileName ) == false ) break;

        sFileDescriptorRecord *fdr = &me->m_Directory [me->m_FileCount++];

        // Copy the fields from the .ark directory to the FDR
        memcpy ( fdr->FileName, arc->FileName, MAX_FILENAME );
        fdr->FileStatus        = arc->FileStatus;
        fdr->RecordsPerSector  = arc->RecordsPerSector;
        fdr->TotalSectors      = arc->TotalSectors;
        fdr->EOF_Offset        = arc->EOF_Offset;
        fdr->RecordLength      = arc->RecordLength;
        fdr->NoFixedRecords    = arc->NoFixedRecords;

        // Allocate space for the file's data and store the pointer in a reserved portion of the FDR
        int totalSectors = GetUSHORT ( &fdr->TotalSectors );
        * ( UCHAR ** ) &fdr->reserved2 = new UCHAR [ DEFAULT_SECTOR_SIZE * totalSectors ];

        // Allocate a fake sector for use by this file
        sSector *sector = new sSector;
        memset ( sector, 0, sizeof ( sSector ));
        sector->Size = DEFAULT_SECTOR_SIZE;
        * ( sSector ** ) &fdr->DataChain = sector;

        me->m_TotalSectors += totalSectors + 1;

        arc++;
    }

    // Look for the end of the directory and swith to the data callback
    if ( memcmp (( char * ) buffer + 252, "END!", 4 ) == 0 ) {
        // Set up the call back for the first file
        return DataCallback ( buffer, size, token );
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DataCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DataCallback ( void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( token, "cArchiveFileSystem::DataCallback", false );

    cArchiveFileSystem *me = ( cArchiveFileSystem * ) token;

    // Get the FDR for the next file in line
    sFileDescriptorRecord *fdr = &me->m_Directory [me->m_FileIndex++];

    int totalSectors = GetUSHORT ( &fdr->TotalSectors );

    buffer = * ( void ** ) fdr->reserved2;
    size   = DEFAULT_SECTOR_SIZE * totalSectors;

    me->m_Decoder->SetWriteCallback ( DataCallback, buffer, size, token );

    return true;
}
    
//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::LoadFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::LoadFile ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::LoadFile", true );

    cFile *file = m_Container->OpenFile ( NULL );
    if ( file == NULL ) {
        return;
    }
    
    sFileDescriptorRecord *FDR = file->GetFDR ();

    int recLen   = FDR->RecordLength;
    int recCount = GetUSHORT_LE ( &FDR->NoFixedRecords );
    int size     = recLen * recCount;

    UCHAR *inputBuffer = new UCHAR [ size ];
    for ( int i = 0; i < recCount; i++ ) {
        file->ReadRecord ( inputBuffer + ( i * recLen ), recLen );
    }

    // If it looks like it might be an archive, decode it
    if ( inputBuffer [0] == 0x80 ) {
        m_Decoder = new cDecodeLZW;
        char buffer [256];
        m_Decoder->SetWriteCallback ( DirectoryCallback, buffer, 256, this );
        m_Decoder->ParseBuffer ( inputBuffer, size );
        delete m_Decoder;
        m_Decoder = NULL;
    }
    
    delete [] inputBuffer;

    file->Release ( NULL );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FileCount () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::FileCount", true );

    return m_FileCount;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cArchiveFileSystem::GetFileDescriptor ( int index ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFileDescriptor", true );

    ASSERT ( index < m_FileCount );

    return &m_Directory [index];
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFreeSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FreeSectors () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFreeSectors", true );

    return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetTotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::TotalSectors () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetTotalSectors", true );

    return m_TotalSectors;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cArchiveFileSystem::GetFileSector ( sFileDescriptorRecord *FDR, int index )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFileSector", true );

    sSector *sector = * ( sSector ** ) &FDR->DataChain;

    UCHAR *fileBuffer = * ( UCHAR ** ) &FDR->reserved2;

    sector->Data = fileBuffer + ( index * DEFAULT_SECTOR_SIZE );

    return sector;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::ExtendFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::ExtendFile ( sFileDescriptorRecord *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::ExtendFile", true );

    FATAL ( "Function not implemented" );

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::TruncateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::TruncateFile ( sFileDescriptorRecord *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::TruncateFile", true );

    FATAL ( "Function not implemented" );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DiskModified
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::DiskModified ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::DiskModified", true );

    FATAL ( "Function not implemented" );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetPath
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::GetPath ( char *buffer, size_t maxLen ) const 
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetPath", true );

    return m_Container->GetPath ( buffer, maxLen );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::GetName ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetName", true );

    return m_Container->GetName ( buffer, maxLen );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsValid () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::IsValid", true );

    return ( m_FileCount > 0 ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsCollection
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsCollection () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::IsCollection", true );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cArchiveFileSystem::OpenFile ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::OpenFile", true );
    
    int len = strlen ( filename );
    
    for ( int i = 0; i < m_FileCount; i++ ) {
        sFileDescriptorRecord *FDR = &m_Directory [i];
        if ( strnicmp ( FDR->FileName, filename, len ) == 0 ) {
             return cFileSystem::CreateFile ( FDR );
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cArchiveFileSystem::CreateFile ( const char *, UCHAR, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::CreateFile", true );
    
    FATAL ( "Function not implemented" );
    
    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::AddFile ( cFile * )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::AddFile", true );

    FATAL ( "Function not implemented" );

    return false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DeleteFile ( const char * )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::DeleteFile", true );

    FATAL ( "Function not implemented" );

    return false;
}
