/* $Id: avlhcphys.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - AVL tree, RTHCPHYS, unique keys.
 */

/*
 * Copyright (C) 2006-2007 knut st. osmundsen (bird-src-spam@anduin.net)
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef NOFILEID
static const char szFileId[] = "Id: kAVLULInt.c,v 1.4 2003/02/13 02:02:38 bird Exp $";
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/*
 * AVL configuration.
 */
#define KAVL_FN(a)                  RTAvlHCPhys##a
#define KAVL_MAX_STACK              27  /* Up to 2^24 nodes. */
#define KAVL_CHECK_FOR_EQUAL_INSERT 1   /* No duplicate keys! */
#define KAVLNODECORE                AVLHCPHYSNODECORE
#define PKAVLNODECORE               PAVLHCPHYSNODECORE
#define PPKAVLNODECORE              PPAVLHCPHYSNODECORE
#define KAVLKEY                     RTHCPHYS
#define PKAVLKEY                    PRTHCPHYS
#define KAVLENUMDATA                AVLHCPHYSENUMDATA
#define PKAVLENUMDATA               PAVLHCPHYSENUMDATA
#define PKAVLCALLBACK               PAVLHCPHYSCALLBACK


/*
 * AVL Compare macros
 */
#define KAVL_G( key1, key2)         ( (key1) >  (key2) )
#define KAVL_E( key1, key2)         ( (key1) == (key2) )
#define KAVL_NE(key1, key2)         ( (key1) != (key2) )


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/avl.h>
#include <iprt/assert.h>

/*
 * Include the code.
 */
#define SSToDS(ptr) ptr
#define KMAX        RT_MAX
#define kASSERT     Assert
#include "avl_Base.cpp.h"
#include "avl_Get.cpp.h"
#include "avl_DoWithAll.cpp.h"
#include "avl_GetBestFit.cpp.h"
#include "avl_RemoveBestFit.cpp.h"
#include "avl_Destroy.cpp.h"

