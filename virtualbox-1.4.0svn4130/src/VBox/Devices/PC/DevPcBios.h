/* $Id: DevPcBios.h 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * PC BIOS Device Header.
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

#ifndef DEV_PCBIOS_H
#define DEV_PCBIOS_H

#define VBOX_DMI_TABLE_ENTR          2
#define VBOX_DMI_TABLE_SIZE          130
#define VBOX_DMI_TABLE_BASE          0xe1000
#define VBOX_DMI_TABLE_VER           0x23

#define VBOX_MPS_TABLE_BASE          0xe1100

#define VBOX_LANBOOT_SEG             0xca00

#endif

