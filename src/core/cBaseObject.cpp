//----------------------------------------------------------------------------
//
// File:	cBaseObject.cpp
// Name:	
// Programmer:	Marc Rousseau
// Date:      	13-April-1998
//
// Description:	
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include <typeinfo>

#include "common.hpp"
#include "logger.hpp"
#include "iBaseObject.hpp"
#include "cBaseObject.hpp"

DBG_REGISTER ( __FILE__ );

#ifdef DEBUG

    ULONG cBaseObject::sm_ActiveObjects;

    static void Check ()
    {
        FUNCTION_ENTRY ( NULL, "Check", true );

        ULONG count = cBaseObject::CountObjects ();

        if ( count != 0 ) {
            ERROR ( "There " << (( count == 1 ) ? "is" : "are" ) << " still " << count << " active cBaseObject" << (( count != 1 ) ? "s" : "" ));
        }
    }

    static int x = atexit ( Check );
//    #pragma exit Check 64

#endif

cBaseObject::cBaseObject ( const char *name ) :
#ifdef DEBUG
    m_Tag ( BASE_OBJECT_VALID ),
    m_ObjectName ( name ),
    m_ObjectCount ( 0 ),
    m_OwnerList ( 8 ),
#endif
    m_RefCount ( 1 )
{
    FUNCTION_ENTRY ( this, "cBaseObject ctor", true );

    TRACE ( "New " << name << " object: " << this << " (" << ( iBaseObject * ) this << ") " << typeid(this).name ());

#ifdef DEBUG
    ASSERT ( name != NULL );

    sm_ActiveObjects++;
#else
    UNREFERENCED_PARAMETER ( name );
#endif
}

cBaseObject::~cBaseObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject dtor", true );

    if ( ASSERT_BOOL ( IsValid ())) return;
    
#ifdef DEBUG

    // By now the derived class should have released all referenced objects
    if ( m_ObjectCount > 0 ) {
        ERROR ( m_ObjectName << " object " << ( void * ) this << " still owns " << m_ObjectCount << " object" << (( m_ObjectCount != 1 ) ? "s" : "" ));
    }

    // We should only get here through Release, but make sure anyway
    if ( m_RefCount > 0 ) {
        ERROR ( m_ObjectName << " object " << ( void * ) this << " has RefCount of " << m_RefCount );
        for ( ULONG i = 0; i < m_RefCount; i++ ) {
            cBaseObject *obj = m_OwnerList [i];
            WARNING ( "  Releasing reference from " << ( obj ? obj->ObjectName () : "NULL" ) << " object " << ( void * ) obj );
            Release ( obj );
        }
    }

////    delete [] m_OwnerList;
////    m_OwnerList = NULL;

    m_Tag         = BASE_OBJECT_INVALID;
    m_ObjectName  = "<Deleted>";
    m_ObjectCount = 0;
////    m_ListMax     = 0;

    sm_ActiveObjects--;

#endif

    m_RefCount    = 0;

}

#ifdef DEBUG

bool cBaseObject::CheckValid () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::CheckValid", true );

    // We don't do anything more here - let derived classes to their thing
    return true;
}

bool cBaseObject::IsValid () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::IsValid", true );

    // Make sure we have write access to 'this' for starters
/*
    if ( IsBadWritePtr (( void * ) this, sizeof ( cBaseObject ))) return false;
*/

    // Wrap all member variable access inside the excpetion handler
    try {
        // See if we have a valid tag
        if ( m_Tag != BASE_OBJECT_VALID ) return false;

        // OK, it looks like one of ours, try using the virtual CheckValid method
        return CheckValid ();
    }
    catch ( ... ) {
    }

    return false;
}

ULONG cBaseObject::CountObjects ()
{
    FUNCTION_ENTRY ( NULL, "cBaseObject::CountObjects", true );

    return sm_ActiveObjects;
}

void cBaseObject::AddObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddObject", true );

    if ( ASSERT_BOOL ( IsValid ())) return;

    m_ObjectCount++;
}

void cBaseObject::RemoveObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject::RemoveObject", true );

    if ( ASSERT_BOOL ( IsValid ())) return;
    if ( ASSERT_BOOL ( m_ObjectCount >= 0 )) return;

    m_ObjectCount--;
}

void cBaseObject::AddOwner ( cBaseObject *owner )
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddOwner", true );

    if ( ASSERT_BOOL ( IsValid ())) return;

    m_OwnerList.push_back ( owner );

    if ( owner != NULL ) owner->AddObject ();
}

void cBaseObject::RemoveOwner ( cBaseObject *owner )
{
    FUNCTION_ENTRY ( this, "cBaseObject::RemoveOwner", true );

    if ( ASSERT_BOOL ( IsValid ())) return;

#if defined ( _MSC_VER )
    tBaseObjectList::iterator ptr = m_OwnerList.begin ();
    while ( ptr != m_OwnerList.end ()) {
        if ( *ptr == owner ) break;
        ptr++;
    }
#else
    tBaseObjectList::iterator ptr = find ( m_OwnerList.begin (), m_OwnerList.end (), owner );
#endif
    if ( ptr != m_OwnerList.end ()) {
        if ( owner != NULL ) owner->RemoveObject ();
        m_OwnerList.erase ( ptr );
        return;
    }

    ERROR (( owner ? owner->ObjectName () : "NULL" ) << " object " << ( void * ) owner << " does not own " << m_ObjectName << " object " << ( void * ) this );
}

const char *cBaseObject::ObjectName () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::ObjectName", true );
    
    return m_ObjectName;
}

#endif

bool cBaseObject::GetInterface ( const char *iName, iBaseObject **pObject )
{
    FUNCTION_ENTRY ( this, "cBaseObject::GetInterface", true );

    if ( strcmp ( iName, "iBaseObject" ) == 0 ) {
        *pObject = ( iBaseObject * ) this;
        return true;
    }

    return false;
}

int cBaseObject::AddRef ( iBaseObject *obj )
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddRef", true );

    if ( ASSERT_BOOL ( IsValid ())) return -1;
    if ( ASSERT_BOOL ( m_RefCount >= 0 )) return -1;

#ifdef DEBUG
    try {
        cBaseObject *base = dynamic_cast <cBaseObject *> ( obj );
        AddOwner ( base );
    }
    catch ( const std::bad_cast &er ) {
        FATAL ( "Invalid cast: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( const std::exception &er ) {
        FATAL ( "Exception: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( ... ) {
        FATAL ( "Unexpected C++ exception!" );
    }
#else
    UNREFERENCED_PARAMETER ( obj );
#endif

    return ++m_RefCount;
}

int cBaseObject::Release ( iBaseObject *obj )
{
    FUNCTION_ENTRY ( this, "cBaseObject::Release", true );

    if ( ASSERT_BOOL ( IsValid ())) return -1;
    if ( ASSERT_BOOL ( m_RefCount > 0 )) return -1;

#ifdef DEBUG
    try {
        cBaseObject *base = dynamic_cast <cBaseObject *> ( obj );
        RemoveOwner ( base );
    }
    catch ( const std::bad_cast &er ) {
        FATAL ( "Invalid cast: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( const std::exception &er ) {
        FATAL ( "Exception: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( ... ) {
        FATAL ( "Unexpected C++ exception!" );
    }
#else
    UNREFERENCED_PARAMETER ( obj );
#endif

    if ( --m_RefCount == 0 ) {
        delete this;
        return 0;
    }

    return m_RefCount;
}
