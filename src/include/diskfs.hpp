//----------------------------------------------------------------------------
//
// File:        diskfs.hpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description: A class to manage the filesystem information on a TI disk.

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

#ifndef DISKFS_HPP_
#define DISKFS_HPP_

#include "fs.hpp"

struct sSector;

class cDiskMedia;

class cDiskFileSystem : public cFileSystem {

    static cDiskFileSystem *sm_SortDisk;

    cDiskMedia    *m_Media;
    VIB           *m_VIB;

protected:

    cDiskFileSystem ( const char * );
    ~cDiskFileSystem ();

    int FindFreeSector ( int start = 0 ) const;
    void SetSectorAllocation ( int index, bool bUsed );

    int FindLastSector ( sFileDescriptorRecord *FDR ) const;
    bool AddFileSector ( sFileDescriptorRecord *FDR, int );

    const sSector *FindSector ( int index ) const;
    sSector *FindSector ( int index );

    int FindFileDescriptorIndex ( const char *name );

    static int sortDirectoryIndex ( const void *ptr1, const void *ptr2 );

    int AddFileDescriptor ( const sFileDescriptorRecord *FDR );

    bool IsValidFDR ( const sFileDescriptorRecord *FDR ) const;

    // cFileSystem non-public methods
    virtual int FileCount () const;
    virtual const sFileDescriptorRecord * GetFileDescriptor ( int ) const;
    virtual int FreeSectors () const;
    virtual int TotalSectors () const;
    virtual const char *VerboseHeader ( int ) const;
    virtual void PrintVerboseInformation ( const sFileDescriptorRecord * ) const;
    virtual sSector *GetFileSector ( sFileDescriptorRecord *FDR, int index );
    virtual int ExtendFile ( sFileDescriptorRecord *FDR, int count );
    virtual void TruncateFile ( sFileDescriptorRecord *FDR, int limit );
    virtual void DiskModified ();

public:

    cDiskFileSystem ( cDiskMedia * );

    static cDiskFileSystem *Open ( const char * );

    // cFileSystem public methods
    virtual bool GetPath ( char *, size_t ) const;
    virtual bool GetName ( char *, size_t ) const;
    virtual bool IsValid () const;
    virtual bool IsCollection () const;
    virtual cFile *OpenFile ( const char * );
    virtual cFile *CreateFile ( const char *, UCHAR, int );
    virtual bool AddFile ( cFile * );
    virtual bool DeleteFile ( const char * );

};

#endif
