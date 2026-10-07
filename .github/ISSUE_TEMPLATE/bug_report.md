---
name: Bug report
about: A compile fails, a program built by this tool chain misbehaves, or the installation does not work
title: ""
labels: bug
assignees: ""
---

**Which compiler?**  Native (on RISC OS) / Linux cross compiler

**What did you do?**
The exact command, and a small source file that shows the problem if you can.

**What did you expect, and what happened?**
Copy the messages from the Task window (or the terminal).

**Your set-up** (this saves a round trip)

- Package versions: the output of `Echo <GCC16$Version>` in a Task window (or the name of the cross compiler tarball)
- The runtime's fix level: the output of `fixlevel 15` ([how](https://github.com/robheaton/riscos-gcc16/blob/main/docs/INSTALL-RISCOS.md#3-check-it))
- RISC OS version and machine (for example RISC OS 5.30 on a Raspberry Pi 4):
- ARMEABISupport, Shared Object Manager and SharedUnixLibrary versions (`*Modules`):
- Did you reboot after installing, and double-click `!GCC16` after the reboot?
- The Task window's size (`WimpSlot`), and whether a text editor or other tasks were running:

Not a bug in this tool chain, but in GCCSDK's own 10.2.0 compiler, in UnixLib's upstream sources or in RISC OS itself? Please still tell us: the [upstream reports](https://github.com/robheaton/riscos-gcc16/blob/main/docs/UPSTREAM.md) show what has been found so far.
