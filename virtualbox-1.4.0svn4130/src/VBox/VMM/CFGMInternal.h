/* $Id: CFGMInternal.h 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * CFGM - Internal header file.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___CFGMInternal_h
#define ___CFGMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>


/** @defgroup grp_cfgm_int Internals.
 * @ingroup grp_cfgm
 * @{
 */


/**
 * Configuration manager propertype value.
 */
typedef union CFGMVALUE
{
    /** Integer value. */
    struct CFGMVALUE_INTEGER
    {
        /** The integer represented as 64-bit unsigned. */
        uint64_t        u64;
    } Integer;

    /** String value. (UTF-8 of course) */
    struct CFGMVALUE_STRING
    {
        /** Length of string. (In bytes, including the terminator.) */
        RTUINT          cch;
        /** Pointer to the string. */
        char           *psz;
    } String;

    /** Byte string value. */
    struct CFGMVALUE_BYTES
    {
        /** Length of byte string. (in bytes) */
        RTUINT          cb;
        /** Pointer to the byte string. */
        uint8_t        *pau8;
    } Bytes;
} CFGMVALUE;
/** Pointer to configuration manager property value. */
typedef CFGMVALUE *PCFGMVALUE;


/**
 * Configuration manager tree node.
 */
typedef struct CFGMLEAF
{
    /** Pointer to the next leaf. */
    PCFGMLEAF       pNext;
    /** Pointer to the previous leaf. */
    PCFGMLEAF       pPrev;

    /** Property type. */
    CFGMVALUETYPE   enmType;
    /** Property value. */
    CFGMVALUE       Value;

    /** Name length. (exclusive) */
    RTUINT          cchName;
    /** Name. */
    char            szName[1];
} CFGMLEAF;


/**
 * Configuration manager tree node.
 */
typedef struct CFGMNODE
{
    /** Pointer to the next node (on this level). */
    PCFGMNODE       pNext;
    /** Pointer to the previuos node (on this level). */
    PCFGMNODE       pPrev;
    /** Pointer Parent node. */
    PCFGMNODE       pParent;
    /** Pointer to first child node. */
    PCFGMNODE       pFirstChild;
    /** Pointer to first property leaf. */
    PCFGMLEAF       pFirstLeaf;

    /** Pointer to the VM owning this node. */
    PVM             pVM;

    /** The root of a 'restricted' subtree, i.e. the parent is
     * invisible to non-trusted users.
     */
    bool            fRestrictedRoot;

    /** Name length. (exclusive) */
    RTUINT          cchName;
    /** Name. */
    char            szName[1];
} CFGMNODE;




/**
 * Converts a CFGM pointer into a VM pointer.
 * @returns Pointer to the VM structure the CFGM is part of.
 * @param   pCFGM   Pointer to CFGM instance data.
 */
#define CFGM2VM(pCFGM)  ( (PVM)((char*)pCFGM - pCFGM->offVM) )

/**
 * CFGM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct CFGM
{
    /** Offset to the VM structure.
     * See CFGM2VM(). */
    RTUINT                  offVM;
    /** Alignment padding. */
    RTUINT                  uPadding0;

    /** Pointer to root node. */
    HCPTRTYPE(PCFGMNODE)    pRoot;
} CFGM;

/** @} */

#endif
