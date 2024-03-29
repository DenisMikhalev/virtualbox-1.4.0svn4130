/* $Id: tstLdrDisasmTest.cpp 4071 2007-08-07 17:07:59Z vboxsync $ */
/** @file
 * innotek Portable Runtime - RTLdr test object.
 *
 * We use precompiled versions of this object for testing all the loaders.
 *
 * This is not supposed to be pretty or usable code, just something which
 * make life difficult for the loader.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <iprt/string.h>

#if defined(IN_RING0) && !defined(RT_OS_WINDOWS) /* Too lazy to make import libs. */
extern "C" DECLIMPORT(int) MyPrintf(const char *pszFormat, ...);
# define MY_PRINTF(a) MyPrintf a
#else
# define MY_PRINTF(a) do {} while (0)
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/* 32-bit code */
static const uint8_t g_ab32BitCode[] =
{
    0x55,               // 1000ab50 55              push    ebp
    0x8b,0xec,          // 1000ab51 8bec            mov     ebp,esp
    0x8b,0x45,0x08,     // 1000ab53 8b4508          mov     eax,dword ptr [ebp+8]
    0x81,0x38,0x07,0x07,// 1000ab56 813807076419    cmp     dword ptr [eax],19640707h
    0x64,0x19,
    0x75,0x09,          // 1000ab5c 7509            jne     kLdr!kLdrModMap+0x17 (1000ab67)
    0x8b,0x4d,0x08,     // 1000ab5e 8b4d08          mov     ecx,dword ptr [ebp+8]
    0x83,0x79,0x2c,0x00,// 1000ab61 83792c00        cmp     dword ptr [ecx+2Ch],0
    0x75,0x07,          // 1000ab65 7507            jne     kLdr!kLdrModMap+0x1e (1000ab6e)
    0xb8,0xc0,0x68,0x06,// 1000ab67 b8c0680600      mov     eax,668C0h
    0x00,
    0xeb,0x14,          // 1000ab6c eb14            jmp     kLdr!kLdrModMap+0x32 (1000ab82)
    0x33,0xd2,          // 1000ab6e 33d2            xor     edx,edx
    0x75,0xe1,          // 1000ab70 75e1            jne     kLdr!kLdrModMap+0x3 (1000ab53)
    0x8b,0x45,0x08,     // 1000ab72 8b4508          mov     eax,dword ptr [ebp+8]
    0x50,               // 1000ab75 50              push    eax
    0x8b,0x4d,0x08,     // 1000ab76 8b4d08          mov     ecx,dword ptr [ebp+8]
    0x8b,0x51,0x2c,     // 1000ab79 8b512c          mov     edx,dword ptr [ecx+2Ch]
    0xff,0x52,0x3c,     // 1000ab7c ff523c          call    dword ptr [edx+3Ch]
    0x83,0xc4,0x04,     // 1000ab7f 83c404          add     esp,4
    0x5d,               // 1000ab82 5d              pop     ebp
    0xc3,               // 1000ab83 c3              ret
    0xcc
};


DECLCALLBACK(int32_t) DisasmTest1ReadCode(RTUINTPTR SrcAddr, uint8_t *pbDst, uint32_t cb, RTUINTPTR uUser)
{
    while (cb > 0)
    {
        *pbDst = g_ab32BitCode[SrcAddr];
        /* next */
        pbDst++;
        SrcAddr++;
        cb--;
    }
    return 0;
}


/*
 * Use an inline function here just to test '__textcoal_nt' sections on darwin.
 */
inline int MyDisasm(uintptr_t CodeIndex, PDISCPUSTATE pCpu, uint32_t *pcb)
{
    uint32_t cb;
    int rc = DISCoreOneEx(CodeIndex, CPUMODE_32BIT, DisasmTest1ReadCode, 0, pCpu, &cb);
    *pcb = cb;
    MY_PRINTF(("DISCoreOneEx -> rc=%d cb=%d  Cpu: opcode=%#x pCurInstr=%p (42=%d)\n", \
               rc, cb, pCpu->opcode, pCpu->pCurInstr, 42)); \
    return rc;
}


extern "C" DECLEXPORT(int) DisasmTest1(void)
{
    DISCPUSTATE Cpu = {0};
    uintptr_t CodeIndex = 0;
    uint32_t cb;
    int rc;
    MY_PRINTF(("DisasmTest1: %p\n", &DisasmTest1));

#define DISAS_AND_CHECK(cbInstr, enmOp) \
        do { \
            rc = MyDisasm(CodeIndex, &Cpu, &cb); \
            if (RT_FAILURE(rc)) \
                return CodeIndex | 0xf000; \
            if (Cpu.pCurInstr->opcode != (enmOp)) \
                return CodeIndex| 0xe000; \
            if (cb != (cbInstr)) \
                return CodeIndex | 0xd000; \
            CodeIndex += cb; \
        } while (0)

    DISAS_AND_CHECK(1, OP_PUSH);
    DISAS_AND_CHECK(2, OP_MOV);
    DISAS_AND_CHECK(3, OP_MOV);
    DISAS_AND_CHECK(6, OP_CMP);
    DISAS_AND_CHECK(2, OP_JNE);
    DISAS_AND_CHECK(3, OP_MOV);
    DISAS_AND_CHECK(4, OP_CMP);
    DISAS_AND_CHECK(2, OP_JNE);
    DISAS_AND_CHECK(5, OP_MOV);
    DISAS_AND_CHECK(2, OP_JMP);
    DISAS_AND_CHECK(2, OP_XOR);
    DISAS_AND_CHECK(2, OP_JNE);
    DISAS_AND_CHECK(3, OP_MOV);
    DISAS_AND_CHECK(1, OP_PUSH);
    DISAS_AND_CHECK(3, OP_MOV);
    DISAS_AND_CHECK(3, OP_MOV);
    DISAS_AND_CHECK(3, OP_CALL);
    DISAS_AND_CHECK(3, OP_ADD);
    DISAS_AND_CHECK(1, OP_POP);
    DISAS_AND_CHECK(1, OP_RETN);
    DISAS_AND_CHECK(1, OP_INT3);

    return rc;
}

