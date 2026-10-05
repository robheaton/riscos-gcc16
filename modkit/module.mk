# module.mk - rules to build a RISC OS module with the GCC 16 EABI tool chain without any C library (modkit).
#   include this file from the Makefile of a module that defines:  MODULE (the output name), CMHG (the CMHG file), SRCS (C sources, RISC OS style 'c/name' or plain .c), OSLIB_FUNCS (OSLib functions to make veneers for)
#   result: $(MODULE),ffa   (the flat module image; load it with RMLoad)
MODKIT   ?= $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BIN      ?= $(HOME)/gccsdk-next/env-f/bin
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
MODCFLAGS = -O2 -std=gnu99 -march=armv6 -mfloat-abi=soft -marm -ffreestanding -fno-pic -fno-pie -fvisibility=hidden -fno-stack-clash-protection -fno-stack-protector \
            -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-builtin -fno-tree-loop-distribute-patterns -Wall \
            -nostdinc -isystem $(MODKIT)include -isystem $(GCCINC) -I$(OSLIB) -I$(BUILD) $(EXTRA_CFLAGS)
OBJS      = $(patsubst %,$(BUILD)/%.o,$(notdir $(SRCS))) $(BUILD)/header.o $(BUILD)/modlib.o $(BUILD)/divmod.o $(BUILD)/modswi.o $(if $(OSLIB_FUNCS),$(BUILD)/oslibv.o)

all: $(MODULE),ffa

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/header.s $(BUILD)/header.h: $(CMHG) $(MODKIT)bin/mkmodhdr.py | $(BUILD)
	python3 $(MODKIT)bin/mkmodhdr.py -s $(BUILD)/header.s -d $(BUILD)/header.h $(CMHG)

$(BUILD)/header.o: $(BUILD)/header.s
	$(CC) -march=armv6 -c $< -o $@

$(BUILD)/modlib.o: $(MODKIT)lib/modlib.c | $(BUILD)
	$(CC) $(MODCFLAGS) -c $< -o $@

$(BUILD)/divmod.o: $(MODKIT)lib/divmod.c | $(BUILD)
	$(CC) $(MODCFLAGS) -c $< -o $@

$(BUILD)/modswi.o: $(MODKIT)lib/modswi.S | $(BUILD)
	$(CC) -march=armv6 -c $< -o $@

$(BUILD)/oslibv.c: $(firstword $(MAKEFILE_LIST)) $(MODKIT)bin/mkoslib.py | $(BUILD)
	python3 $(MODKIT)bin/mkoslib.py -I $(OSLIB)/oslib -o $@ $(OSLIB_FUNCS)

$(BUILD)/oslibv.o: $(BUILD)/oslibv.c $(BUILD)/header.h
	$(CC) $(MODCFLAGS) -c $< -o $@

# the C sources: 'c/name' (RISC OS style, no extension) or 'name.c'
define COMPILE
$$(BUILD)/$(notdir $(1)).o: $(1) $$(BUILD)/header.h | $$(BUILD)
	$$(CC) $$(MODCFLAGS) -x c -c $(1) -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call COMPILE,$(s))))

$(BUILD)/$(MODULE).elf: $(OBJS) $(MODKIT)lib/module.ld
	$(LD) -T $(MODKIT)lib/module.ld -static -nostdlib -q -o $@ $(OBJS) 2>&1 | grep -v "RWX permissions\|dynamic-undefined-weak" || true

$(MODULE),ffa: $(BUILD)/$(MODULE).elf
	python3 $(MODKIT)bin/modreloc.py $< $@

clean:
	rm -rf $(BUILD) "$(MODULE),ffa"
.PHONY: all clean
