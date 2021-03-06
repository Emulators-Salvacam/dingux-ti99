//----------------------------------------------------------------------------
//
// File:        fs.hpp
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

#ifndef FS_HPP_
#define FS_HPP_

#include "cBaseObject.hpp"

// Flags used by TI to indicate file types

#define DATA_TYPE               0x00
#define PROGRAM_TYPE            0x01

#define DISPLAY_TYPE            0x00
#define INTERNAL_TYPE           0x02

#define WRITE_PROTECTED_TYPE    0x08

#define FIXED_TYPE              0x00
#define VARIABLE_TYPE           0x80

const int MAX_FILENAME          = 10;
const int MAX_CHAINS            = 76;
const int MAX_FILES             = 127;      // Maximum number of files that can fit on a disk (DEFAULT_SECTOR_SIZE/2-1)

// On-Disk structures used by TI

struct VIB {
    char      VolumeName [ MAX_FILENAME ];
    USHORT    FormattedSectors;
    UCHAR     SectorsPerTrack;
    char      DSK [ 3 ];
    UCHAR     reserved;
    UCHAR     TracksPerSide;
    UCHAR     Sides;
    UCHAR     Density;
    UCHAR     reserved2 [ 36 ]; 
    UCHAR     AllocationMap [ 200 ];
};

struct CHAIN {
    UCHAR     start;
    UCHAR     start_offset;
    UCHAR     offset;
};

struct sFileDescriptorRecord {
    char      FileName [ MAX_FILENAME ];
    char      reserved1 [ 2 ];
    UCHAR     FileStatus;
    UCHAR     RecordsPerSector;
    USHORT    TotalSectors;
    UCHAR     EOF_Offset;
    UCHAR     RecordLength;
    USHORT    NoFixedRecords;               // For some strange reason, this is little-endian!
    char      reserved2 [ 8 ];
    CHAIN     DataChain [ MAX_CHAINS ];
};

struct sSector;

class cFile;

class cFileSystem : public cBaseObject {

private:

    // Disable the copy constructor and assignment operator defaults
    cFileSystem ( const cFileSystem & );		// no implementation
    void operator = ( const cFileSystem & );		// no implementation

protected:

    cFileSystem () : cBaseObject ( "cFileSystem" )     {}

    cFile *CreateFile ( sFileDescriptorRecord *FDR );

    // Funcions used by ShowDirectory
    virtual int FileCount () const = 0;
    virtual const sFileDescriptorRecord * GetFileDescriptor ( int ) const = 0;
    virtual int FreeSectors() const = 0;
    virtual int TotalSectors() const = 0;
    virtual const char *VerboseHeader ( int ) const;
    virtual void PrintVerboseInformation ( const sFileDescriptorRecord * ) const;

public:

    static cFileSystem *Open ( const char * );

    static bool IsValidName ( const char *name );
    static bool IsValidFDR ( const sFileDescriptorRecord *fdr );

    virtual void ShowDirectory ( bool ) const;
    virtual int  GetFilenames ( char *names[] ) const;

    // Functions used by cFile
    virtual sSector *GetFileSector ( sFileDescriptorRecord *FDR, int index ) = 0;
    virtual int ExtendFile ( sFileDescriptorRecord *FDR, int count ) = 0;
    virtual void TruncateFile ( sFileDescriptorRecord *FDR, int limit ) = 0;
    virtual void DiskModified () = 0;

    // Generic file system functions 
    virtual bool GetPath ( char *, size_t ) const = 0;
    virtual bool GetName ( char *, size_t ) const = 0;
    virtual bool IsValid () const = 0;
    virtual bool IsCollection () const = 0;
    virtual cFile *OpenFile ( const char * ) = 0;
    virtual cFile *CreateFile ( const char *, UCHAR, int ) = 0;
    virtual bool AddFile ( cFile * ) = 0;
    virtual bool DeleteFile ( const char * ) = 0;

};

#endif
