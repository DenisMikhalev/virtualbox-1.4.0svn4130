/** @file
 *
 * VBox disassembler:
 * Internal header
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

#ifndef __DisasmInternal_h__
#define __DisasmInternal_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/dis.h>

#define ExceptionMemRead          0x666
#define ExceptionInvalidModRM     0x667
#define ExceptionInvalidParameter 0x668

#define IDX_ParseNop                0
#define IDX_ParseModRM              1
#define IDX_UseModRM                2
#define IDX_ParseImmByte            3
#define IDX_ParseImmBRel            4
#define IDX_ParseImmUshort          5
#define IDX_ParseImmV               6
#define IDX_ParseImmVRel            7
#define IDX_ParseImmAddr            8
#define IDX_ParseFixedReg           9
#define IDX_ParseImmUlong           10
#define IDX_ParseImmQword           11
#define IDX_ParseTwoByteEsc         12
#define IDX_ParseImmGrpl            13
#define IDX_ParseShiftGrp2          14
#define IDX_ParseGrp3               15
#define IDX_ParseGrp4               16
#define IDX_ParseGrp5               17
#define IDX_Parse3DNow              18
#define IDX_ParseGrp6               19
#define IDX_ParseGrp7               20
#define IDX_ParseGrp8               21
#define IDX_ParseGrp9               22
#define IDX_ParseGrp10              23
#define IDX_ParseGrp12              24
#define IDX_ParseGrp13              25
#define IDX_ParseGrp14              26
#define IDX_ParseGrp15              27
#define IDX_ParseGrp16              28
#define IDX_ParseModFence           29
#define IDX_ParseYv                 30
#define IDX_ParseYb                 31
#define IDX_ParseXv                 32
#define IDX_ParseXb                 33
#define IDX_ParseEscFP              34
#define IDX_ParseNopPause           35
#define IDX_ParseImmByteSX          36
#define IDX_ParseMax                (IDX_ParseImmByteSX+1)

#ifdef IN_RING0
#define DIS_THROW(a)                /* Not available. */
#elif  __L4ENV__
#define DIS_THROW(a)                longjmp(*pCpu->pJumpBuffer, a)
#else
#define DIS_THROW(a)                throw(a)
#endif


extern PFNDISPARSE  pfnFullDisasm[IDX_ParseMax];
extern PFNDISPARSE  pfnCalcSize[IDX_ParseMax];


__BEGIN_DECLS

int ParseInstruction(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, PDISCPUSTATE pCpu);

int ParseIllegal(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseModRM(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseModRM_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int UseModRM(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmByte(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmByte_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmByteSX(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmByteSX_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmBRel(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmBRel_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmUshort(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmUshort_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmV(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmV_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmVRel(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmVRel_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

int ParseImmAddr(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmAddr_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseFixedReg(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmUlong(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmUlong_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmQword(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmQword_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

int ParseTwoByteEsc(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseImmGrpl(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseShiftGrp2(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp3(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp4(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp5(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int Parse3DNow(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp6(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp7(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp8(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp9(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp10(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp12(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp13(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp14(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp15(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseGrp16(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseModFence(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseNopPause(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

int ParseYv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseYb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseXv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
int ParseXb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

/* Floating point parsing */
int ParseEscFP(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

/* Disassembler printf */
void disasmSprintf(char *pszOutput, RTUINTPTR pu8Instruction, PDISCPUSTATE pCpu, POP_PARAMETER pParam1, POP_PARAMETER pParam2, POP_PARAMETER pParam3 = NULL);
void disasmGetPtrString(PDISCPUSTATE pCpu, PCOPCODE pOp, POP_PARAMETER pParam);
void disasmModRMReg(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam, int fRegAddr);
void disasmModRMReg16(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam);
void disasmModRMSReg(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam);
void disasmPrintAbs32(POP_PARAMETER pParam);
void disasmPrintDisp32(POP_PARAMETER pParam);
void disasmPrintDisp8(POP_PARAMETER pParam);
void disasmPrintDisp16(POP_PARAMETER pParam);


#ifdef IN_GC
#define  DISReadByte(pCpu,  pAddress) (*(uint8_t *)(pAddress))
#define  DISReadWord(pCpu,  pAddress) (*(uint16_t *)(pAddress))
#define  DISReadDWord(pCpu, pAddress) (*(uint32_t *)(pAddress))
#define  DISReadQWord(pCpu, pAddress) (*(uint64_t *)(pAddress))
#else
/* Read functions */
uint8_t  DISReadByte(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint16_t DISReadWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint32_t DISReadDWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint64_t DISReadQWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
#endif

__END_DECLS

#endif /* !__DisasmInternal_h__ */

