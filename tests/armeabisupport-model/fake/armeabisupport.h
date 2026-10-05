/* replacement for the CMunge-generated armeabisupport.h: the error blocks are real objects here (the generated header casts function addresses to int, which does not work with 64-bit pointers) */
#ifndef FAKE_ARMEABISUPPORT_H
#define FAKE_ARMEABISUPPORT_H
#include "kernel.h"
extern _kernel_oserror mock_err[16];
#define armeabisupport_bad_reason       (&mock_err[0])
#define armeabisupport_bad_param        (&mock_err[1])
#define armeabisupport_no_memory        (&mock_err[2])
#define armeabisupport_bad_process      (&mock_err[3])
#define armeabisupport_in_use           (&mock_err[4])
#define armeabisupport_page_map_error   (&mock_err[5])
#define armeabisupport_EACCES           (&mock_err[6])
#define armeabisupport_EEXIST           (&mock_err[7])
#define armeabisupport_EINVAL           (&mock_err[8])
#define armeabisupport_ENAMETOOLONG     (&mock_err[9])
#define armeabisupport_ENOENT           (&mock_err[10])
#define armeabisupport_ENOSPC           (&mock_err[11])
#define armeabisupport_EBADF            (&mock_err[12])
#define armeabisupport_ENOMEM           (&mock_err[13])
#define armeabisupport_EOPSYS           (&mock_err[14])
#define ARMEABISupport_00 (0x00059d00)
#define ARMEABISupport_MemoryOp 0x59d00
#define ARMEABISupport_AbortOp 0x59d01
#define ARMEABISupport_StackOp 0x59d02
#define ARMEABISupport_Cleanup 0x59d03
#define ARMEABISupport_MMapOp 0x59d04
#define ARMEABISupport_ShmOp 0x59d05
#define error_BAD_SWI ((_kernel_oserror *) -1)
#endif
