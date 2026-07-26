#
# Plain (non-autoconf) Makefile for the LH5801 sdas port.
#
# Deliberately not wired into configure.ac/AC_DO_PORT yet: every existing
# AC_DO_PORT call configures sdas/as<port>/Makefile *and* src/<port>/Makefile
# together (see sdcc/configure.ac), and we don't have a src/lh5801 compiler
# backend yet -- that's a later phase of the project. This Makefile builds
# standalone via a plain `make` in this directory; real autoconf integration
# is deferred until the compiler backend exists too.
#

CC		= cc
CFLAGS		= -O2 -Wall -DSDCDB -DNOICE -DINDEXLIB -I. -I../asxxsrc -I../../support/util
LDFLAGS		=
LIBS		= -lm

OBJDIR		= obj

UTILLIB		= ../../support/util
UTILSRC		= dbuf.c dbuf_string.c
UTILLIBOBJS	= $(UTILSRC:%.c=$(OBJDIR)/%.o)

ASXXLIB		= ../asxxsrc
ASXXLIBSRC	= asdbg.c asdata.c asexpr.c aslex.c aslist.c asmain.c asout.c assubr.c assym.c asmcro.c sdas.c strcmpi.c
ASXXLIBOBJS	= $(ASXXLIBSRC:%.c=$(OBJDIR)/%.o)

SRC		= m5801pst.c m5801mch.c m5801adr.c
OBJS		= $(SRC:%.c=$(OBJDIR)/%.o)

ASOBJECTS	= $(OBJS) $(ASXXLIBOBJS) $(UTILLIBOBJS)

ASX		= sdaslh5801

# sdld (aslink) is a single generic, port-agnostic linker binary -- not
# part of this port at all -- but the link_test/ regression test (see
# test_link.py) needs one to validate that this port's .rel output
# actually links correctly. Built standalone here for the same
# not-yet-autoconf reason as $(ASX) above; needs the top-level
# sdccconf.h stub (../../sdccconf.h) purely for aslink.h's
# DIR_SEPARATOR_STRING/CHAR macros.
LINKOBJDIR	= objlink
LINKSRC		= ../linksrc
LINKCFLAGS	= -O2 -Wall -DINDEXLIB -DUNIX -I. -I$(LINKSRC) -I../.. -I$(ASXXLIB)
LINKSRCFILES	= lk_readnl.c lkaomf51.c lkar.c lkarea.c lkdata.c lkelf.c lkeval.c \
		  lkhead.c lklex.c lklib.c lklibr.c lklist.c lkmain.c lkmem.c \
		  lknoice.c lkout.c lkrel.c lkrloc.c lkrloc3.c lksdcclib.c \
		  lksym.c sdld.c lksdcdb.c lkbank.c
LINKOBJS	= $(LINKSRCFILES:%.c=$(LINKOBJDIR)/%.o) $(LINKOBJDIR)/strcmpi.o
SDLD		= sdld

all: $(ASX)

$(ASX): $(ASOBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(ASOBJECTS) $(LIBS)

$(SDLD): $(LINKOBJS)
	$(CC) $(LINKCFLAGS) $(LDFLAGS) -o $@ $(LINKOBJS) $(LIBS)

$(OBJDIR)/.stamp:
	mkdir -p $(OBJDIR)
	touch $(OBJDIR)/.stamp

$(LINKOBJDIR)/.stamp:
	mkdir -p $(LINKOBJDIR)
	touch $(LINKOBJDIR)/.stamp

$(OBJDIR)/%.o: %.c $(OBJDIR)/.stamp lh5801.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(ASXXLIB)/%.c $(OBJDIR)/.stamp
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o: $(UTILLIB)/%.c $(OBJDIR)/.stamp
	$(CC) -c $(CFLAGS) -o $@ $<

$(LINKOBJDIR)/%.o: $(LINKSRC)/%.c $(LINKOBJDIR)/.stamp
	$(CC) -c $(LINKCFLAGS) -o $@ $<

$(LINKOBJDIR)/strcmpi.o: $(ASXXLIB)/strcmpi.c $(LINKOBJDIR)/.stamp
	$(CC) -c $(LINKCFLAGS) -o $@ $<

check: $(ASX) $(SDLD)
	python3 test_lh5801.py
	python3 test_link.py

clean:
	rm -rf $(OBJDIR) $(LINKOBJDIR) $(ASX) $(SDLD)

.PHONY: all clean check
