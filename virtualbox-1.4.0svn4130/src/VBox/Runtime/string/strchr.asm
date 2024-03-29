; $Id: strchr.asm 4071 2007-08-07 17:07:59Z vboxsync $
;; @file
; innotek Portable Runtime - No-CRT strchr - AMD64 & X86.
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.

%include "iprt/asmdefs.mac"

BEGINCODE

;;
; @param    psz     gcc: rdi  msc: rcx  x86:[esp+4]
; @param    ch      gcc: esi  msc: edx  x86:[esp+8]
BEGINPROC RT_NOCRT(strchr)
        cld

        ; check for ch == 0 and setup normal strchr.
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        or      dl, dl
        jz near .strlen
        mov     r9, rsi                 ; save rsi
        mov     rsi, rcx
 %else
        or      sil, sil
        jz near .strlen
        mov     edx, esi
        mov     rsi, rdi
 %endif
%else
        mov     edx, [esp + 8]
        or      dl, dl
        jz near .strlen
        mov     ecx, esi                ; save esi
        mov     esi, [esp + 4]
%endif

        ; do the search
.next:
        lodsb
        cmp     al, dl
        je      .found
        test    al, al
        jz      .not_found

        lodsb
        cmp     al, dl
        je      .found
        test    al, al
        jz      .not_found

        lodsb
        cmp     al, dl
        je      .found
        test    al, al
        jz      .not_found

        lodsb
        cmp     al, dl
        je      .found
        test    al, al
        jz      .not_found
        jmp .next

.found:
        lea     xAX, [xSI - 1]
%ifdef ASM_CALL64_MSC
        mov     rsi, r9
%endif
%ifdef RT_ARCH_X86
        mov     esi, ecx
%endif
        ret

.not_found:
%ifdef ASM_CALL64_MSC
        mov     rsi, r9
%endif
%ifdef RT_ARCH_X86
        mov     esi, ecx
%endif
        xor     eax, eax
        ret

;
; Special case: strchr(str, '\0');
;
align 16
.strlen:
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        mov     r9, rdi                 ; save rdi
        mov     rdi, rcx
 %endif
%else
        mov     edx, edi                ; save edi
        mov     edi, [esp + 4]
%endif
        mov     xCX, -1
        xor     eax, eax
        repne scasb

        lea     xAX, [xDI - 1]
%ifdef ASM_CALL64_MSC
        mov     rdi, r9
%endif
%ifdef RT_ARCH_X86
        mov     edi, edx
%endif
        ret
ENDPROC RT_NOCRT(strchr)

