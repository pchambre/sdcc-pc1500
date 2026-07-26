/* m5801pst.c */

/*
 *  Copyright (C) 2026  Paul Chambre
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "asxxxx.h"
#include "lh5801.h"

/*
 * Mnemonic Structure
 */
struct	mne	mne[] = {

	/* system -- generic sdas directives, identical across every port */

    {   NULL,   "CON",          S_ATYP,         0,      A_CON   },
    {   NULL,   "OVR",          S_ATYP,         0,      A_OVR   },
    {   NULL,   "REL",          S_ATYP,         0,      A_REL   },
    {   NULL,   "ABS",          S_ATYP,         0,      A_ABS   },
    {   NULL,   "NOPAG",        S_ATYP,         0,      A_NOPAG },
    {   NULL,   "PAG",          S_ATYP,         0,      A_PAG   },

    {   NULL,   "CODE",         S_ATYP,         0,      A_CODE  },
    {   NULL,   "DATA",         S_ATYP,         0,      A_DATA  },
    {   NULL,   "LOAD",         S_ATYP,         0,      A_LOAD  },
    {   NULL,   "NOLOAD",       S_ATYP,         0,      A_NOLOAD },

    {	NULL,	".page",	S_PAGE,		0,	0	},
    {	NULL,	".title",	S_HEADER,	0,	O_TITLE	},
    {	NULL,	".sbttl",	S_HEADER,	0,	O_SBTTL	},
    {	NULL,	".module",	S_MODUL,	0,	0	},
    {	NULL,	".include",	S_INCL,		0,	I_CODE	},
    {	NULL,	".incbin",	S_INCL,		0,	I_BNRY	},
    {	NULL,	".area",	S_AREA,		0,	0	},
    {	NULL,	".org",		S_ORG,		0,	0	},
    {	NULL,	".radix",	S_RADIX,	0,	0	},
    {	NULL,	".globl",	S_GLOBL,	0,	0	},
    {	NULL,	".local",	S_LOCAL,	0,	0	},
    {	NULL,	".if",		S_CONDITIONAL,	0,	O_IF	},
    {	NULL,	".iff",		S_CONDITIONAL,	0,	O_IFF	},
    {	NULL,	".ift",		S_CONDITIONAL,	0,	O_IFT	},
    {	NULL,	".iftf",	S_CONDITIONAL,	0,	O_IFTF	},
    {	NULL,	".ifdef",	S_CONDITIONAL,	0,	O_IFDEF	},
    {	NULL,	".ifndef",	S_CONDITIONAL,	0,	O_IFNDEF},
    {	NULL,	".ifgt",	S_CONDITIONAL,	0,	O_IFGT	},
    {	NULL,	".iflt",	S_CONDITIONAL,	0,	O_IFLT	},
    {	NULL,	".ifge",	S_CONDITIONAL,	0,	O_IFGE	},
    {	NULL,	".ifle",	S_CONDITIONAL,	0,	O_IFLE	},
    {	NULL,	".ifeq",	S_CONDITIONAL,	0,	O_IFEQ	},
    {	NULL,	".ifne",	S_CONDITIONAL,	0,	O_IFNE	},
    {	NULL,	".ifb",		S_CONDITIONAL,	0,	O_IFB	},
    {	NULL,	".ifnb",	S_CONDITIONAL,	0,	O_IFNB	},
    {	NULL,	".ifidn",	S_CONDITIONAL,	0,	O_IFIDN	},
    {	NULL,	".ifdif",	S_CONDITIONAL,	0,	O_IFDIF	},
    {	NULL,	".iif",		S_CONDITIONAL,	0,	O_IIF	},
    {	NULL,	".iiff",	S_CONDITIONAL,	0,	O_IIFF	},
    {	NULL,	".iift",	S_CONDITIONAL,	0,	O_IIFT	},
    {	NULL,	".iiftf",	S_CONDITIONAL,	0,	O_IIFTF	},
    {	NULL,	".iifdef",	S_CONDITIONAL,	0,	O_IIFDEF},
    {	NULL,	".iifndef",	S_CONDITIONAL,	0,	O_IIFNDEF},
    {	NULL,	".iifgt",	S_CONDITIONAL,	0,	O_IIFGT	},
    {	NULL,	".iiflt",	S_CONDITIONAL,	0,	O_IIFLT	},
    {	NULL,	".iifge",	S_CONDITIONAL,	0,	O_IIFGE	},
    {	NULL,	".iifle",	S_CONDITIONAL,	0,	O_IIFLE	},
    {	NULL,	".iifeq",	S_CONDITIONAL,	0,	O_IIFEQ	},
    {	NULL,	".iifne",	S_CONDITIONAL,	0,	O_IIFNE	},
    {	NULL,	".iifb",	S_CONDITIONAL,	0,	O_IIFB	},
    {	NULL,	".iifnb",	S_CONDITIONAL,	0,	O_IIFNB	},
    {	NULL,	".iifidn",	S_CONDITIONAL,	0,	O_IIFIDN},
    {	NULL,	".iifdif",	S_CONDITIONAL,	0,	O_IIFDIF},
    {	NULL,	".else",	S_CONDITIONAL,	0,	O_ELSE	},
    {	NULL,	".endif",	S_CONDITIONAL,	0,	O_ENDIF	},
    {	NULL,	".list",	S_LISTING,	0,	O_LIST	},
    {	NULL,	".nlist",	S_LISTING,	0,	O_NLIST	},
    {   NULL,   ".uleb128",     S_ULEB128,      0,      0       },
    {   NULL,   ".sleb128",     S_SLEB128,      0,      0       },
    {	NULL,	".equ",		S_EQU,		0,	O_EQU	},
    {	NULL,	".gblequ",	S_EQU,		0,	O_GBLEQU},
    {	NULL,	".lclequ",	S_EQU,		0,	O_LCLEQU},
    {	NULL,	".byte",	S_DATA,		0,	O_1BYTE	},
    {	NULL,	".db",		S_DATA,		0,	O_1BYTE	},
    {	NULL,	".fcb",		S_DATA,		0,	O_1BYTE	},
    {	NULL,	".word",	S_DATA,		0,	O_2BYTE	},
    {	NULL,	".dw",		S_DATA,		0,	O_2BYTE	},
    {	NULL,	".fdb",		S_DATA,		0,	O_2BYTE	},
    {	NULL,	".blkb",	S_BLK,		0,	O_1BYTE	},
    {	NULL,	".ds",		S_BLK,		0,	O_1BYTE	},
    {	NULL,	".rmb",		S_BLK,		0,	O_1BYTE	},
    {	NULL,	".rs",		S_BLK,		0,	O_1BYTE	},
    {	NULL,	".blkw",	S_BLK,		0,	O_2BYTE	},
    {	NULL,	".ascii",	S_ASCIX,	0,	O_ASCII	},
    {	NULL,	".ascis",	S_ASCIX,	0,	O_ASCIS	},
    {	NULL,	".asciz",	S_ASCIX,	0,	O_ASCIZ	},
    {	NULL,	".str",		S_ASCIX,	0,	O_ASCII	},
    {	NULL,	".strs",	S_ASCIX,	0,	O_ASCIS	},
    {	NULL,	".strz",	S_ASCIX,	0,	O_ASCIZ	},
    {	NULL,	".fcc",		S_ASCIX,	0,	O_ASCII	},
    {	NULL,	".define",	S_DEFINE,	0,	O_DEF	},
    {	NULL,	".undefine",	S_DEFINE,	0,	O_UNDEF	},
    {	NULL,	".even",	S_BOUNDARY,	0,	O_EVEN	},
    {	NULL,	".odd",		S_BOUNDARY,	0,	O_ODD	},
    {	NULL,	".bndry",	S_BOUNDARY,	0,	O_BNDRY	},
    {	NULL,	".msg"	,	S_MSG,		0,	0	},
    {	NULL,   ".assume",      S_ERROR,        0,      O_ASSUME},
    {	NULL,   ".error",       S_ERROR,        0,      O_ERROR	},

/* sdas specific */
    {   NULL,   ".optsdcc",     S_OPTSDCC,      0,      0       },
/* end sdas specific */

	/* Macro Processor -- generic, identical across every port */

    {	NULL,	".macro",	S_MACRO,	0,	O_MACRO	},
    {	NULL,	".endm",	S_MACRO,	0,	O_ENDM	},
    {	NULL,	".mexit",	S_MACRO,	0,	O_MEXIT	},

    {	NULL,	".narg",	S_MACRO,	0,	O_NARG	},
    {	NULL,	".nchr",	S_MACRO,	0,	O_NCHR	},
    {	NULL,	".ntyp",	S_MACRO,	0,	O_NTYP	},

    {	NULL,	".irp",		S_MACRO,	0,	O_IRP	},
    {	NULL,	".irpc",	S_MACRO,	0,	O_IRPC	},
    {	NULL,	".rept",	S_MACRO,	0,	O_REPT	},

    {	NULL,	".nval",	S_MACRO,	0,	O_NVAL	},

    {	NULL,	".mdelete",	S_MACRO,	0,	O_MDEL	},

	/* LH5801 mnemonics -- see docs/lh5801_opcode_reference.md (in the
	   pc1500emu project) for the source table these are transposed
	   from, and lh5801.h for what each S_TYPxxx type/m_valu packing
	   means. */

	/* inherent, 1-byte opcode */
    {	NULL,	"aex",		S_INH,		0,	0xF1	},
    {	NULL,	"nop",		S_INH,		0,	0x38	},
    {	NULL,	"cin",		S_INH,		0,	0xF7	},
    {	NULL,	"tin",		S_INH,		0,	0xF5	},
    {	NULL,	"rec",		S_INH,		0,	0xF9	},
    {	NULL,	"sec",		S_INH,		0,	0xFB	},
    {	NULL,	"rpu",		S_INH,		0,	0xE3	},
    {	NULL,	"spu",		S_INH,		0,	0xE1	},
    {	NULL,	"rpv",		S_INH,		0,	0xB8	},
    {	NULL,	"spv",		S_INH,		0,	0xA8	},
    {	NULL,	"rti",		S_INH,		0,	0x8A	},
    {	NULL,	"rtn",		S_INH,		0,	0x9A	},
    {	NULL,	"rol",		S_INH,		0,	0xDB	},
    {	NULL,	"ror",		S_INH,		0,	0xD1	},
    {	NULL,	"shl",		S_INH,		0,	0xD9	},
    {	NULL,	"shr",		S_INH,		0,	0xD5	},

