//----------------------------------------------------------------------------
//
// File:        support.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description:
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
//   15-Jan-2003    Renamed from original fileio.cpp
//
//----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if ! defined ( __WIN32__ )
  #include <unistd.h>
#endif
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

#if defined ( __GNUC__ )
    #define MKDIR(x,y) mkdir ( x, y )
    const char FILE_SEPERATOR = '/';
    const char *COMMON_PATH = "/opt/ti99sim";
#elif defined ( __OS2__ ) || defined ( __WIN32__ )
    #include <direct.h>
    #define MKDIR(x,y) mkdir ( x )
    const char FILE_SEPERATOR = '\\';
    const char *COMMON_PATH = "";
#elif defined ( __AMIGAOS__ )
    #include <direct.h>
    #define MKDIR(x,y) mkdir ( x )
    const char FILE_SEPERATOR = '/';
    const char *COMMON_PATH = "/opt/ti99sim";
#endif

static char home [ 256 ] = ".";

static int CreateHomePath ()
{
    FUNCTION_ENTRY ( NULL, "CreateHomePath", true );

    const char *ptr = getenv ( "HOME" );

    sprintf ( home, "%s%c.ti99sim", ptr ? ptr : ".", FILE_SEPERATOR );
    MKDIR ( home, 0775 );

    return 0;
}

static int  x = CreateHomePath ();

const char *HOME_PATH = home;

bool IsWriteable ( const char *filename )
{
    FUNCTION_ENTRY ( NULL, "IsWriteable", true );

    bool retVal = false;

    struct stat info;
    if ( stat ( filename, &info ) == 0 ) {
#if defined ( __WIN32__ ) || defined ( __AMIGAOS__ ) || defined ( PSP )
        if ( info.st_mode & S_IWRITE ) retVal = true;
#else
        if ( getuid () == info.st_uid ) {
            if ( info.st_mode & S_IWUSR ) retVal = true;
        } else if ( getgid () == info.st_gid ) {
            if ( info.st_mode & S_IWGRP ) retVal = true;
        } else {
            if ( info.st_mode & S_IWOTH ) retVal = true;
        }
#endif
    } else {
        // TBD: Check write permissions to the directory
        retVal = true;
    }

    return retVal;
}

static bool TryPath ( const char *path, const char *filename )
{
    FUNCTION_ENTRY ( NULL, "TryPath", true );

    char buffer [256];

    sprintf ( buffer, "%s%c%s", path, FILE_SEPERATOR, filename );

    TRACE ( "Name: " << buffer );

    FILE *file = fopen ( buffer, "rb" );
    if ( file ) fclose ( file );

    return ( file == NULL ) ? false : true;
}

const char *LocateFile ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "LocateFile", true );

    static char buffer [256];
    static char fullname[256];

    if ( filename == NULL ) return NULL;

    // Make a static copy of filename
    strcpy ( buffer, filename );

    if ( path == NULL ) {
        path = ".";
        strcpy ( fullname, filename );
    } else {
        sprintf ( fullname, "%s%c%s", path, FILE_SEPERATOR, filename );
    }

    // If we were given an absolute path, see if it's valid or not
#if defined ( __WIN32__ )
    if (( filename [0] == FILE_SEPERATOR ) || ( filename [1] == ':' )) {
#else
    if ( filename [0] == FILE_SEPERATOR ) {
#endif
        FILE *file = fopen ( filename, "rb" );
        if ( file ) fclose ( file );
        return ( file == NULL ) ? NULL : buffer;
    }

    const char *pPath = NULL;
    const char *pFile = NULL;

    // Try: CWD, CWD/path, ~/.ti99sim/path, /opt/ti99sim/path
    if (( TryPath ( ".", pFile = buffer )                   == true ) ||
        ( TryPath ( ".", pFile = fullname )                 == true ) ||
        ( TryPath ( pPath = home, pFile = fullname )        == true ) ||
        ( TryPath ( pPath = COMMON_PATH, pFile = fullname ) == true )) {
        if ( pPath == NULL ) return pFile;
        sprintf ( buffer, "%s%c%s", pPath, FILE_SEPERATOR, pFile );
        return buffer;
    }

    return NULL;
}

#if defined ( __AMIGAOS__ )

char *strdup ( const char *string )
{
    size_t length = strlen ( string );

    char *ptr = ( char * ) malloc ( length + 1 );
    memcpy ( ptr, string, length );
    ptr [length-1] = '\0';
    
    return ptr;
}

#endif

#if defined ( __AMIGAOS__ ) || ( _MSC_VER >= 1300 )

char *strndup ( const char *string, size_t max )
{
    size_t length = strlen ( string );
    if ( length > max ) length = max;

    char *ptr = ( char * ) malloc ( length + 1 );
    memcpy ( ptr, string, length );
    ptr [length-1] = '\0';
    
    return ptr;
}

#endif
