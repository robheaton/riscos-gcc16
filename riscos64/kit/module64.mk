# module64.mk - make rules for a 64-bit RISC OS module (the kit).  A Makefile:
#     MODULE = Hello64                 # the module file: Hello64,ffa
#     CMHG   = hello64.cmhg            # the CMHG file (cmunge64 writes the header and  $(MODULE)h.h  from it)
#     SRCS   = hello64.c util.c        # C sources (.c) and C++ sources (.cc .cpp .cxx), assembler (.S)
#     #FP    = 1                       # hardware floating point in the module (-mmodule-fp)
#     #EXTRA_CFLAGS = -Wall
#     include <kit>/kit/module64.mk     (the folder of this file; make -f path or  include $(shell dirname ...) )
# make  builds $(MODULE),ffa;  make clean.   The commands aarch64-riscos-gcc and cmunge64 must be on PATH (install-kit64.sh puts them in PREFIX/bin).
CC64    ?= aarch64-riscos-gcc
CMUNGE  ?= cmunge64
MODFLAG  = $(if $(FP),-mmodule-fp,-mmodule)
OBJDIR  ?= .
HDR      = $(OBJDIR)/$(MODULE)h.h
HDROBJ   = $(OBJDIR)/$(MODULE)_hdr.o
OBJS     = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(notdir $(SRCS)))))
all: $(MODULE),ffa
$(HDR) $(HDROBJ): $(CMHG)
	$(CMUNGE) -p -d $(HDR) -o $(HDROBJ) $(CMHG)
$(OBJDIR)/%.o: %.c $(HDR)
	$(CC64) $(MODFLAG) -O2 -I$(OBJDIR) $(EXTRA_CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: %.cc $(HDR)
	$(CC64) $(MODFLAG) -O2 -I$(OBJDIR) $(EXTRA_CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: %.cpp $(HDR)
	$(CC64) $(MODFLAG) -O2 -I$(OBJDIR) $(EXTRA_CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: %.S
	$(CC64) $(MODFLAG) -c $< -o $@
$(MODULE),ffa: $(OBJS) $(HDROBJ)
	$(CC64) $(MODFLAG) -O2 $(OBJS) $(HDROBJ) -o $(MODULE),ffa
clean:
	rm -f $(OBJS) $(HDR) $(HDROBJ) $(MODULE),ffa
.PHONY: all clean
