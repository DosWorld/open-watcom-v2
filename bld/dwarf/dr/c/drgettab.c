/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2022 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  DWARF file table utility functions.
*
****************************************************************************/


#include "drpriv.h"
#include "drutils.h"
#include "walloca.h"
#include "drgettab.h"

#include "clibext.h"


// these are the increments in which the dynamically growing tables will
// grow. NOTE that the code relies on VBL_ARRAY_DELTA being a power of 2

#define VBL_ARRAY_DELTA     0x10
#define VBL_ARRAY_MASK      (VBL_ARRAY_DELTA - 1)

file_info               FileNameTable;

static bool GrabLineAddr( drmem_hdl abbrev, drmem_hdl mod, mod_scan_info *x, void *data )
/***************************************************************************************/
/* this is called by ScanCompileUnit with abbrevptr and dataptr pointing at
 * the start of a compile unit die.  This picks out the line number info.
 * offset, and stores it in data */
{
    /* unused parameters */ (void)x;

    if( DWRScanForAttrib( &abbrev, &mod, DW_AT_stmt_list ) ) {
        *((unsigned_32 *)data) = DWRReadConstant( abbrev, mod );
    }
    return( false );    // do not continue with the search.
}

void DWRInitFileTable( file_table *tab )
/**************************************/
{
    tab->len = 0;
    tab->tab = DWRALLOC( VBL_ARRAY_DELTA * sizeof( filetab_entry ) );
}

void DWRFiniFileTable( file_table *tab, bool freenames )
/******************************************************/
{
    filetab_idx     ftidx;

    if( freenames ) {
        for( ftidx = 0; ftidx < tab->len; ftidx++ ) {
            DWRFREE( tab->tab[ftidx].u.name );
        }
    }
    if( tab->tab != NULL ) {
        DWRFREE( tab->tab );
    }
}

static void GrowTable( file_table *tab )
/**************************************/
{
    tab->len++;
    if( !(tab->len & VBL_ARRAY_MASK) ) {        // it will overflow
        tab->tab = DWRREALLOC( tab->tab,
                   (tab->len + VBL_ARRAY_DELTA) * sizeof(filetab_entry) );
    }
}

static filetab_idx DWRAddFileName( char *name, file_table *tab )
/**************************************************************/
{
    filetab_idx     ftidx;
    char            **names;

    names = (char **)tab->tab;
    for( ftidx = 0; ftidx < tab->len; ++ftidx ) {
        if( strcmp( name, *names ) == 0 ) {
            DWRFREE( name );
            return( ftidx );
        }
        names++;
    }
    GrowTable( tab );
    tab->tab[ftidx].u.name = name;
    return( ftidx );
}

static void DWRInsertIndex( filetab_idx ftidx, file_table *tab, unsigned where )
/******************************************************************************/
{
    if( where == TAB_IDX_FNAME ) {
        tab->tab[tab->len - 1].u.idx.fnameidx = ftidx;
    } else {
        tab->tab[tab->len - 1].u.idx.pathidx = ftidx;
    }
}

static void DWRAddIndex( filetab_idx ftidx, file_table *tab, unsigned where )
/***************************************************************************/
{
    GrowTable( tab );
    DWRInsertIndex( ftidx, tab, where );
}

static filetab_idx DWRIndexPath( dr_fileidx pathidx, file_table *tab )
/********************************************************************/
{
    return( tab->tab[pathidx].u.idx.pathidx );
}

filetab_idx DWRIndexFile( dr_fileidx fileidx, file_table *tab )
/*************************************************************/
{
    return( tab->tab[fileidx].u.idx.fnameidx );
}

char * DWRIndexFileName( filetab_idx ftidx, file_table *tab )
/***********************************************************/
{
    return( tab->tab[ftidx].u.name );
}

static void DWRTrimTableSize( file_table *tab )
/*********************************************/
{
    tab->tab = DWRREALLOC( tab->tab, tab->len * sizeof(filetab_entry) );
}

