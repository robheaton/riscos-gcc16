# the compiler options of a 64-bit module (sourced by build-kit.sh and by aarch64-riscos-gcc -mmodule): no library, no floating point or vector register (the state of those at EL1 is not known: module test
# 0.4, Mod64F), position independent without a GOT (the module relocates its own pointers), x18 reserved.
MODFLAGS="-ffreestanding -fno-pic -fno-pie -fvisibility=hidden -mcmodel=small -mgeneral-regs-only -mstrict-align -ffixed-x18 -mno-outline-atomics -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-tree-loop-distribute-patterns -D__riscos64__ -DMODULE"
# -mmodule-fp (aarch64-riscos-gcc): hardware floating point and vector registers allowed in the module (Pyromaniac keeps the caller's v0-v31 and FPCR across a SWI of a module; other entries were not tested).
MODFLAGS_FP=$(echo "$MODFLAGS" | sed 's/-mgeneral-regs-only //')
