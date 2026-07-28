/*-------------------------------------------------------------------------
   features.h - LH5801 features.

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   As a special exception, if you link this library with other files,
   some of which are compiled with SDCC, to produce an executable,
   this library does not by itself cause the resulting executable to
   be covered by the GNU General Public License. This exception does
   not however invalidate any other reasons why the executable file
   might be covered by the GNU General Public License.
-------------------------------------------------------------------------*/

#ifndef __SDCC_ASM_FEATURES_H
#define __SDCC_ASM_FEATURES_H   1

/* _REENTRANT expands to the real __reentrant keyword here (unlike an
   earlier version of this file, which defined it empty). That earlier
   choice was based on a misdiagnosis: stdio.h's
   `typedef void (*pfn_outputchar)(...) _REENTRANT;` did fail to parse,
   but the actual cause was that src/lh5801/main.c's lh5801_keywords[]
   was empty, so isTargetKeyword() (SDCCglue.c) never recognized
   "__reentrant" as a keyword at all -- SDCC.lex's TKEYWORD macro then
   silently downgrades it to a plain identifier, which is what actually
   produced the syntax error. It looked exactly like a missing grammar
   production (declarator2_function_attributes/function_declarator in
   SDCC.y in fact already handle REENTRANT on a function-pointer
   declarator like this one just fine), but wasn't one. Once
   lh5801_keywords[] was fixed (needed anyway for printf's own
   pfn_outputchar callback -- see CALL/PCALL codegen in gen.c), this
   exact typedef line was confirmed, via a standalone test, to parse
   correctly with no grammar changes at all. Real reentrant-ness *is*
   meaningful for us despite options.stackAuto=0 (src/lh5801/main.c):
   SDCCast.c's E_NONRENT_ARGS check requires a function type be marked
   reentrant before it can be called indirectly with arguments, which
   printf_large.c's internal callback needs. */
#define _REENTRANT __reentrant
#define _CODE
#define _AUTOMEM
#define _STATMEM

#define __SDCC_NONBANKED

#endif
