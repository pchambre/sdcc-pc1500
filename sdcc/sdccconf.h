/*
 * TEMPORARY placeholder, hand-written -- NOT the real autoconf-generated
 * sdccconf.h (see configure.ac / sdccconf_in.h for the genuine source).
 *
 * sdas/asxxsrc/asxxxx.h unconditionally #includes "../../sdccconf.h" on
 * non-Windows builds, but this checkout hasn't had the top-level ./configure
 * run yet (deliberately -- see sdas/aslh5801/Makefile's header comment: the
 * LH5801 assembler port is being built standalone during the
 * assembler-only phase of the project, before any src/lh5801 compiler
 * backend/full autoconf integration exists). Only DIR_SEPARATOR_CHAR/
 * DIR_SEPARATOR_STRING are actually referenced by the code this stub needs
 * to satisfy.
 *
 * Delete this file once the real ./configure is run for full SDCC build
 * integration -- it will overwrite this with the genuine generated header.
 */
#ifndef SDCCCONF_H
#define SDCCCONF_H

#define DIR_SEPARATOR_STRING "/"
#define DIR_SEPARATOR_CHAR '/'

#endif
