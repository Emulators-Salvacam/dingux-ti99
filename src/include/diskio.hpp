//----------------------------------------------------------------------------
//
// File:        diskio.hpp
// Date:        14-Jul-2001
// Programmer:  Marc Rousseau
//
// Description: A class to manipulte disk images
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

#ifndef DISKIO_HPP_
#define DISKIO_HPP_

#include "cBaseObject.hpp"

#define DEFAULT_SECTOR_SIZE     256

#define MAX_SECTORS             20
#define MAX_TRACKS              80
#define MAX_TRACKS_LO           40
#define MAX_TRACKS_HI           80

#define TRACK_SIZE_FM           3177
#define TRACK_SIZE_MFM          6450

#define MAX_TRACK_SIZE          7000

#define INDEX_GAP_FM            16
#define INDEX_GAP_MFM           40

#define SYNC_BYTES_FM           6
#define SYNC_BYTES_MFM          12

#define ID_DATA_GAP_FM          11
#define ID_DATA_GAP_MFM         22

#define DATA_ID_GAP_FM          45
#define DATA_ID_GAP_MFM         24

#define EOT_FILLER_FM           276
#define EOT_FILLER_MFM          736

/*
  IBM System 3740 - Single Density

  GAP4a SYNC IAM GAP1 SYNC IDAM C H S N CC GAP2 SYNC DataAM       CC
   40x   6x       26x  6x       Y D E O RR  11x  6x   FB    Data  RR  GAP3  GAP4b
   FF    00   FC  FF   00   FE  L   C   CC  FF   00   F8          CC


  IBM System 34 - Double Density

  GAP4a SYNC  IAM  GAP1 SYNC IDAM   C H S N CC GAP2 SYNC DataAM        CC
   80x   12x 3x     50x  12x 3x     Y D E O RR  22x  12x 3x FB   Data  RR  GAP3  GAP4b
   4E    00  C2 FC  4E   00  A1 FE  L   C   CC  4e   00  A1 F8         CC


  TI-DISK ROM Defaults

  GAP1  SYNC IAM C H S N CC GAP2 SYNC DataAM      CC  GAP3  GAP4b
   16x   6x      Y D E O RR  11x  6x         Data RR   45x   231x
   00    00   FE L   C   CC  FF   00   FB         CC   FF    FF


Index Gap              12   >FF

ID Sync Bytes           6   >00
ID Address Mark         1   >FE
Track Address           1   Track #       - >00 - >27 (0-39)
Side Number             1   Side #        - >00 - >01
Sector Address          1   Sector #      - >00 - >08
Sector Length           1   >01           - >01 = 256
ID CRC                  2   ID CRC Value

ID/Data Seperator      11   >FF

Data Sync Bytes         6   >00
Data Address Mark       1   >FB
Data                  256   File Data
Data CRC                2   Data CRC Value

Data/ID Seperator      36   >FF

End of Track Filler   240   >FF



Index Gap              32   >4E

ID Sync Bytes          12   >00
                        3   >A1
ID Address Mark         1   >FE
Track Address           1   Track #       - >00 - >27 (0-39)
Side Number             1   Side #        - >00 - >01
Sector Address          1   Sector #      - >00 - >11
Sector Length           1   >01           - >01 = 256
ID CRC                  2   ID CRC Value

ID/Data Seperator      11   >4E

Data Sync Bytes        12   >00
                        3   >A1
Data Address Mark       1   >FB
Data                  256   File Data
Data CRC                2   Data CRC Value

Data/ID Seperator      28   >4E

End of Track Filler    190  >4E
*/

