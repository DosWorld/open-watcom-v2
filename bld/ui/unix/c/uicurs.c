/****************************************************************************
*
*                            Open Watcom Project
*
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


#include "uidef.h"
#include "uiattrs.h"
#include "uivirts.h"
#include "uicurshk.h"


#define _swap(a,b)      {int i; i=a; a=b; b=i;}

static ORD              OldCursorRow;
static ORD              OldCursorCol;
static CURSOR_TYPE      OldCursorType;

void UIAPI uioffcursor( void )
/*****************************/
{
    UIData->cursor_type = C_OFF;
    if( UIData->cursor_on ) {
        UIData->cursor_on = false;
        _physupdate( NULL );
    }
}


void UIAPI uioncursor( void )
/****************************/
{
    if( !UIData->cursor_on ) {
        UIData->cursor_on = true;
        _physupdate( NULL );
    }
}


static void savecursor( void )
/****************************/
{
    CATTR   dummy;

    _uigetcursor( &OldCursorRow, &OldCursorCol, &OldCursorType, &dummy );
    UIData->cursor_on = true;
}


void intern newcursor( void )
/***************************/
{
    if( UIData->cursor_type == C_OFF ) {
        uioffcursor();
    } else {
        uioncursor();
    }
}


static void swapcursor( void )
/****************************/
{
    _swap( UIData->cursor_type, OldCursorType );
    _swap( UIData->cursor_col, OldCursorCol );
    _swap( UIData->cursor_row, OldCursorRow );
    UIData->cursor_on = true;
}


void UIAPI uigetcursor( ORD *row, ORD *col, CURSOR_TYPE *type, CATTR *attr )
/**************************************************************************/
{
    _uigetcursor( row, col, type, attr );
}


void UIAPI uisetcursor( ORD row, ORD col, CURSOR_TYPE typ, CATTR attr )
/*********************************************************************/
{
    _uisetcursor( row, col, typ, attr );
}


void UIAPI uiswapcursor( void )
/******************************/
{
    swapcursor();
    newcursor();
}


void UIAPI uiinitcursor( void )
/******************************/
{
    UIData->cursor_row = (ORD)-1;
    UIData->cursor_col = (ORD)-1;
    UIData->cursor_type = C_OFF;
    savecursor();
    _uisetcursor( OldCursorRow, OldCursorCol, OldCursorType, 0 );
    uioffcursor();
}


void UIAPI uifinicursor( void )
/******************************/
{
    UIData->cursor_row = 0;
    UIData->cursor_col = 0;
    UIData->cursor_type = C_NORMAL;
    _physupdate( NULL );
}
