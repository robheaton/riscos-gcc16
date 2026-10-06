# module.mk - rules to build a RISC OS module with the GCC 16 EABI tool chain without any C library (modkit), with  gcc -mmodule  (a tool chain of 16.2.0-14 or later) and  cmunge.
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
OBJS      = $(patsubst %,$(BUILD)/%.o,$(notdir $(SRCS))) $(BUILD)/header.o $(BUILD)/modlib.o $(BUILD)/divmod.o $(BUILD)/modswi.o $(BUILD)/oslibv.o

all: $(MODULE),ffa

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/header.s $(BUILD)/header.h &: $(CMHG) | $(BUILD)
	CMUNGE_CC=$(CC) $(CMUNGE) -tgcc -32bit -p -d $(BUILD)/header.h -s $(BUILD)/header.s $(CMHG)

$(BUILD)/header.o: $(BUILD)/header.s
	$(CC) -mmodule -c $< -o $@

$(BUILD)/modlib.o: $(MODKIT)lib/modlib.c | $(BUILD)
	$(CC) $(MODCFLAGS) -c $< -o $@

$(BUILD)/divmod.o: $(MODKIT)lib/divmod.c | $(BUILD)
	$(CC) $(MODCFLAGS) -c $< -o $@

$(BUILD)/modswi.o: $(MODKIT)lib/modswi.S | $(BUILD)
	$(CC) -march=armv6 -c $< -o $@

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

# the driver adds the linker script of the tool chain (module.ld) and links libmodkit.a after the objects; the three library objects above come first, so the kit's own sources win
$(BUILD)/$(MODULE).elf: $(OBJS)
	$(CC) -mmodule -o $@ $(OBJS) 2>&1 | grep -v "RWX permissions\|dynamic-undefined-weak" || true

$(MODULE),ffa: $(BUILD)/$(MODULE).elf
	$(MODRELOC) $< $@

clean:
	rm -rf $(BUILD) "$(MODULE),ffa"
.PHONY: all clean
