/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2020 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include "vi.h"
#include <stddef.h>

#include "clibext.h"


static vlist    GlobVars = { NULL, NULL };

/*
 * var_add - add a new variable
 */
static void var_add( const char *name, const char *val, vlist *vl, bool glob )
{
    vars        *new, *curr;
    var_len     len;
    size_t      name_len;

    /*
     * see if we can just update an existing copy
     */
    len = strlen( val );
    for( curr = vl->head; curr != NULL; curr = curr->next ) {
        if( strcmp( curr->name, name ) == 0 ) {
            ReplaceString( &curr->value, val );
            curr->len = len;
#ifndef VICOMP
            if( glob && !EditFlags.CompileAssignmentsDammit ) {
                EditFlags.CompileAssignments = false;
            }
#endif
            return;
        }
    }

    /*
     * create and add a new variable
     */
    name_len = strlen( name );
    new = MemAlloc( offsetof( vars, name ) + name_len + 1 );
    memcpy( new->name, name, name_len + 1 );
    new->value = DupString( val );
    new->len = len;

    if( glob ) {
#ifndef VICOMP
        EditFlags.CompileAssignments = false;
#endif
    }
    AddLLItemAtEnd( (ss **)&vl->head, (ss **)&vl->tail, (ss *)new );

} /* var_add */

#ifndef VICOMP
/*
 * GlobVarAddStr
 */
void GlobVarAddStr( const char *name, const char *val )
{
    var_add( name, val, &GlobVars, true );

} /* GlobVarAddStr */


/*
 * GlobVarAddRowAndCol - add row and column vars
*/
void GlobVarAddRowAndCol( void )
{
    int vc;
    int len;

    if( CurrentLine == NULL ) {
        len = 0;
    } else {
        len = CurrentLine->len;
    }

    GlobVarAddLong( "R", CurrentPos.line );
    GlobVarAddLong( "Linelen", len );
    vc = VirtualColumnOnCurrentLine( CurrentPos.column );
    GlobVarAddLong( "C", vc );
    // VarDump( );

} /* GlobVarAddRowAndCol */

/*
 * SetModifiedVar - set the modified variable
 */
void SetModifiedVar( bool val )
{
    GlobVarAddLong( "M", val );

} /* SetModifiedVar */

/*
 * GlobVarAddLong
 */
void GlobVarAddLong( const char *name, long val )
{
    char ibuff[MAX_NUM_STR];

    var_add( name, ltoa( val, ibuff, 10 ), &GlobVars, true );

} /* GlobVarAddLong */

/*
 * VarAddLong
 */
void VarAddLong( const char *name, long val, vlist *vl )
{
    char ibuff[MAX_NUM_STR];

    VarAddStr( name, ltoa( val, ibuff, 10 ), vl );

} /* VarAddLong */
#endif /* VICOMP */

/*
 * VarAddStr - add a new variable
 */
void VarAddStr( const char *name, const char *val, vlist *vl )
{
    bool        glob;

    /*
     * check local/global setting
     */
    if( IS_LOCALVAR( name ) ) {
        if( vl == NULL )
            /* error variable list must be defined */
            return;
        glob = false;
    } else {
        vl = &GlobVars;
        glob = true;
    }
    var_add( name, val, vl, glob );

} /* VarAddStr */

/*
 * VarListDelete - delete a local variable list
 */
void VarListDelete( vlist *vl )
{
    vars *curr, *next;

    for( curr = vl->head; curr != NULL; curr = next ) {
        next = curr->next;
        MemFree( curr->value );
        MemFree( curr );
    }

} /* VarListDelete */

#ifndef VICOMP
/*
 * VarName - parse a variable name of the form %(foo)
 */
bool VarName( char *new, const char *name, vlist *vl )
{
    char    tmp[MAX_SRC_LINE];
    size_t  len;

    if( name[0] != '%' || name[1] == '\0' ) {
        return( false );
    }
    ++name;
    len = strlen( name );
    if( name[0] == '(' ) {
        ++name;
        len -= 2;
    }
    memcpy( tmp, name, len );
    tmp[len] = '\0';
    if( strchr( tmp, '%' ) != NULL ) {
        Expand( new, tmp, vl );
    } else {
        strcpy( new, tmp );
    }
    return( true );

} /* VarName */

/*
 * VarFind - locate data for a specific variable name
 */
vars * VarFind( const char *name, vlist *vl )
{
    vars        *curr;

    /*
     * search locals
     */
    if( IS_LOCALVAR( name ) ) {
        if( vl == NULL ) {
            return( NULL );
        }
    } else {
        vl = &GlobVars;
    }

    /*
     * search globals
     */
    for( curr = vl->head; curr != NULL; curr = curr->next ) {
        if( strcmp( name, curr->name ) == 0 ) {
            return( curr );
        }
    }
    return( NULL );

} /* VarFind */


/* Free the globals */
void GlobVarFini( void )
{
    VarListDelete( &GlobVars );
}

#if 0
void VarDump( void ) {
    vars        *curr;
    int         count = 0;
    FILE        *f = fopen( "C:\\vi.out", "a+t" );

    for( curr = GlobVars.head; curr != NULL; curr = curr->next ) {
        // fprintf( f,"N:%s V:%s %x\n", curr->name, curr->value, curr->next );
        count++;
    }
    if( count == 13 ) {
        count = 13;
    }
    fprintf( f, "count %d\n", count );
    fclose( f );
}

void VarSC( char *str )
{
    /// DEBUG BEGIN
    {
        vars    *currn = GlobVars.head;
        if( currn != NULL ) {
            while( currn->next != NULL ) {
                currn = currn->next;
            }
            if( GlobVars.tail != currn ) {
                printf( "%s\n", str );
            }
        }
    }
    /// DEBUG END
}
#endif

#endif /* VICOMP */
