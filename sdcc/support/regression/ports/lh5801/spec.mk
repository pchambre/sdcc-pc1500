# Regression test specification for the lh5801 target (Sharp PC-1500),
# running on pc1500sim -- pc1500emu's tools/sim, a bare LH5801 in a flat
# 64K of RAM. Build it with pc1500emu's CMake, or on its own:
#   g++ -O2 -std=c++20 -Isrc/cpu tools/sim/sim.cpp src/cpu/lh5801.cpp -o pc1500sim
# and put it on PATH or pass PC1500SIM=/path/to/pc1500sim.
#
# Tests link with support/scripts/build-lh5801.sh (this port's two-pass
# area layout and its runtime library), loaded at 0100H.

PC1500SIM ?= pc1500sim
SIM_CYCLES = 2000000000
EMU = $(PC1500SIM) -c $(SIM_CYCLES)
# not uCsim.cmd: pc1500sim reads nothing from stdin
EMU_INPUT =
BUILD_LH5801 = $(top_srcdir)/support/scripts/build-lh5801.sh
LOAD_ADDR = 0100

SDCCFLAGS += -mlh5801 --less-pedantic -I$(top_srcdir) \
             -I$(top_srcdir)/device/include -I$(top_srcdir)/device/include/lh5801

OBJEXT = .rel
BINEXT = .bin

# otherwise `make` deletes testfwk.rel and `make -j` will fail
.PRECIOUS: $(PORT_CASES_DIR)/%$(OBJEXT)

# Required extras
EXTRAS = $(PORT_CASES_DIR)/testfwk$(OBJEXT) $(PORT_CASES_DIR)/support$(OBJEXT)
include $(srcdir)/fwk/lib/spec.mk

# Link: build-lh5801.sh names its output after the first object, in the
# current directory, so each test links in its own directory.
# (build-lh5801.sh as a prerequisite also keeps Makefile.in's generic
# sdcc-driver rule, with otherwise the same prerequisites, from replacing this.)
%$(BINEXT): %$(OBJEXT) $(EXTRAS) $(FWKLIB) $(PORT_CASES_DIR)/fwk.lib $(BUILD_LH5801)
	cd $(dir $@) && $(abspath $(BUILD_LH5801)) $(abspath $< $(EXTRAS) $(PORT_CASES_DIR)/fwk.lib) $(LOAD_ADDR) > /dev/null
	mv $(basename $@)-$(LOAD_ADDR)-pc1500.bin $@
	mv $(basename $@)-$(LOAD_ADDR)-pc1500.map $(basename $@).map

%$(OBJEXT): %.c
	$(VALGRIND) $(SDCC) $(SDCCFLAGS) -c $< -o $@

$(PORT_CASES_DIR)/%$(OBJEXT): $(PORTS_DIR)/$(PORT)/%.c
	$(SDCC) $(SDCCFLAGS) -c $< -o $@

$(PORT_CASES_DIR)/%$(OBJEXT): $(srcdir)/fwk/lib/%.c
	$(SDCC) $(SDCCFLAGS) -c $< -o $@

$(PORT_CASES_DIR)/fwk.lib: $(srcdir)/fwk/lib/fwk.lib
	cat < $(srcdir)/fwk/lib/fwk.lib > $@

_clean:
