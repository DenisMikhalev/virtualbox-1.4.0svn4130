/*
 *  TAP-Win32 -- A kernel driver to provide virtual tap device functionality
 *               on Windows.  Originally derived from the CIPE-Win32
 *               project by Damion K. Wilson, with extensive modifications by
 *               James Yonan.
 *
 *  All source code which derives from the CIPE-Win32 project is
 *  Copyright (C) Damion K. Wilson, 2003, and is released under the
 *  GPL version 2 (see below).
 *
 *  All other source code is Copyright (C) 2002-2005 OpenVPN Solutions LLC,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//-----------------
// DEBUGGING OUTPUT
//-----------------

#define NOTE_ERROR() \
{ \
  g_LastErrorFilename = __FILE__; \
  g_LastErrorLineNumber = __LINE__; \
}


#ifdef DEBUG
#define DEBUGP(fmt) { DbgPrint fmt; }
#else
#define DEBUGP(fmt)
#endif

#if DBG

typedef struct {
  unsigned int in;
  unsigned int out;
  unsigned int capacity;
  char *text;
  BOOLEAN error;
  MUTEX lock;
} DebugOutput;

VOID MyDebugPrint (const unsigned char* format, ...);

VOID MyAssert (const unsigned char *file, int line);

VOID DumpPacket (const char *prefix,
		 const unsigned char *data,
		 unsigned int len);

VOID DumpPacket2 (const char *prefix,
		  const ETH_HEADER *eth,
		  const unsigned char *data,
		  unsigned int len);

#define CAN_WE_PRINT (DEBUGP_AT_DISPATCH || KeGetCurrentIrql () < DISPATCH_LEVEL)

#if ALSO_DBGPRINT
#define DEBUGP(fmt) { DbgPrint fmt; if (CAN_WE_PRINT) DbgPrint fmt; }
#else
#define DEBUGP(fmt) { DbgPrint fmt; }
#endif

#define MYASSERT(exp) \
{ \
  if (!(exp)) \
    { \
      MyAssert(__FILE__, __LINE__); \
    } \
}

#if DBG
#define DUMP_PACKET(prefix, data, len) \
  DumpPacket (prefix, data, len)

#define DUMP_PACKET2(prefix, eth, data, len) \
  DumpPacket2 (prefix, eth, data, len)
#else
#define DUMP_PACKET(prefix, data, len)
#define DUMP_PACKET2(prefix, eth, data, len)
#endif

#else 

#define MYASSERT(exp)
#define DUMP_PACKET(prefix, data, len)
#define DUMP_PACKET2(prefix, eth, data, len)

#endif
