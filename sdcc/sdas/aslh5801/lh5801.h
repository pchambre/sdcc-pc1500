/* lh5801.h */

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

/*
 * sdas port for the Sharp LH5801 CPU (Sharp PC-1500). Opcode/
 * addressing-mode facts are transposed from pc1500emu's
 * docs/lh5801_opcode_reference.md, itself cross-checked against the PC-1500
 * and PC-1600 technical reference manuals and validated against real
 * PC-1500 hardware over the course of that project.
 */

struct adsym
{
	char	a_str[4];	/* addressing string */
	int	a_val;		/* addressing mode value */
};

/*
 * Addressing types, returned by addr(). Values >= 30 to stay clear of the
 * generic S_* directive tags in asxxsrc/asxxxx.h (which top out at 35), the
 * same convention every other sdas port uses.
 */
#define	S_A	30	/* accumulator */
#define	S_XL	31
#define	S_YL	32
#define	S_UL	33
#define	S_XH	34
#define	S_YH	35
#define	S_UH	36
#define	S_X	37	/* 16-bit register, register-transfer context */
#define	S_Y	38
#define	S_U	39
#define	S_S	40	/* stack pointer */
#define	S_P	41	/* program counter */
#define	S_IX	42	/* (X) -- ME0 indirect via X */
#define	S_IY	43	/* (Y) -- ME0 indirect via Y */
#define	S_IU	44	/* (U) -- ME0 indirect via U */
#define	S_IX1	45	/* #(X) -- ME1 indirect via X */
#define	S_IY1	46	/* #(Y) -- ME1 indirect via Y */
#define	S_IU1	47	/* #(U) -- ME1 indirect via U */
#define	S_EXT	48	/* (ab) -- ME0 absolute */
#define	S_EXT1	49	/* #(ab) -- ME1 absolute */

/*
 * Instruction types (m_type / mp->m_type). Values >= 100, one per distinct
 * operand-syntax shape in the ISA (see m5801mch.c for the encoding rule
 * each implements, and m5801pst.c's mne[] table for which mnemonics use
 * which type). Each type's opcode byte(s) are packed into the mne[] entry's
 * m_valu field (up to 4 bytes, LSB = byte 0); the exact packing is
 * documented per type below and implemented in machine().
 */
#define	S_INH		100	/* 1-byte inherent.                    byte0=op */
#define	S_INH2		101	/* FD-prefixed 2-byte inherent.        byte0=op */
#define	S_TYPR		102	/* ADC/SBC/CPA/LDA/STA (RL/RH/(R)/(ab), +ME1).
				   byte0=rl_base byte1=rh_base byte2=mem_base */
#define	S_TYPM		103	/* AND/ORA/EOR/BIT ((R)/(ab) only, +ME1).
				   byte0=mem_base */
#define	S_TYPDC		104	/* DCA/DCS ((R) only, +ME1, no (ab)).
				   byte0=mem_base */
#define	S_TYPMI		105	/* ADI/ANI/ORI/BII (A,i / (R),i / (ab),i, +ME1).
				   byte0=mem_base byte1=a_base */
#define	S_TYPCPI	106	/* CPI (RL,i / RH,i / A,i).
				   byte0=rl_base byte1=rh_base byte2=a_base */
#define	S_IMMA		107	/* SBI/EAI (A,i only, single opcode).  byte0=op */
#define	S_TYPID		108	/* INC/DEC (A / RL / RH(ME1) / R(16-bit)).
				   byte0=a_op byte1=rl_base byte2=r_base */
#define	S_TYPLDI	109	/* LDI (RL,i / RH,i / A,i / S,i,j).
				   byte0=rl_base byte1=rh_base
				   byte2=a_base  byte3=s_base */
#define	S_TYPLDX	110	/* LDX (source register select nibble).
				   no packed bytes -- single mnemonic */
#define	S_TYPSTX	111	/* STX (dest register select, irregular).
				   no packed bytes -- single mnemonic */
#define	S_TYPADR	112	/* ADR (Rreg only, FD-prefixed).       byte0=base */
#define	S_TYPRXYU	113	/* LDE/LIN/SDE/SIN (Rreg only).        byte0=base */
#define	S_TYPSTK	114	/* PSH/POP (A/X/Y/U, FD-prefixed).
				   byte0=xyu_base byte1=a_base */
#define	S_TYPDR		115	/* DRL/DRR ((X) / #(X) only).          byte0=op */
#define	S_TYPLOP	116	/* LOP (UL,i, fixed encoding).
				   no packed bytes -- single mnemonic */
#define	S_TYPABS	117	/* JMP/SJP (absolute 16-bit target).   byte0=op */
#define	S_TYPBRA	118	/* BCH/BCS/BCR/BHS/BHR/BVS/BVR/BZS/BZR
				   (direction picks the opcode -- see
				   m5801mch.c for why this can't be deferred
				   to the linker the way a normal PC-relative
				   branch's R_PCR relocation can).
				   byte0=plus_base byte1=minus_base */
#define	S_TYPVC		119	/* VCS/VCR/VMJ/VVS/VZS/VZR/VHR/VHS
				   (vector call, separate immediate vector
				   byte, not PC-relative).              byte0=op */
#define	S_TYPVEJ	120	/* VEJ (operand IS the opcode byte, must be
				   even and in C0H-F6H).
				   no packed bytes -- single mnemonic */

	/* machine dependent functions */

	/* m5801adr.c */
extern	struct	adsym	regs[];
extern	struct	adsym	xyu[];
extern	int		addr(struct expr *esp);
extern	int		admode(struct adsym *sp);
extern	int		srch(char *str);

	/* m5801mch.c */
extern	void		machine(struct mne *mp);
extern	int		mchpcr(struct expr *esp);
extern	void		minit(void);
