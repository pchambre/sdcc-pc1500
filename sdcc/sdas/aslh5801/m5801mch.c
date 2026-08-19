/* m5801mch.c */

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

char	*cpu	= "Sharp LH5801 (Sharp PC-1500)";
char	*dsft	= "asm";

/*
 * Each mne[] entry's m_valu packs up to four opcode-related bytes, LSB
 * first; which bytes a given m_type uses is documented in lh5801.h next to
 * that type's #define.
 */
#define	B0(v)	((int) ((v) & 0xFF))
#define	B1(v)	((int) (((v) >> 8) & 0xFF))
#define	B2(v)	((int) (((v) >> 16) & 0xFF))
#define	B3(v)	((int) (((v) >> 24) & 0xFF))

/*
 * FD is the LH5801's second-opcode-space prefix byte (see
 * docs/lh5801_opcode_reference.md's "Opcode byte format" note) -- used for
 * ME1 (#(...)) addressing and for a number of otherwise-inherent
 * instructions that just happen to live in that second space.
 */
#define	FD	0xFD

#define	OPCY_ERR	((char) (0xFE))

/*
 * Process a machine op.
 */
void
machine(struct mne *mp)
{
	a_uint op;
	int t1;
	struct expr e1, e2;
	int v1;

	clrexpr(&e1);
	clrexpr(&e2);
	op = mp->m_valu;

	switch (mp->m_type) {

	case S_INH:
		outab(B0(op));
		break;

	case S_INH2:
		outab(FD);
		outab(B0(op));
		break;

	case S_TYPR:
		t1 = addr(&e1);
		switch (t1) {
		case S_XL:	outab(B0(op));			break;
		case S_YL:	outab(B0(op) + 0x10);		break;
		case S_UL:	outab(B0(op) + 0x20);		break;
		case S_XH:	outab(B1(op));			break;
		case S_YH:	outab(B1(op) + 0x10);		break;
		case S_UH:	outab(B1(op) + 0x20);		break;
		case S_IX:	outab(B2(op));			break;
		case S_IY:	outab(B2(op) + 0x10);		break;
		case S_IU:	outab(B2(op) + 0x20);		break;
		case S_EXT:	outab(B2(op) + 0xA0);	outrw(&e1, 0);	break;
		case S_IX1:	outab(FD); outab(B2(op));		break;
		case S_IY1:	outab(FD); outab(B2(op) + 0x10);	break;
		case S_IU1:	outab(FD); outab(B2(op) + 0x20);	break;
		case S_EXT1:	outab(FD); outab(B2(op) + 0xA0); outrw(&e1, 0);	break;
		default:	xerr('a', "Invalid Addressing Mode.");		break;
		}
		break;

	case S_TYPM:
		t1 = addr(&e1);
		switch (t1) {
		case S_IX:	outab(B0(op));			break;
		case S_IY:	outab(B0(op) + 0x10);		break;
		case S_IU:	outab(B0(op) + 0x20);		break;
		case S_EXT:	outab(B0(op) + 0xA0);	outrw(&e1, 0);	break;
		case S_IX1:	outab(FD); outab(B0(op));		break;
		case S_IY1:	outab(FD); outab(B0(op) + 0x10);	break;
		case S_IU1:	outab(FD); outab(B0(op) + 0x20);	break;
		case S_EXT1:	outab(FD); outab(B0(op) + 0xA0); outrw(&e1, 0);	break;
		default:	xerr('a', "Invalid Addressing Mode -- (R) Or (ab) Required.");	break;
		}
		break;

	case S_TYPDC:
		t1 = addr(&e1);
		switch (t1) {
		case S_IX:	outab(B0(op));			break;
		case S_IY:	outab(B0(op) + 0x10);		break;
		case S_IU:	outab(B0(op) + 0x20);		break;
		case S_IX1:	outab(FD); outab(B0(op));		break;
		case S_IY1:	outab(FD); outab(B0(op) + 0x10);	break;
		case S_IU1:	outab(FD); outab(B0(op) + 0x20);	break;
		default:	xerr('a', "Invalid Addressing Mode -- (X)/(Y)/(U) Only, No Absolute Form.");	break;
		}
		break;

	case S_TYPMI:
		t1 = addr(&e1);
		if (t1 == S_A) {
			outab(B1(op));
		} else {
			switch (t1) {
			case S_IX:	outab(B0(op));			break;
			case S_IY:	outab(B0(op) + 0x10);		break;
			case S_IU:	outab(B0(op) + 0x20);		break;
			case S_EXT:	outab(B0(op) + 0xA0);	outrw(&e1, 0);	break;
			case S_IX1:	outab(FD); outab(B0(op));		break;
			case S_IY1:	outab(FD); outab(B0(op) + 0x10);	break;
			case S_IU1:	outab(FD); outab(B0(op) + 0x20);	break;
			case S_EXT1:	outab(FD); outab(B0(op) + 0xA0); outrw(&e1, 0);	break;
			default:	xerr('a', "Invalid Addressing Mode.");	break;
			}
		}
		comma(1);
		expr(&e2, 0);
		outrb(&e2, 0);
		break;

	case S_TYPCPI:
		t1 = addr(&e1);
		switch (t1) {
		case S_XL:	outab(B0(op));		break;
		case S_YL:	outab(B0(op) + 0x10);	break;
		case S_UL:	outab(B0(op) + 0x20);	break;
		case S_XH:	outab(B1(op));		break;
		case S_YH:	outab(B1(op) + 0x10);	break;
		case S_UH:	outab(B1(op) + 0x20);	break;
		case S_A:	outab(B2(op));		break;
		default:	xerr('a', "Invalid Addressing Mode.");	break;
		}
		comma(1);
		expr(&e2, 0);
		outrb(&e2, 0);
		break;

	case S_IMMA:
		t1 = addr(&e1);
		if (t1 != S_A) {
			xerr('a', "Accumulator (A) Required.");
		}
		comma(1);
		expr(&e2, 0);
		outab(B0(op));
		outrb(&e2, 0);
		break;

	case S_TYPID:
		t1 = addr(&e1);
		switch (t1) {
		case S_A:	outab(B0(op));			break;
		case S_XL:	outab(B1(op));			break;
		case S_YL:	outab(B1(op) + 0x10);		break;
		case S_UL:	outab(B1(op) + 0x20);		break;
		case S_XH:	outab(FD); outab(B1(op));		break;
		case S_YH:	outab(FD); outab(B1(op) + 0x10);	break;
		case S_UH:	outab(FD); outab(B1(op) + 0x20);	break;
		case S_X:	outab(B2(op));			break;
		case S_Y:	outab(B2(op) + 0x10);		break;
		case S_U:	outab(B2(op) + 0x20);		break;
		default:	xerr('a', "Invalid Addressing Mode.");	break;
		}
		break;

	case S_TYPLDI:
		t1 = addr(&e1);
		switch (t1) {
		case S_XL:
			outab(B0(op));
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_YL:
			outab(B0(op) + 0x10);
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_UL:
			outab(B0(op) + 0x20);
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_XH:
			outab(B1(op));
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_YH:
			outab(B1(op) + 0x10);
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_UH:
			outab(B1(op) + 0x20);
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_A:
			outab(B2(op));
			comma(1); expr(&e2, 0); outrb(&e2, 0);
			break;
		case S_S:
			outab(B3(op));
			comma(1); expr(&e2, 0); outrw(&e2, 0);
			break;
		default:
			xerr('a', "Invalid Addressing Mode.");
			break;
		}
		break;

	case S_TYPLDX:
		t1 = addr(&e1);
		switch (t1) {
		case S_X:	outab(FD); outab(0x08);	break;
		case S_Y:	outab(FD); outab(0x18);	break;
		case S_U:	outab(FD); outab(0x28);	break;
		case S_S:	outab(FD); outab(0x48);	break;
		case S_P:	outab(FD); outab(0x58);	break;
		default:	xerr('a', "Register X, Y, U, S, Or P Required.");	break;
		}
		break;

	case S_TYPSTX:
		t1 = addr(&e1);
		switch (t1) {
		case S_X:	outab(FD); outab(0x4A);	break;
		case S_Y:	outab(FD); outab(0x5A);	break;
		case S_U:	outab(FD); outab(0x6A);	break;
		case S_S:	outab(FD); outab(0x4E);	break;
		case S_P:	outab(FD); outab(0x5E);	break;
		default:	xerr('a', "Register X, Y, U, S, Or P Required.");	break;
		}
		break;

	case S_TYPADR:
		t1 = addr(&e1);
		switch (t1) {
		case S_X:	outab(FD); outab(B0(op));		break;
		case S_Y:	outab(FD); outab(B0(op) + 0x10);	break;
		case S_U:	outab(FD); outab(B0(op) + 0x20);	break;
		default:	xerr('a', "Register X, Y, Or U Required.");	break;
		}
		break;

	case S_TYPRXYU:
		t1 = addr(&e1);
		switch (t1) {
		case S_X:	outab(B0(op));		break;
		case S_Y:	outab(B0(op) + 0x10);	break;
		case S_U:	outab(B0(op) + 0x20);	break;
		default:	xerr('a', "Register X, Y, Or U Required.");	break;
		}
		break;

	case S_TYPSTK:
		t1 = addr(&e1);
		switch (t1) {
		case S_A:	outab(FD); outab(B1(op));		break;
		case S_X:	outab(FD); outab(B0(op));		break;
		case S_Y:	outab(FD); outab(B0(op) + 0x10);	break;
		case S_U:	outab(FD); outab(B0(op) + 0x20);	break;
		default:	xerr('a', "Register A, X, Y, Or U Required.");	break;
		}
		break;

	case S_TYPDR:
		t1 = addr(&e1);
		switch (t1) {
		case S_IX:	outab(B0(op));			break;
		case S_IX1:	outab(FD); outab(B0(op));	break;
		default:	xerr('a', "Only (X) Or #(X) Valid.");	break;
		}
		break;

	/*
	 * LOP (0x88 i): decrements UL, and if no borrow, branches to
	 * P-i -- evaluated *after* fetching both opcode bytes (see
	 * src/cpu/lh5801.cpp's case 0x88), i.e. relative to (dot+2), same
	 * as the backward ("-i") half of S_TYPBRA below. Unlike those,
	 * LOP has only the one opcode/direction (0x88 always subtracts),
	 * so there's no forward form to choose between -- just compute the
	 * backward displacement directly, the same way S_TYPBRA's own
	 * "v1 < 0" branch does.
	 *
	 * i must previously have used outrb(&e2, 0) here, which emits the
	 * raw low byte of e2's absolute value (i.e. the target label's own
	 * address & 0xFF) instead of a displacement -- correct only by
	 * coincidence when dot+2-target happened to equal that low byte.
	 */
	case S_TYPLOP:
		t1 = addr(&e1);
		if (t1 != S_UL) {
			xerr('a', "UL Required.");
		}
		comma(1);
		expr(&e2, 0);
		/*
		 * dot.s_addr must be read here, before outab(0x88) advances
		 * it -- same ordering S_TYPBRA above uses (its own expr()
		 * call, and thus its own read of dot.s_addr for v1, happens
		 * before either of its outab() calls).
		 */
		if (mchpcr(&e2)) {
			v1 = (int) (dot.s_addr + 2 - e2.e_addr);
			outab(0x88);
			if (v1 < 0 || v1 > 255) {
				xerr('a', "Branching Range Exceeded.");
			}
			outab(v1);
		} else {
			outab(0x88);
			xerr('a', "LH5801 LOP Must Target A Symbol In The Same Area (No External/Cross-Area Targets).");
			outab(0);
		}
		if (e2.e_mode != S_USER) {
			rerr();
		}
		break;

	case S_TYPABS:
		expr(&e1, 0);
		outab(B0(op));
		outrw(&e1, 0);
		break;

	/*
	 * BCH/BCS/BCR/BHS/BHR/BVS/BVR/BZS/BZR: each has two base opcodes,
	 * one for the "+i" (forward, P+i) form and one for "-i" (backward,
	 * P-i) -- confirmed against src/cpu/lh5801.cpp's own branch
	 * execution (case 0x8E/0x9E etc.): the CPU adds/subtracts an
	 * *unsigned* displacement to P *after* fetching it, i.e. relative
	 * to (opcode address + 2), and which of the two opcodes is used
	 * picks the direction -- unlike a typical single-opcode signed
	 * relative branch (e.g. 6808's BRA), the direction is baked into
	 * the choice of opcode byte, not the sign of a stored byte value.
	 *
	 * This means the byte we emit can only be resolved once we know
	 * the sign of (target - (dot+2)), which we can only be sure of at
	 * assemble time for a same-area target -- there is no way to defer
	 * "which of these two possible opcodes" to the linker the way a
	 * normal R_PCR relocation defers "what displacement value" for an
	 * external/cross-area target. So cross-area branch targets are
	 * rejected outright rather than silently mishandled.
	 */
	case S_TYPBRA:
		expr(&e1, 0);
		if (mchpcr(&e1)) {
			v1 = (int) (e1.e_addr - dot.s_addr - 2);
			if (v1 >= 0) {
				if (v1 > 255) {
					xerr('a', "Branching Range Exceeded.");
				}
				outab(B0(op));
				outab(v1);
			} else {
				v1 = -v1;
				if (v1 > 255) {
					xerr('a', "Branching Range Exceeded.");
				}
				outab(B1(op));
				outab(v1);
			}
		} else {
			xerr('a', "LH5801 Branches Must Target A Symbol In The Same Area (No External/Cross-Area Targets).");
			outab(B0(op));
			outab(0);
		}
		if (e1.e_mode != S_USER) {
			rerr();
		}
		break;

	case S_TYPVC:
		outab(B0(op));
		expr(&e1, 0);
		outrb(&e1, 0);
		break;

	case S_TYPVEJ:
		expr(&e1, 0);
		if (e1.e_flag == 0 && e1.e_base.e_ap == NULL) {
			v1 = (int) e1.e_addr;
			if ((v1 < 0xC0) || (v1 > 0xF6) || (v1 & 1)) {
				xerr('a', "VEJ Vector Must Be Even, In The Range 0C0H-0F6H.");
			}
			outab(v1);
		} else {
			xerr('a', "VEJ Vector Must Be A Constant.");
			outab(0xC0);
		}
		break;

	default:
		opcycles = OPCY_ERR;
		xerr('o', "Internal Opcode Error.");
		break;
	}
}

/*
 * Branch/Jump PCR Mode Check -- true if esp resolves within the current
 * area (so we can compute the exact displacement now), false otherwise
 * (absolute constant, or a symbol in a different/unknown area).
 */
int
mchpcr(struct expr *esp)
{
	if (esp->e_base.e_ap == dot.s_area) {
		return(1);
	}
	if (esp->e_flag==0 && esp->e_base.e_ap==NULL) {
		esp->e_flag = 1;
		esp->e_base.e_sp = &sym[1];
	}
	return(0);
}

/*
 * Machine specific initialization.
 */
void
minit(void)
{
	/*
	 * Byte order: LH5801 16-bit values (absolute addresses, LDI S,i,j)
	 * are documented high-byte-first ("ab" = a high, b low; "i,j" = i
	 * high, j low) -- see docs/lh5801_opcode_reference.md's notation
	 * table.
	 */
	hilo = 1;
}