	/* inherent, FD-prefixed 2-byte opcode */
    {	NULL,	"hlt",		S_INH2,		0,	0xB1	},
    {	NULL,	"ita",		S_INH2,		0,	0xBA	},
    {	NULL,	"cdv",		S_INH2,		0,	0x8E	},
    {	NULL,	"atp",		S_INH2,		0,	0xCC	},
    {	NULL,	"att",		S_INH2,		0,	0xEC	},
    {	NULL,	"am0",		S_INH2,		0,	0xCE	},
    {	NULL,	"am1",		S_INH2,		0,	0xDE	},
    {	NULL,	"off",		S_INH2,		0,	0x4C	},
    {	NULL,	"rdp",		S_INH2,		0,	0xC0	},
    {	NULL,	"sdp",		S_INH2,		0,	0xC1	},
    {	NULL,	"rie",		S_INH2,		0,	0xBE	},
    {	NULL,	"sie",		S_INH2,		0,	0x81	},
    {	NULL,	"tta",		S_INH2,		0,	0xAA	},

	/* register+memory ALU family: RL/RH/(R)/(ab), + ME1 (#) variants */
    {	NULL,	"adc",		S_TYPR,		0,	0x038202	},
    {	NULL,	"sbc",		S_TYPR,		0,	0x018000	},
    {	NULL,	"cpa",		S_TYPR,		0,	0x078606	},
    {	NULL,	"lda",		S_TYPR,		0,	0x058404	},
    {	NULL,	"sta",		S_TYPR,		0,	0x0E080A	},

	/* memory-only family: (R)/(ab), + ME1 (#) variants */
    {	NULL,	"and",		S_TYPM,		0,	0x09	},
    {	NULL,	"ora",		S_TYPM,		0,	0x0B	},
    {	NULL,	"eor",		S_TYPM,		0,	0x0D	},
    {	NULL,	"bit",		S_TYPM,		0,	0x0F	},

	/* memory-only, no absolute form: (R) + ME1 (#) variants only */
    {	NULL,	"dca",		S_TYPDC,	0,	0x8C	},
    {	NULL,	"dcs",		S_TYPDC,	0,	0x0C	},

	/* memory-immediate + A-immediate family */
    {	NULL,	"adi",		S_TYPMI,	0,	0xB34F	},
    {	NULL,	"ani",		S_TYPMI,	0,	0xB949	},
    {	NULL,	"ori",		S_TYPMI,	0,	0xBB4B	},
    {	NULL,	"bii",		S_TYPMI,	0,	0xBF4D	},

	/* RL,i / RH,i / A,i */
    {	NULL,	"cpi",		S_TYPCPI,	0,	0xB74C4E	},

	/* A,i only, single opcode */
    {	NULL,	"sbi",		S_IMMA,		0,	0xB1	},
    {	NULL,	"eai",		S_IMMA,		0,	0xBD	},

	/* INC/DEC: A / RL / RH(ME1) / R(16-bit) */
    {	NULL,	"inc",		S_TYPID,	0,	0x4440DD	},
    {	NULL,	"dec",		S_TYPID,	0,	0x4642DF	},

	/* LDI: RL,i / RH,i / A,i / S,i,j */
    {	NULL,	"ldi",		S_TYPLDI,	0,	0xAAB5484AUL	},

	/* LDX: source register select nibble (dest always X) */
    {	NULL,	"ldx",		S_TYPLDX,	0,	0	},

	/* STX: dest register select, irregular spacing */
    {	NULL,	"stx",		S_TYPSTX,	0,	0	},

	/* ADR: Rreg only, FD-prefixed */
    {	NULL,	"adr",		S_TYPADR,	0,	0xCA	},

	/* LDE/LIN/SDE/SIN: Rreg only, not FD-prefixed */
    {	NULL,	"lde",		S_TYPRXYU,	0,	0x47	},
    {	NULL,	"lin",		S_TYPRXYU,	0,	0x45	},
    {	NULL,	"sde",		S_TYPRXYU,	0,	0x43	},
    {	NULL,	"sin",		S_TYPRXYU,	0,	0x41	},

	/* PSH/POP: A/X/Y/U, FD-prefixed */
    {	NULL,	"psh",		S_TYPSTK,	0,	0xC888	},
    {	NULL,	"pop",		S_TYPSTK,	0,	0x8A0A	},

	/* DRL/DRR: (X) or #(X) only */
    {	NULL,	"drl",		S_TYPDR,	0,	0xD7	},
    {	NULL,	"drr",		S_TYPDR,	0,	0xD3	},

	/* LOP: UL,i fixed encoding */
    {	NULL,	"lop",		S_TYPLOP,	0,	0	},

	/* JMP/SJP: absolute 16-bit target */
    {	NULL,	"jmp",		S_TYPABS,	0,	0xBA	},
    {	NULL,	"sjp",		S_TYPABS,	0,	0xBE	},

	/* Conditional/unconditional relative branches: +i / -i opcode pair */
    {	NULL,	"bch",		S_TYPBRA,	0,	0x9E8E	},
    {	NULL,	"bcs",		S_TYPBRA,	0,	0x9383	},
    {	NULL,	"bcr",		S_TYPBRA,	0,	0x9181	},
    {	NULL,	"bhs",		S_TYPBRA,	0,	0x9787	},
    {	NULL,	"bhr",		S_TYPBRA,	0,	0x9585	},
    {	NULL,	"bvs",		S_TYPBRA,	0,	0x9F8F	},
    {	NULL,	"bvr",		S_TYPBRA,	0,	0x9D8D	},
    {	NULL,	"bzs",		S_TYPBRA,	0,	0x9B8B	},
    {	NULL,	"bzr",		S_TYPBRA,	0,	0x9989	},

	/* Vector subroutine calls: opcode + separate immediate vector byte */
    {	NULL,	"vcs",		S_TYPVC,	0,	0xC3	},
    {	NULL,	"vcr",		S_TYPVC,	0,	0xC1	},
    {	NULL,	"vmj",		S_TYPVC,	0,	0xCD	},
    {	NULL,	"vvs",		S_TYPVC,	0,	0xCF	},
    {	NULL,	"vzs",		S_TYPVC,	0,	0xCB	},
    {	NULL,	"vzr",		S_TYPVC,	0,	0xC9	},
    {	NULL,	"vhr",		S_TYPVC,	0,	0xC5	},
    {	NULL,	"vhs",		S_TYPVC,	0,	0xC7	},

	/* VEJ: operand IS the opcode byte (must be even, 0C0H-0F6H) */
    {	NULL,	"vej",		S_TYPVEJ,	S_EOL,	0	}
};
