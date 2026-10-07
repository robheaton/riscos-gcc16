# module.mk - rules to build a RISC OS module with the GCC 16 EABI tool chain without UnixLib or the Shared C Library (modkit has a small C library of its own), with  gcc -mmodule  (a tool chain of 16.2.0-14 or later; the CMHG options and the C library of 16.2.0-15 need that release) and  cmunge.
#   include this file from the Makefile of a module that defines:  MODULE (the output name), CMHG (the CMHG file), SRCS (C sources, RISC OS style 'c/name' or plain .c); optional: OSLIB_FUNCS (OSLib functions to make veneers for)
#   result: $(MODULE),ffa   (the flat module image; load it with RMLoad)
MODKIT   ?= $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
# the bin folder of the tool chain: the one this module.mk was installed in (<tc>/share/riscos-modkit/module.mk), else the work area's
BIN      ?= $(if $(wildcard $(MODKIT)../../bin/arm-riscos-gnueabihf-gcc),$(abspath $(MODKIT)../../bin),$(HOME)/gccsdk-next/env-f/bin)
# make's built-in CC / LD / AR (cc, ld, ar) are for the host: replace them unless the user gave one on the command line or in the environment
ifeq ($(origin CC),default)
CC = $(BIN)/arm-riscos-gnueabihf-gcc
endif
ifeq ($(origin LD),default)
LD = $(BIN)/arm-riscos-gnueabihf-ld
endif
ifeq ($(origin AR),default)
AR = $(BIN)/arm-riscos-gnueabihf-ar
endif
OSLIB    ?= $(HOME)/gccsdk/env/include
BUILD    ?= build
GCCINC   := $(shell $(CC) -print-file-name=include)
# -mmodule: ARMv6, soft float, ARM state, freestanding, no PIC, no stack protector, the headers of modkit (see riscos-gnueabihf.h of the compiler); the kit's own include folder first, so that its changes count
MODCFLAGS = -mmodule -O2 -std=gnu99 -Wall -isystem $(MODKIT)include -I$(OSLIB) -I$(BUILD) $(EXTRA_CFLAGS)
# the tools of the tool chain (C programs: install-modkit.sh puts them in its bin/); the Python versions in this kit's bin/ do the same when a tool chain has only the older install
CMUNGE   ?= $(if $(wildcard $(BIN)/cmunge),$(BIN)/cmunge,python3 $(MODKIT)bin/cmunge)
MKOSLIB  ?= $(if $(wildcard $(BIN)/arm-riscos-gnueabihf-mkoslib),$(BIN)/arm-riscos-gnueabihf-mkoslib,python3 $(MODKIT)bin/mkoslib.py)
MODRELOC ?= $(if $(wildcard $(BIN)/arm-riscos-gnueabihf-modreloc),$(BIN)/arm-riscos-gnueabihf-modreloc,python3 $(MODKIT)bin/modreloc.py)
OBJS      = $(patsubst %,$(BUILD)/%.o,$(notdir $(SRCS))) $(BUILD)/header.o $(BUILD)/oslibv.o
# the C library of the kit, built here from the kit's own sources (so that a change of the kit counts at once): one object per source, in an archive that is linked after the module's objects.  The driver
# links the installed libmodkit.a (the same library and then libgcc) after that, for what is still missing.
LIBSRCS   = $(wildcard $(MODKIT)lib/*.c) $(wildcard $(MODKIT)lib/*.S)
LIBOBJS   = $(patsubst $(MODKIT)lib/%,$(BUILD)/lib/%.o,$(LIBSRCS))
LOCALLIB  = $(BUILD)/libmodkit-local.a

all: $(MODULE),ffa

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/header.s $(BUILD)/header.h &: $(CMHG) | $(BUILD)
	CMUNGE_CC=$(CC) $(CMUNGE) -tgcc -32bit -p -d $(BUILD)/header.h -s $(BUILD)/header.s $(CMHG)

$(BUILD)/header.o: $(BUILD)/header.s
	$(CC) -mmodule -c $< -o $@

$(BUILD)/lib: | $(BUILD)
	mkdir -p $@

$(BUILD)/lib/%.c.o: $(MODKIT)lib/%.c | $(BUILD)/lib
	$(CC) $(MODCFLAGS) -c $< -o $@

$(BUILD)/lib/%.S.o: $(MODKIT)lib/%.S | $(BUILD)/lib
	$(CC) -march=armv6 -c $< -o $@

$(LOCALLIB): $(LIBOBJS)
	rm -f $@
	$(AR) rcs $@ $^

# the OSLib veneers: every OSLib X-function that the sources use (the undefined symbols of their objects), plus the ones named in OSLIB_FUNCS (for a function that only a library of yours calls)
SRC_OBJS  = $(patsubst %,$(BUILD)/%.o,$(notdir $(SRCS)))
$(BUILD)/oslibv.c: $(SRC_OBJS) | $(BUILD)
	$(MKOSLIB) -I $(OSLIB)/oslib -o $@ $(OSLIB_FUNCS) --from-objects $(SRC_OBJS)

$(BUILD)/oslibv.o: $(BUILD)/oslibv.c $(BUILD)/header.h
	$(CC) $(MODCFLAGS) -c $< -o $@

# the C sources: 'c/name' (RISC OS style, no extension) or 'name.c'
define COMPILE
$$(BUILD)/$(notdir $(1)).o: $(1) $$(BUILD)/header.h | $$(BUILD)
	$$(CC) $$(MODCFLAGS) -x c -c $(1) -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call COMPILE,$(s))))

# the driver adds the linker script of the tool chain (module.ld) and links libmodkit.a (and libgcc) after the objects; the kit's own library comes first, so the kit's own sources win
$(BUILD)/$(MODULE).elf: $(OBJS) $(LOCALLIB)
	$(CC) -mmodule -o $@ $(OBJS) $(LOCALLIB)

$(MODULE),ffa: $(BUILD)/$(MODULE).elf
	$(MODRELOC) $< $@

clean:
	rm -rf $(BUILD) "$(MODULE),ffa"
.PHONY: all clean
