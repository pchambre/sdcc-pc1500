/* m5801adr.c */

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
 * LH5801 operand syntax (deliberately mirrors the technical reference
 * manual's own notation, see docs/lh5801_opcode_reference.md):
 *
 *   Register direct:     a  xl  yl  ul  xh  yh  uh  x  y  u  s  p
 *   ME0 indirect:        (x)  (y)  (u)
 *   ME1 indirect:        #(x)  #(y)  #(u)
 *   ME0 absolute:        (expr)
 *   ME1 absolute:        #(expr)
 *
 * Unlike most sdas ports, '#' is never "immediate" here -- the manual's own
 * notation already uses '#' exclusively for ME1 (the LH5811-side memory
 * space), so we keep that meaning rather than overloading it. Immediate
 * operands (the "i" in ADI, LDI, etc.) have no marker at all: the mnemonic
 * itself already implies "this operand is an immediate", so those are
 * parsed with a plain expr() call in m5801mch.c, not through addr().
 */

int
addr(struct expr *esp)
{
	int c;
	int indx;

	if ((c = getnb()) == '#') {
		if (getnb() != '(') {
			xerr('a', "Expected '(' After '#'.");
		}
		if ((indx = admode(xyu)) != 0) {
			switch (indx) {
			case S_X:	esp->e_mode = S_IX1;	break;
			case S_Y:	esp->e_mode = S_IY1;	break;
			default:	esp->e_mode = S_IU1;	break;
			}
		} else {
			expr(esp, 0);
			esp->e_mode = S_EXT1;
		}
		if (getnb() != ')') {
			xerr('a', "Missing ')'.");
		}
	} else
	if (c == '(') {
		if ((indx = admode(xyu)) != 0) {
			switch (indx) {
			case S_X:	esp->e_mode = S_IX;	break;
			case S_Y:	esp->e_mode = S_IY;	break;
			default:	esp->e_mode = S_IU;	break;
			}
		} else {
			expr(esp, 0);
			esp->e_mode = S_EXT;
		}
		if (getnb() != ')') {
			xerr('a', "Missing ')'.");
		}
	} else {
		unget(c);
		if ((indx = admode(regs)) != 0) {
			esp->e_mode = indx;
		} else {
			xerr('a', "Invalid Addressing Mode.");
			esp->e_mode = S_A;
		}
	}
	return (esp->e_mode);
}

/*
 * Enter admode() to search a specific addressing mode table
 * for a match. Return the addressing value on a match or
 * zero for no match.
 */
int
admode(struct adsym *sp)
{
	char *ptr;
	int i;
	char *ips;

	ips = ip;
	unget(getnb());

	i = 0;
	while ( *(ptr = &sp[i].a_str[0]) ) {
		if (srch(ptr)) {
			return(sp[i].a_val);
		}
		i++;
	}
	ip = ips;
	return(0);
}

/*
 *      srch --- does string match ?
 */
int
srch(char *str)
{
	char *ptr;
	ptr = ip;

	while (*ptr && *str) {
		if(ccase[*ptr & 0x007F] != ccase[*str & 0x007F])
			break;
		ptr++;
		str++;
	}
	if (ccase[*ptr & 0x007F] == ccase[*str & 0x007F]) {
		ip = ptr;
		return(1);
	}

	if (!*str)
		if (!(ctype[*ptr & 0x007F] & LTR16)) {
			ip = ptr;
			return(1);
		}
	return(0);
}

/* Full register-direct table -- 2-character names listed before the
 * 1-character names they'd otherwise be a prefix of (x/xl/xh, y/yl/yh,
 * u/ul/uh), matching the convention every other sdas port uses even though
 * srch()'s trailing-boundary check makes the order harmless here (none of
 * these names are followed by non-letter continuation characters the way
 * 6808's "x+"/"x++" are). */
struct adsym	regs[] = {
    {	"xl",	S_XL	},
    {	"xh",	S_XH	},
    {	"yl",	S_YL	},
    {	"yh",	S_YH	},
    {	"ul",	S_UL	},
    {	"uh",	S_UH	},
    {	"x",	S_X	},
    {	"y",	S_Y	},
    {	"u",	S_U	},
    {	"a",	S_A	},
    {	"s",	S_S	},
    {	"p",	S_P	},
    {	"",	0x00	}
};

/* Register table for inside (...) / #(...) -- only X, Y, U can be used as
 * a pointer register. */
struct adsym	xyu[] = {
    {	"x",	S_X	},
    {	"y",	S_Y	},
    {	"u",	S_U	},
    {	"",	0x00	}
};