static void ReadNameEntry( drmem_hdl *start, file_info *nametab,
                           file_table *idxtab, file_table *maptab )
/*****************************************************************/
{
    char            *name;
    filetab_idx     ftidx;
    dr_fileidx      pathidx;

    name = DWRVMCopyString( start );
    ftidx = DWRAddFileName( name, &nametab->fnametab );
    DWRAddIndex( ftidx, idxtab, TAB_IDX_FNAME );
    pathidx = DWRVMReadULEB128( start );
    ftidx = DWRIndexPath( pathidx, maptab );
    DWRInsertIndex( ftidx, idxtab, TAB_IDX_PATH );
    DWRVMSkipLEB128( start );   // skip time
    DWRVMSkipLEB128( start );   // skip length
}

void DWRScanFileTable( drmem_hdl start, file_info *nametab, file_table *idxtab )
/******************************************************************************/
// find the filenames in the line information, and return them in a table
{
    drmem_hdl       finish;
    file_table      curridxmap;
    char            *name;
    int             index;
    filetab_idx     ftidx;
    unsigned        length;
    unsigned_8      *oparray;
    int             op_base;
    int             value;
    unsigned_32     stmt_offset;
    dw_lns          value_lns;
    dw_lne          value_lne;

    stmt_offset = (unsigned_32)-1;
    DWRGetCompileUnitHdr( start, GrabLineAddr, &stmt_offset );
    if( stmt_offset == (unsigned_32)-1 ) {
        return;
    }
    start = DWRCurrNode->sections[DR_DEBUG_LINE].base + stmt_offset;
    finish = start + DWRVMReadDWord( start );
    op_base = DWRVMReadByte( start + offsetof( stmt_prologue, opcode_base ) );
    start += offsetof( stmt_prologue, standard_opcode_lengths );
    oparray = walloca( op_base - 1 );
    for( index = 0; index < op_base - 1; index++ ) {
        oparray[index] = DWRVMReadByte( start );
        start++;
    }
    DWRInitFileTable( &curridxmap );
    while( start < finish ) {           // get directory table
        value = DWRVMReadByte( start );
        if( value == 0 ) {
            start++;
            break;
        }
        name = DWRVMCopyString( &start );
        ftidx = DWRAddFileName( name, &nametab->pathtab );
        DWRAddIndex( ftidx, &curridxmap, TAB_IDX_PATH );
    }
    while( start < finish ) {           // get filename table
        value = DWRVMReadByte( start );
        if( value == 0 ) {
            start++;
            break;
        }
        ReadNameEntry( &start, nametab, idxtab, &curridxmap );
    }
    while( start < finish ) {   // now go through the statement program
        value_lns = DWRVMReadByte( start );
        start++;
        if( value_lns == 0 ) {      // it's an extended opcode
            length = DWRVMReadULEB128( &start );
            value_lne = DWRVMReadByte( start );
            if( value_lne == DW_LNE_define_file ) {
                start++;
                ReadNameEntry( &start, nametab, idxtab, &curridxmap );
            } else {
                start += length;
            }
        } else if( value_lns < op_base ) {  // it is a standard opcode
            if( value_lns == DW_LNS_fixed_advance_pc ) {
                start += sizeof( unsigned_16 );    // it is a fixed size
            } else {    // it is a variable # of blocks
                for( value = oparray[value_lns - 1]; value > 0; --value ) {
                    DWRVMSkipLEB128( &start );
                }
            }
        }       // else it was a special op, and thus only 1 byte long
    }
    DWRTrimTableSize( idxtab );
    DWRFiniFileTable( &curridxmap, false );
}

char * DWRFindFileName( dr_fileidx fileidx, drmem_hdl entry )
/***********************************************************/
{
    compunit_info   *compunit;
    filetab_idx     ftidx;

    if( fileidx != 0 ) {
        compunit = DWRFindCompileInfo( entry );
        ftidx = DWRIndexFile( fileidx - 1, &compunit->filetab );
        return( DWRIndexFileName( ftidx, &FileNameTable.fnametab ) );
    }
    return( NULL );
}
