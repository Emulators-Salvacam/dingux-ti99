//----------------------------------------------------------------------------
//
// File:        diskfs.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description: A class to manage the filesystem information on a TI disk.
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
#include "diskfs.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

cDiskFileSystem *cDiskFileSystem::sm_SortDisk;

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
// Procedure:   cDiskFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem *cDiskFileSystem::Open ( const char *filename )
{
    FUNCTION_ENTRY ( NULL, "cDiskFileSystem::Open", true );
    
    cDiskMedia *media = new cDiskMedia ( filename );

    if ( media->GetFormat () != FORMAT_UNKNOWN ) {
        cDiskFileSystem *disk = new cDiskFileSystem ( media );
        media->Release ( NULL );
        if ( disk->IsValid () == true ) {
            return disk;
        }
        disk->Release ( NULL );
    }

    media->Release ( NULL );

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem ctor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::cDiskFileSystem ( cDiskMedia *media ) :
    m_Media ( media ),
    m_VIB ( NULL )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem ctor", true );

    if ( m_Media != NULL ) {
        m_Media->AddRef ( this );
        sSector *sec = m_Media->GetSector ( 0, 0, 0 );
        if ( sec != NULL ) {
            m_VIB = ( VIB * ) sec->Data;
        }
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem dtor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::~cDiskFileSystem ()
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem dtor", true );

    if ( m_Media != NULL ) {
	m_Media->Release ( this );
        m_Media = NULL;
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFreeSector
// Purpose:     Find a free sector on the disk beginning at sector 'start'
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFreeSector ( int start ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindFreeSector", true );

    int index = ( start / 8 ) * 8;

    for ( unsigned i = ( start / 8 ); i < SIZE ( m_VIB->AllocationMap ); i++ ) {
        UCHAR bits = m_VIB->AllocationMap [i];
        for ( int j = 0; j < 8; j++ ) {
            if ((( bits & 1 ) == 0 ) && ( index >= start )) {
                return index;
            }
            index++;
            bits >>= 1;
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::SetSectorAllocation
// Purpose:     Update the allocation bitmap in the VIB for the indicated sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::SetSectorAllocation ( int index, bool bUsed )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::SetSectorAllocation", true );

    int i   = index / 8;
    int bit = index % 8;

    if ( bUsed == true ) {
        m_VIB->AllocationMap [i] |= 1 << bit;
    } else {
        m_VIB->AllocationMap [i] &= ~ ( 1 << bit );
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindLastSector
// Purpose:     Return the index of the last sector of the file
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindLastSector ( sFileDescriptorRecord *FDR ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindLastSector", true );
    
    int totalSectors = GetUSHORT ( &FDR->TotalSectors );

    const CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        ASSERT ( offset > count );

        count = offset;
        chain++;
    }

    return count - 1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileSector
// Purpose:     Add the sector to the file's sector chain
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFileSector ( sFileDescriptorRecord *FDR, int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFileSector", true );

    int totalSectors = GetUSHORT ( &FDR->TotalSectors );

    CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count      = 0;
    int lastOffset = 0;

    // Walk the chain to find the last entry
    while ( count < totalSectors ) {
        lastOffset = count;
        ASSERT ( chain < FDR->DataChain + MAX_CHAINS );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;
        ASSERT ( offset > count );
        count      = offset;
        chain++;
    }

    // See if we can append to the last chain
    if ( count > 0 ) {
        int start  = chain[-1].start + (( int ) ( chain[-1].start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain[-1].offset << 4 ) + ( chain[-1].start_offset >> 4 ) + 1;
        if ( index == start + offset - lastOffset ) {
            chain[-1].start_offset = (( totalSectors & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
            chain[-1].offset       = ( totalSectors >> 4 ) & 0xFF;
            totalSectors++;
            FDR->TotalSectors = GetUSHORT ( &totalSectors );
            return true;
        }
    }

    // Start a new chain if there is room
    if ( chain < FDR->DataChain + MAX_CHAINS ) {
        chain->start        = index & 0xFF;
        chain->start_offset = (( totalSectors & 0x0F ) << 4 ) | (( index >> 8 ) & 0x0F );
        chain->offset       = ( totalSectors >> 4 ) & 0xFF;
        totalSectors++;
        FDR->TotalSectors = GetUSHORT ( &totalSectors );
        return true;
    }

    WARNING ( "Not enough room in CHAIN list" );

    return false;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cDiskFileSystem::FindSector ( int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindSector", true );

    int trackSize = (( m_VIB != NULL ) && ( m_VIB->SectorsPerTrack != 0 )) ? m_VIB->SectorsPerTrack : 9;

    int t = index / trackSize;
    int s = index % trackSize;
    int h = 0;

    if ( t >= m_Media->NumTracks ()) {
        t = 2 * m_Media->NumTracks () - t - 1;
        h = 1;
    }

    if ( t >= m_Media->NumTracks ()) {
        WARNING ( "Invalid sector index (" << index << ")" );
        return NULL;
    }

    return m_Media->GetSector ( t, h, s );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sSector *cDiskFileSystem::FindSector ( int index ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindSector", true );

    int trackSize = (( m_VIB != NULL ) && ( m_VIB->SectorsPerTrack != 0 )) ? m_VIB->SectorsPerTrack : 9;

    int t = index / trackSize;
    int s = index % trackSize;
    int h = 0;

    if ( t >= m_Media->NumTracks ()) {
        t = 2 * m_Media->NumTracks () - t - 1;
        h = 1;
    }

    if ( t >= m_Media->NumTracks ()) {
        WARNING ( "Invalid sector index (" << index << ")" );
        return NULL;
    }

    return m_Media->GetSector ( t, h, s );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFileDescriptorIndex
// Purpose:     Return the sector index of the file descriptor with the given filename
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFileDescriptorIndex ( const char *name )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindFileDescriptorIndex", true );

    USHORT *dirIndex = ( USHORT * ) FindSector ( 1 )->Data;

    int start = ( dirIndex [0] == 0 ) ? 1 : 0;
    int len   = ( strlen ( name ) < 10 ) ? strlen ( name ) : 10;

    for ( int i = start; i < 128; i++ ) {
        if ( dirIndex [i] == 0 ) break;
        int index = GetUSHORT ( &dirIndex [i] );
        sFileDescriptorRecord *FDR = ( sFileDescriptorRecord * ) FindSector ( index )->Data;
        if ( strnicmp ( FDR->FileName, name, len ) == 0 ) {
            return index;
        }
    }

    return -1;
}

typedef int (*QSORT_FUNC) ( const void *, const void * );

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::sortDirectoryIndex
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::sortDirectoryIndex ( const void *ptr1, const void *ptr2 )
{
    sFileDescriptorRecord *fdr1 = ( sFileDescriptorRecord * ) sm_SortDisk->FindSector ( GetUSHORT ( ptr1 ))->Data;
    sFileDescriptorRecord *fdr2 = ( sFileDescriptorRecord * ) sm_SortDisk->FindSector ( GetUSHORT ( ptr2 ))->Data;

    return strcmp ( fdr1->FileName, fdr2->FileName );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::AddFileDescriptor ( const sFileDescriptorRecord *FDR )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFileDescriptor", true );

    // Find a place to put the FDR
    int fdrIndex = FindFreeSector ( 0 );
    if ( fdrIndex == -1 ) {
        WARNING ( "Out of disk space" );
        return -1;
    }

    USHORT *FDI = ( USHORT * ) FindSector ( 1 )->Data;

    sm_SortDisk = this;

    // Preserve the 'visibilty' of files
    int start = (( FDI [0] == 0 ) && ( FDI [1] != 0 )) ? 1 : 0;

    // Look for a free slot in the file descriptor index
    for ( int i = start; i < 127; i++ ) {

        if ( FDI [i] == 0 ) {

            // Mark the new FDR's sector as used
            SetSectorAllocation ( fdrIndex, true );

            // Copy the FDR to the sector on m_Media
            sFileDescriptorRecord *newFDR = ( sFileDescriptorRecord * ) FindSector ( fdrIndex )->Data;
            memcpy ( newFDR, FDR, DEFAULT_SECTOR_SIZE );

            // Make sure the new name is padded with spaces
            for ( unsigned j = strlen ( newFDR->FileName ); j < sizeof ( newFDR->FileName ); j++ ) {
                newFDR->FileName [j] = ' ';
            }

            // Zero out the CHAIN list
            newFDR->TotalSectors = 0;
            memset ( newFDR->DataChain, 0, sizeof ( CHAIN ) * MAX_CHAINS );

            // Add the name and resort the directory
            FDI [i] = GetUSHORT ( &fdrIndex );
            qsort ( FDI + start, i - start + 1, sizeof ( USHORT ), ( QSORT_FUNC ) sortDirectoryIndex );
            break;
        }
    }

    return fdrIndex;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FileCount () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FileCount", true );

    const USHORT *dirIndex = ( const USHORT * ) FindSector ( 1 )->Data;

    int start = ( dirIndex [0] == 0 ) ? 1 : 0;

    for ( int i = start; i < 128; i++ ) {
        if ( dirIndex [i] == 0 ) return i - start;
    }

    return 127;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileDescriptor
// Purpose:     
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cDiskFileSystem::GetFileDescriptor ( int index ) const
{
    FUNCTION_ENTRY  ( this, "cDiskFileSystem::GetFileDescriptor", false );

    const USHORT *dirIndex = ( const USHORT * ) FindSector ( 1 )->Data;

    int start = ( dirIndex [0] == 0 ) ? 1 : 0;

    const sSector *sector = FindSector ( GetUSHORT ( &dirIndex [start + index] ));

    return ( const sFileDescriptorRecord * ) sector->Data;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FreeSectors
// Purpose:     Count the number of free sectors on the disk as indicated by the
//              allocation bitmap in the VIB
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FreeSectors () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FreeSectors", true );

    int free = 0;

    for ( unsigned i = 0; i < SIZE ( m_VIB->AllocationMap ); i++ ) {
        UCHAR bits = m_VIB->AllocationMap [i];
        for ( int j = 0; j < 8; j++ ) {
            if (( bits & 1 ) == 0 ) free++;
            bits >>= 1;
        }
    }

    return free;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TotalSectors
// Purpose:     
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::TotalSectors () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::TotalSectors", true );

    return GetUSHORT ( &m_VIB->FormattedSectors ) - 2;
}
 
//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::VerboseHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cDiskFileSystem::VerboseHeader ( int index ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::VerboseHeader", false );

    static char *header [2] = {
        " FDI Chains",
        " === ======="
    };

    ASSERT ( index < 2 );

    return header [index];
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::PrintVerboseInformation
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::PrintVerboseInformation ( const sFileDescriptorRecord *FDR ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::PrintVerboseInformation", false );

    const CHAIN *chain = FDR->DataChain;
    int totalSectors   = GetUSHORT ( &FDR->TotalSectors );

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        ASSERT ( offset > count );
        
        printf ( " %03d/%03d", start, offset - count );
        
        count = offset;
        chain++;
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileSector
// Purpose:     Get then Nth sector for this file.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cDiskFileSystem::GetFileSector ( sFileDescriptorRecord *FDR, int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetFileSector", true );

    int totalSectors = GetUSHORT ( &FDR->TotalSectors );

    if ( index >= totalSectors ) {
        WARNING ( "Requested index (" << index << ") exceeds totalSectors (" << totalSectors << ")" );
        return NULL;
    }

    const CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;
        
        ASSERT ( offset > count );
        
        // Is it in this chain?
        if ( index < offset ) {
            int sector = start + ( index - count );
            return FindSector ( sector );
        }

        count = offset;
        chain++;
    }

    FATAL ( "Internal error: Error traversing file CHAIN" );

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::ExtendFile
// Purpose:     Increase the sector allocation for this file by 'count'.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::ExtendFile ( sFileDescriptorRecord *FDR, int count )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::ExtendFile", true );

    // Try to extend the file without fragmenting
    int start = FindLastSector ( FDR ) + 1;
    if ( start == 0 ) {
        start = 34;
    }

    for ( int i = 0; i < count; i++ ) {

        int index = FindFreeSector ( start );
        if ( index == -1 ) {
            // Can't stay contiguous, try any available sector - start at sector 34 (from TI-DSR)
            index = FindFreeSector ( 34 );
            if ( index == -1 ) {
                // No 'normal' sectors left - try FDI sector range
                index = FindFreeSector ( 0 );
                if ( index == -1 ) {
                    WARNING ( "Disk is full" );
                    return i;
                }
            }
        }

        // Add this sector to the file chain and mark it in use
        if ( AddFileSector ( FDR, index ) == false ) {
            WARNING ( "File is too fragmented" );
            return i;
        }

        sSector *sector = FindSector ( index );
        memset ( sector->Data, 0, DEFAULT_SECTOR_SIZE );

        SetSectorAllocation ( index, true );

        start = index + 1;

        DiskModified ();
    }

    return count;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TruncateFile
// Purpose:     Free all sectors beyond the indicated sector count limit
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::TruncateFile ( sFileDescriptorRecord *FDR, int limit )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::TruncateFile", true );

    int totalSectors = GetUSHORT ( &FDR->TotalSectors );

    if ( limit >= totalSectors ) {
        WARNING ( "limit (" << limit << ") exceeds totalSectors (" << totalSectors << ")" );
        return;
    }

    CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < limit ) {

        ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        ASSERT ( offset > count );

        if ( limit < offset ) {

            // Mark the excess sectors as free
            for ( int i = limit - count; i < offset - count; i++ ) {
                SetSectorAllocation ( start + i, false );
            }

            // Update the chain
            chain->start        = start & 0xFF;
            chain->start_offset = (( limit & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
            chain->offset       = ( limit >> 4 ) & 0xFF;
        }

        count = offset;
        chain++;
    }

    // Zero out the rest of the chain entries & free their sectors
    while ( count < totalSectors ) {

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        // Mark the sectors as free
        for ( int i = 0; i < offset - count; i++ ) {
            SetSectorAllocation ( start + i, false );
        }

        chain->start        = 0;
        chain->start_offset = 0;
        chain->offset       = 0;

        count = offset;

        chain++;
    }

    FDR->TotalSectors = GetUSHORT ( &limit );

    DiskModified ();
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DiskModified 
// Purpose:     
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::DiskModified ()
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DiskModified", true );

    m_Media->DiskModified ();
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetPath
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::GetPath ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetPath", true );

    if ( m_Media == NULL ) return false;

    const char *path = m_Media->GetName ();

    if ( maxLen < strlen ( path ) + 1 ) {
	ERROR ( "Destination buffer is too short" );
        return false;
    }

    strcpy ( buffer, path );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetName
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::GetName ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetName", true );

    size_t length = MAX_FILENAME;

    while (( length > 0 ) && (  m_VIB->VolumeName [ length - 1 ] == ' ' )) {
        length--;
    }

    if ( maxLen < length + 1 ) {
	ERROR ( "Destination buffer is too short" );
        return false;
    }

    sprintf ( buffer, "%*.*s", length, length, m_VIB->VolumeName );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsValid () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::IsValid", true );

    if (( m_Media == NULL ) || ( m_VIB == NULL )) return false;
    if ( m_Media->GetFormat () == FORMAT_UNKNOWN ) return false;
    if ( IsValidName ( m_VIB->VolumeName ) == false ) return false;
    if ( memcmp ( m_VIB->DSK, "DSK", 3 ) != 0 ) return false;

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsCollection
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsCollection () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::IsCollection", true );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cDiskFileSystem::OpenFile ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::OpenFile", true );

    cFile *file = NULL;

    int index = FindFileDescriptorIndex ( filename );

    if ( index != -1 ) {
	file = cFileSystem::CreateFile (( sFileDescriptorRecord * ) FindSector ( index )->Data );
    }

    return file;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::CreateFile
// Purpose:     Create a new file on the disk (deletes any pre-existing file)
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cDiskFileSystem::CreateFile ( const char *filename, UCHAR type, int recordLength )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::CreateFile", true );

    // Get rid of any existing file by this name
    int index = FindFileDescriptorIndex ( filename );
    if ( index != -1 ) {
        DeleteFile ( filename );
    }

    sFileDescriptorRecord fdr;
    memset ( &fdr, 0, sizeof ( fdr ));

    for ( int i = 0; i < MAX_FILENAME; i++ ) {
        fdr.FileName [i] = ( *filename != '\0' ) ? *filename++ : ' ';
    }

    fdr.FileStatus       = type;
    fdr.RecordsPerSector = ( type & VARIABLE_TYPE ) ? ( 255 / ( recordLength + 1 )) : 256 / recordLength;
    fdr.RecordLength     = recordLength;

    FATAL ( "Function not implemented yet" );

    return NULL;
}


//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFile ( cFile *file )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFile", true );

    const sFileDescriptorRecord *FDR = file->GetFDR ();

////    TRACE ( "Adding file " << file->GetName () << " to disk " << GetName ());

    // Look for a file on this disk with the same name
    int index = FindFileDescriptorIndex ( FDR->FileName );
    if ( index != -1 ) {
        // See if this file we found is the one we're trying to add
        if (( void * ) FDR == ( void * ) FindSector ( index )->Data ) {
            WARNING ( "File already exists on this disk" );
            return true;
        }

        // Remove the existing file
        DeleteFile ( FDR->FileName );
    }

    // First, make sure the disk has enough room for the file
    int totalSectors = GetUSHORT ( &FDR->TotalSectors );
    if ( FreeSectors () < totalSectors + 1 ) {
        WARNING ( "Not enough room on disk for file" );
        return false;
    }

    // Next, Try to add another filename
    int fdrIndex = AddFileDescriptor ( FDR );
    if ( fdrIndex == -1 ) {
        WARNING ( "No room left in the file descriptor table" );
        return false;
    }

    // Get a pointer to the new FDR so we can update the file chain
    sFileDescriptorRecord *newFDR = ( sFileDescriptorRecord * ) FindSector ( fdrIndex )->Data;

    // This shouldn't fail since we've already checked for free space
    if ( ExtendFile ( newFDR, totalSectors ) == false ) {
        FATAL ( "Internal error: Unable to extend file" );
        DeleteFile ( FDR->FileName );
        return false;
    }

    // Copy the data to the new data sectors
    for ( int i = 0; i < totalSectors; i++ ) {
        sSector *sector = GetFileSector ( newFDR, i );
        file->ReadSector ( i, sector->Data );
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::DeleteFile ( const char *fileName )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DeleteFile", true );

    // See if we have a file with this name
    int index = FindFileDescriptorIndex ( fileName );
    if ( index == -1 ) {
        return false;
    }

    USHORT *FDI = ( USHORT * ) FindSector ( 1 )->Data;

    int start = ( FDI [0] == 0 ) ? 1 : 0;

    // Remove the pointer to the FDR from the FDI
    int fdrIndex = GetUSHORT ( &index );
    for ( int i = start; i < 127; i++ ) {
        if ( FDI [i] == fdrIndex ) {
            memcpy ( FDI + i, FDI + i + 1, ( 127 - i ) * sizeof ( USHORT ));
            FDI [127] = 0;
            break;
        }
    }

    sFileDescriptorRecord *FDR = ( sFileDescriptorRecord * ) FindSector ( index )->Data;

    // Release all of the sectors owned by the file itself
    TruncateFile ( FDR, 0 );

    // Release the FDR's sector
    SetSectorAllocation ( index, false );

    DiskModified ();

    return true;
}
