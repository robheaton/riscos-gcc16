# tickmod - stage B1 of the module work: the vector and callback veneers of modkit on the real machine, without sockets
MODULE = TickMod
CMHG   = cmhg/header
SRCS   = c/tickmod
OSLIB_FUNCS = xos_claim xos_release xos_add_call_back xos_remove_call_back xos_read_monotonic_time
include ../../module.mk

# TickWait: an absolute program (loaded and run at &8000) that waits in user mode, so that the Obey file of the hardware run can wait between commands
all: TickWait,ff8
$(BUILD)/tickwait.o: tickwait.s | $(BUILD)
	$(CC) -march=armv6 -c $< -o $@
TickWait,ff8: $(BUILD)/tickwait.o
	$(LD) -Ttext=0x8000 -e _start -static -nostdlib -o $(BUILD)/tickwait.elf $<
	$(BIN)/arm-riscos-gnueabihf-objcopy -O binary $(BUILD)/tickwait.elf $@