//----------------------------------------------------------------------------
//     +------+------+------+------+------+------+----------+
//     | ACYL | ASID | LCYL | LSID | LSEC | LLEN |  COUNT   |
//     +------+------+------+------+------+------+----------+
//
//      ACYL    Actual cylinder, 1 byte
//      ASID    Actual side, 1 byte
//      LCYL    Logical cylinder; cylinder as read, 1 byte
//      LSID    Logical side; or side as read, 1 byte
//      LSEC    Sector number as read, 1 byte
//      LLEN    Length code as read, 1 byte
//      COUNT   Byte count of data to follow, 2 bytes.  If zero,
//              no data is contained in this sector.
//
// All sectors occurring on a side will be grouped together;
// however, they will appear in the same order as they occurred on
// the diskette.  Therefore, if an 8 sector-per-track diskette were
// scanned which had a physical interleave of 2:1, the sectors might
// appear in the order 1,5,2,6,3,7,4,8 in the DOS dump file.
//
//----------------------------------------------------------------------------

enum eDiskFormat {
    FORMAT_UNKNOWN,
    FORMAT_RAW_TRACK,       // Image is an array of tracks - PC99
    FORMAT_RAW_SECTOR,      // Image is an array of sectors - v9t9
    FORMAT_ANADISK,         // Image is an array of headers+sectors
    FORMAT_MAX
};

enum eDiskDensity {
    DENSITY_UNKNOWN,
    DENSITY_SINGLE,         // FM disk format
    DENSITY_DOUBLE,         // MFM disk format
    DENSITY_MIXED,          // Combination of FM & MFM tracks
    DENSITY_MAX
};

struct sAnadiskHeader {
    UCHAR          ActualCylinder;
    UCHAR          ActualSide;
    UCHAR          LogicalCylinder;
    UCHAR          LogicalSide;
    UCHAR          LogicalSector;
    UCHAR          Length;
    USHORT         DataCount;
};

struct sSector {
    int            LogicalCylinder;
    int            LogicalSide;
    int            LogicalSector;
    int            Size;
    UCHAR         *Data;
};

struct sTrack {
    eDiskDensity   Density;
    sSector        Sector [ MAX_SECTORS ];
    int            Size;
    UCHAR         *Data;
};

class cDiskMedia : public cBaseObject {

    bool           m_HasChanged;
    bool           m_IsWriteProtected;
    char          *m_FileName;
    eDiskFormat    m_Format;
    int            m_MaxHeads;
    int            m_MaxTracks;
    int            m_NumHeads;
    int            m_NumTracks;
    UCHAR         *m_RawData;
    sTrack         m_Track [ 2 ][ MAX_TRACKS ];

    static eDiskFormat DetermineFormat ( FILE * );
    static UCHAR *FindAddressMark ( UCHAR, UCHAR, eDiskDensity, UCHAR *, UCHAR * );
    static UCHAR *FindEndOfTrack ( eDiskDensity, UCHAR, int, UCHAR *, UCHAR * );

    void AllocateTracks ( int, int );

    void FormatTrack ( int, int, eDiskDensity, int, sSector * );
    void FormatDisk ( int, int, eDiskDensity );

    bool ReadDiskRawTrack ( FILE * );
    bool ReadDiskRawSector ( FILE * );
    bool ReadDiskAnadisk ( FILE * );

    bool LoadFile ();

    bool IsValidRawSector ();

    bool SaveDiskRawTrack ( FILE * );
    bool SaveDiskRawSector ( FILE * );
    bool SaveDiskAnadisk ( FILE * );

protected:

    virtual ~cDiskMedia ();

public:

    cDiskMedia ( const char * );
    cDiskMedia ( int = MAX_TRACKS, int = 2 );

    void DiskModified ()            { m_HasChanged = true; }
    bool HasChanged () const        { return m_HasChanged; }
    bool IsWriteProtected () const  { return m_IsWriteProtected; }

    eDiskFormat GetFormat () const;

    int NumTracks () const	    { return m_NumTracks; }
    int NumSides () const	    { return m_NumHeads; }

    const char *GetName () const    { return m_FileName; }
    void SetName ( const char * );

    void ClearDisk ();

    bool LoadFile ( const char * );
    bool SaveFile ( eDiskFormat = FORMAT_UNKNOWN, bool = false );

    sTrack  *GetTrack ( int, int );
    sSector *GetSector ( int, int, int, int = -1 );

    void WriteSector ( int, int, int, int, void * );
    void WriteTrack ( int, int, int, UCHAR * );
};

#endif
