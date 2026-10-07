/* swisnums.h - the SWI numbers of the RISC OS Open sources: 951 SWIs from 69 assembler headers, with their X versions (bit 17 set), as the SharedCLibrary's <swis.h> defines them.
   Made by modkit/bin/mkswis.py from the hdr/ folders of https://gitlab.riscosopen.org/RiscOS/Sources (Apache License 2.0, Copyright Castle Technology Ltd and RISC OS Open Ltd and others).  DO NOT EDIT. */
#ifndef _SWISNUMS_H
#define _SWISNUMS_H

#undef OS_WriteC
#define OS_WriteC 0x0
#undef XOS_WriteC
#define XOS_WriteC 0x20000
#undef OS_WriteS
#define OS_WriteS 0x1
#undef XOS_WriteS
#define XOS_WriteS 0x20001
#undef OS_Write0
#define OS_Write0 0x2
#undef XOS_Write0
#define XOS_Write0 0x20002
#undef OS_NewLine
#define OS_NewLine 0x3
#undef XOS_NewLine
#define XOS_NewLine 0x20003
#undef OS_ReadC
#define OS_ReadC 0x4
#undef XOS_ReadC
#define XOS_ReadC 0x20004
#undef OS_CLI
#define OS_CLI 0x5
#undef XOS_CLI
#define XOS_CLI 0x20005
#undef OS_Byte
#define OS_Byte 0x6
#undef XOS_Byte
#define XOS_Byte 0x20006
#undef OS_Word
#define OS_Word 0x7
#undef XOS_Word
#define XOS_Word 0x20007
#undef OS_File
#define OS_File 0x8
#undef XOS_File
#define XOS_File 0x20008
#undef OS_Args
#define OS_Args 0x9
#undef XOS_Args
#define XOS_Args 0x20009
#undef OS_BGet
#define OS_BGet 0xA
#undef XOS_BGet
#define XOS_BGet 0x2000A
#undef OS_BPut
#define OS_BPut 0xB
#undef XOS_BPut
#define XOS_BPut 0x2000B
#undef OS_GBPB
#define OS_GBPB 0xC
#undef XOS_GBPB
#define XOS_GBPB 0x2000C
#undef OS_Find
#define OS_Find 0xD
#undef XOS_Find
#define XOS_Find 0x2000D
#undef OS_ReadLine
#define OS_ReadLine 0xE
#undef XOS_ReadLine
#define XOS_ReadLine 0x2000E
#undef OS_Control
#define OS_Control 0xF
#undef XOS_Control
#define XOS_Control 0x2000F
#undef OS_GetEnv
#define OS_GetEnv 0x10
#undef XOS_GetEnv
#define XOS_GetEnv 0x20010
#undef OS_Exit
#define OS_Exit 0x11
#undef XOS_Exit
#define XOS_Exit 0x20011
#undef OS_SetEnv
#define OS_SetEnv 0x12
#undef XOS_SetEnv
#define XOS_SetEnv 0x20012
#undef OS_IntOn
#define OS_IntOn 0x13
#undef XOS_IntOn
#define XOS_IntOn 0x20013
#undef OS_IntOff
#define OS_IntOff 0x14
#undef XOS_IntOff
#define XOS_IntOff 0x20014
#undef OS_CallBack
#define OS_CallBack 0x15
#undef XOS_CallBack
#define XOS_CallBack 0x20015
#undef OS_EnterOS
#define OS_EnterOS 0x16
#undef XOS_EnterOS
#define XOS_EnterOS 0x20016
#undef OS_BreakPt
#define OS_BreakPt 0x17
#undef XOS_BreakPt
#define XOS_BreakPt 0x20017
#undef OS_BreakCtrl
#define OS_BreakCtrl 0x18
#undef XOS_BreakCtrl
#define XOS_BreakCtrl 0x20018
#undef OS_UnusedSWI
#define OS_UnusedSWI 0x19
#undef XOS_UnusedSWI
#define XOS_UnusedSWI 0x20019
#undef OS_UpdateMEMC
#define OS_UpdateMEMC 0x1A
#undef XOS_UpdateMEMC
#define XOS_UpdateMEMC 0x2001A
#undef OS_SetCallBack
#define OS_SetCallBack 0x1B
#undef XOS_SetCallBack
#define XOS_SetCallBack 0x2001B
#undef OS_Mouse
#define OS_Mouse 0x1C
#undef XOS_Mouse
#define XOS_Mouse 0x2001C
#undef OS_Heap
#define OS_Heap 0x1D
#undef XOS_Heap
#define XOS_Heap 0x2001D
#undef OS_Module
#define OS_Module 0x1E
#undef XOS_Module
#define XOS_Module 0x2001E
#undef OS_Claim
#define OS_Claim 0x1F
#undef XOS_Claim
#define XOS_Claim 0x2001F
#undef OS_Release
#define OS_Release 0x20
#undef XOS_Release
#define XOS_Release 0x20020
#undef OS_ReadUnsigned
#define OS_ReadUnsigned 0x21
#undef XOS_ReadUnsigned
#define XOS_ReadUnsigned 0x20021
#undef OS_GenerateEvent
#define OS_GenerateEvent 0x22
#undef XOS_GenerateEvent
#define XOS_GenerateEvent 0x20022
#undef OS_ReadVarVal
#define OS_ReadVarVal 0x23
#undef XOS_ReadVarVal
#define XOS_ReadVarVal 0x20023
#undef OS_SetVarVal
#define OS_SetVarVal 0x24
#undef XOS_SetVarVal
#define XOS_SetVarVal 0x20024
#undef OS_GSInit
#define OS_GSInit 0x25
#undef XOS_GSInit
#define XOS_GSInit 0x20025
#undef OS_GSRead
#define OS_GSRead 0x26
#undef XOS_GSRead
#define XOS_GSRead 0x20026
#undef OS_GSTrans
#define OS_GSTrans 0x27
#undef XOS_GSTrans
#define XOS_GSTrans 0x20027
#undef OS_BinaryToDecimal
#define OS_BinaryToDecimal 0x28
#undef XOS_BinaryToDecimal
#define XOS_BinaryToDecimal 0x20028
#undef OS_FSControl
#define OS_FSControl 0x29
#undef XOS_FSControl
#define XOS_FSControl 0x20029
#undef OS_ChangeDynamicArea
#define OS_ChangeDynamicArea 0x2A
#undef XOS_ChangeDynamicArea
#define XOS_ChangeDynamicArea 0x2002A
#undef OS_GenerateError
#define OS_GenerateError 0x2B
#undef XOS_GenerateError
#define XOS_GenerateError 0x2002B
#undef OS_ReadEscapeState
#define OS_ReadEscapeState 0x2C
#undef XOS_ReadEscapeState
#define XOS_ReadEscapeState 0x2002C
#undef OS_EvaluateExpression
#define OS_EvaluateExpression 0x2D
#undef XOS_EvaluateExpression
#define XOS_EvaluateExpression 0x2002D
#undef OS_SpriteOp
#define OS_SpriteOp 0x2E
#undef XOS_SpriteOp
#define XOS_SpriteOp 0x2002E
#undef OS_ReadPalette
#define OS_ReadPalette 0x2F
#undef XOS_ReadPalette
#define XOS_ReadPalette 0x2002F
#undef OS_ServiceCall
#define OS_ServiceCall 0x30
#undef XOS_ServiceCall
#define XOS_ServiceCall 0x20030
#undef OS_ReadVduVariables
#define OS_ReadVduVariables 0x31
#undef XOS_ReadVduVariables
#define XOS_ReadVduVariables 0x20031
#undef OS_ReadPoint
#define OS_ReadPoint 0x32
#undef XOS_ReadPoint
#define XOS_ReadPoint 0x20032
#undef OS_UpCall
#define OS_UpCall 0x33
#undef XOS_UpCall
#define XOS_UpCall 0x20033
#undef OS_CallAVector
#define OS_CallAVector 0x34
#undef XOS_CallAVector
#define XOS_CallAVector 0x20034
#undef OS_ReadModeVariable
#define OS_ReadModeVariable 0x35
#undef XOS_ReadModeVariable
#define XOS_ReadModeVariable 0x20035
#undef OS_RemoveCursors
#define OS_RemoveCursors 0x36
#undef XOS_RemoveCursors
#define XOS_RemoveCursors 0x20036
#undef OS_RestoreCursors
#define OS_RestoreCursors 0x37
#undef XOS_RestoreCursors
#define XOS_RestoreCursors 0x20037
#undef OS_SWINumberToString
#define OS_SWINumberToString 0x38
#undef XOS_SWINumberToString
#define XOS_SWINumberToString 0x20038
#undef OS_SWINumberFromString
#define OS_SWINumberFromString 0x39
#undef XOS_SWINumberFromString
#define XOS_SWINumberFromString 0x20039
#undef OS_ValidateAddress
#define OS_ValidateAddress 0x3A
#undef XOS_ValidateAddress
#define XOS_ValidateAddress 0x2003A
#undef OS_CallAfter
#define OS_CallAfter 0x3B
#undef XOS_CallAfter
#define XOS_CallAfter 0x2003B
#undef OS_CallEvery
#define OS_CallEvery 0x3C
#undef XOS_CallEvery
#define XOS_CallEvery 0x2003C
#undef OS_RemoveTickerEvent
#define OS_RemoveTickerEvent 0x3D
#undef XOS_RemoveTickerEvent
#define XOS_RemoveTickerEvent 0x2003D
#undef OS_InstallKeyHandler
#define OS_InstallKeyHandler 0x3E
#undef XOS_InstallKeyHandler
#define XOS_InstallKeyHandler 0x2003E
#undef OS_CheckModeValid
#define OS_CheckModeValid 0x3F
#undef XOS_CheckModeValid
#define XOS_CheckModeValid 0x2003F
#undef OS_ChangeEnvironment
#define OS_ChangeEnvironment 0x40
#undef XOS_ChangeEnvironment
#define XOS_ChangeEnvironment 0x20040
#undef OS_ClaimScreenMemory
#define OS_ClaimScreenMemory 0x41
#undef XOS_ClaimScreenMemory
#define XOS_ClaimScreenMemory 0x20041
#undef OS_ReadMonotonicTime
#define OS_ReadMonotonicTime 0x42
#undef XOS_ReadMonotonicTime
#define XOS_ReadMonotonicTime 0x20042
#undef OS_SubstituteArgs
#define OS_SubstituteArgs 0x43
#undef XOS_SubstituteArgs
#define XOS_SubstituteArgs 0x20043
#undef OS_PrettyPrint
#define OS_PrettyPrint 0x44
#undef XOS_PrettyPrint
#define XOS_PrettyPrint 0x20044
#undef OS_Plot
#define OS_Plot 0x45
#undef XOS_Plot
#define XOS_Plot 0x20045
#undef OS_WriteN
#define OS_WriteN 0x46
#undef XOS_WriteN
#define XOS_WriteN 0x20046
#undef OS_AddToVector
#define OS_AddToVector 0x47
#undef XOS_AddToVector
#define XOS_AddToVector 0x20047
#undef OS_WriteEnv
#define OS_WriteEnv 0x48
#undef XOS_WriteEnv
#define XOS_WriteEnv 0x20048
#undef OS_ReadArgs
#define OS_ReadArgs 0x49
#undef XOS_ReadArgs
#define XOS_ReadArgs 0x20049
#undef OS_ReadRAMFsLimits
#define OS_ReadRAMFsLimits 0x4A
#undef XOS_ReadRAMFsLimits
#define XOS_ReadRAMFsLimits 0x2004A
#undef OS_ClaimDeviceVector
#define OS_ClaimDeviceVector 0x4B
#undef XOS_ClaimDeviceVector
#define XOS_ClaimDeviceVector 0x2004B
#undef OS_ReleaseDeviceVector
#define OS_ReleaseDeviceVector 0x4C
#undef XOS_ReleaseDeviceVector
#define XOS_ReleaseDeviceVector 0x2004C
#undef OS_DelinkApplication
#define OS_DelinkApplication 0x4D
#undef XOS_DelinkApplication
#define XOS_DelinkApplication 0x2004D
#undef OS_RelinkApplication
#define OS_RelinkApplication 0x4E
#undef XOS_RelinkApplication
#define XOS_RelinkApplication 0x2004E
#undef OS_HeapSort
#define OS_HeapSort 0x4F
#undef XOS_HeapSort
#define XOS_HeapSort 0x2004F
#undef OS_ExitAndDie
#define OS_ExitAndDie 0x50
#undef XOS_ExitAndDie
#define XOS_ExitAndDie 0x20050
#undef OS_ReadMemMapInfo
#define OS_ReadMemMapInfo 0x51
#undef XOS_ReadMemMapInfo
#define XOS_ReadMemMapInfo 0x20051
#undef OS_ReadMemMapEntries
#define OS_ReadMemMapEntries 0x52
#undef XOS_ReadMemMapEntries
#define XOS_ReadMemMapEntries 0x20052
#undef OS_SetMemMapEntries
#define OS_SetMemMapEntries 0x53
#undef XOS_SetMemMapEntries
#define XOS_SetMemMapEntries 0x20053
#undef OS_AddCallBack
#define OS_AddCallBack 0x54
#undef XOS_AddCallBack
#define XOS_AddCallBack 0x20054
#undef OS_ReadDefaultHandler
#define OS_ReadDefaultHandler 0x55
#undef XOS_ReadDefaultHandler
#define XOS_ReadDefaultHandler 0x20055
#undef OS_SetECFOrigin
#define OS_SetECFOrigin 0x56
#undef XOS_SetECFOrigin
#define XOS_SetECFOrigin 0x20056
#undef OS_SerialOp
#define OS_SerialOp 0x57
#undef XOS_SerialOp
#define XOS_SerialOp 0x20057
#undef OS_ReadSysInfo
#define OS_ReadSysInfo 0x58
#undef XOS_ReadSysInfo
#define XOS_ReadSysInfo 0x20058
#undef OS_Confirm
#define OS_Confirm 0x59
#undef XOS_Confirm
#define XOS_Confirm 0x20059
#undef OS_ChangedBox
#define OS_ChangedBox 0x5A
#undef XOS_ChangedBox
#define XOS_ChangedBox 0x2005A
#undef OS_CRC
#define OS_CRC 0x5B
#undef XOS_CRC
#define XOS_CRC 0x2005B
#undef OS_ReadDynamicArea
#define OS_ReadDynamicArea 0x5C
#undef XOS_ReadDynamicArea
#define XOS_ReadDynamicArea 0x2005C
#undef OS_PrintChar
#define OS_PrintChar 0x5D
#undef XOS_PrintChar
#define XOS_PrintChar 0x2005D
#undef OS_ChangeRedirection
#define OS_ChangeRedirection 0x5E
#undef XOS_ChangeRedirection
#define XOS_ChangeRedirection 0x2005E
#undef OS_RemoveCallBack
#define OS_RemoveCallBack 0x5F
#undef XOS_RemoveCallBack
#define XOS_RemoveCallBack 0x2005F
#undef OS_FindMemMapEntries
#define OS_FindMemMapEntries 0x60
#undef XOS_FindMemMapEntries
#define XOS_FindMemMapEntries 0x20060
#undef OS_SetColour
#define OS_SetColour 0x61
#undef XOS_SetColour
#define XOS_SetColour 0x20061
#undef OS_ClaimSWI
#define OS_ClaimSWI 0x62
#undef XOS_ClaimSWI
#define XOS_ClaimSWI 0x20062
#undef OS_ReleaseSWI
#define OS_ReleaseSWI 0x63
#undef XOS_ReleaseSWI
#define XOS_ReleaseSWI 0x20063
#undef OS_Pointer
#define OS_Pointer 0x64
#undef XOS_Pointer
#define XOS_Pointer 0x20064
#undef OS_ScreenMode
#define OS_ScreenMode 0x65
#undef XOS_ScreenMode
#define XOS_ScreenMode 0x20065
#undef OS_DynamicArea
#define OS_DynamicArea 0x66
#undef XOS_DynamicArea
#define XOS_DynamicArea 0x20066
#undef OS_AbortTrap
#define OS_AbortTrap 0x67
#undef XOS_AbortTrap
#define XOS_AbortTrap 0x20067
#undef OS_Memory
#define OS_Memory 0x68
#undef XOS_Memory
#define XOS_Memory 0x20068
#undef OS_ClaimProcessorVector
#define OS_ClaimProcessorVector 0x69
#undef XOS_ClaimProcessorVector
#define XOS_ClaimProcessorVector 0x20069
#undef OS_Reset
#define OS_Reset 0x6A
#undef XOS_Reset
#define XOS_Reset 0x2006A
#undef OS_MMUControl
#define OS_MMUControl 0x6B
#undef XOS_MMUControl
#define XOS_MMUControl 0x2006B
#undef OS_ResyncTime
#define OS_ResyncTime 0x6C
#undef XOS_ResyncTime
#define XOS_ResyncTime 0x2006C
#undef OS_PlatformFeatures
#define OS_PlatformFeatures 0x6D
#undef XOS_PlatformFeatures
#define XOS_PlatformFeatures 0x2006D
#undef OS_SynchroniseCodeAreas
#define OS_SynchroniseCodeAreas 0x6E
#undef XOS_SynchroniseCodeAreas
#define XOS_SynchroniseCodeAreas 0x2006E
#undef OS_CallASWI
#define OS_CallASWI 0x6F
#undef XOS_CallASWI
#define XOS_CallASWI 0x2006F
#undef OS_AMBControl
#define OS_AMBControl 0x70
#undef XOS_AMBControl
#define XOS_AMBControl 0x20070
#undef OS_CallASWIR12
#define OS_CallASWIR12 0x71
#undef XOS_CallASWIR12
#define XOS_CallASWIR12 0x20071
#undef OS_SpecialControl
#define OS_SpecialControl 0x72
#undef XOS_SpecialControl
#define XOS_SpecialControl 0x20072
#undef OS_EnterUSR32
#define OS_EnterUSR32 0x73
#undef XOS_EnterUSR32
#define XOS_EnterUSR32 0x20073
#undef OS_EnterUSR26
#define OS_EnterUSR26 0x74
#undef XOS_EnterUSR26
#define XOS_EnterUSR26 0x20074
#undef OS_VIDCDivider
#define OS_VIDCDivider 0x75
#undef XOS_VIDCDivider
#define XOS_VIDCDivider 0x20075
#undef OS_NVMemory
#define OS_NVMemory 0x76
#undef XOS_NVMemory
#define XOS_NVMemory 0x20076
#undef OS_TaskControl
#define OS_TaskControl 0x78
#undef XOS_TaskControl
#define XOS_TaskControl 0x20078
#undef OS_Hardware
#define OS_Hardware 0x7A
#undef XOS_Hardware
#define XOS_Hardware 0x2007A
#undef OS_IICOp
#define OS_IICOp 0x7B
#undef XOS_IICOp
#define XOS_IICOp 0x2007B
#undef OS_LeaveOS
#define OS_LeaveOS 0x7C
#undef XOS_LeaveOS
#define XOS_LeaveOS 0x2007C
#undef OS_ReadLine32
#define OS_ReadLine32 0x7D
#undef XOS_ReadLine32
#define XOS_ReadLine32 0x2007D
#undef OS_SubstituteArgs32
#define OS_SubstituteArgs32 0x7E
#undef XOS_SubstituteArgs32
#define XOS_SubstituteArgs32 0x2007E
#undef OS_HeapSort32
#define OS_HeapSort32 0x7F
#undef XOS_HeapSort32
#define XOS_HeapSort32 0x2007F
#undef OS_ConvertStandardDateAndTime
#define OS_ConvertStandardDateAndTime 0xC0
#undef XOS_ConvertStandardDateAndTime
#define XOS_ConvertStandardDateAndTime 0x200C0
#undef OS_ConvertDateAndTime
#define OS_ConvertDateAndTime 0xC1
#undef XOS_ConvertDateAndTime
#define XOS_ConvertDateAndTime 0x200C1
#undef OS_ConvertHex1
#define OS_ConvertHex1 0xD0
#undef XOS_ConvertHex1
#define XOS_ConvertHex1 0x200D0
#undef OS_ConvertHex2
#define OS_ConvertHex2 0xD1
#undef XOS_ConvertHex2
#define XOS_ConvertHex2 0x200D1
#undef OS_ConvertHex4
#define OS_ConvertHex4 0xD2
#undef XOS_ConvertHex4
#define XOS_ConvertHex4 0x200D2
#undef OS_ConvertHex6
#define OS_ConvertHex6 0xD3
#undef XOS_ConvertHex6
#define XOS_ConvertHex6 0x200D3
#undef OS_ConvertHex8
#define OS_ConvertHex8 0xD4
#undef XOS_ConvertHex8
#define XOS_ConvertHex8 0x200D4
#undef OS_ConvertCardinal1
#define OS_ConvertCardinal1 0xD5
#undef XOS_ConvertCardinal1
#define XOS_ConvertCardinal1 0x200D5
#undef OS_ConvertCardinal2
#define OS_ConvertCardinal2 0xD6
#undef XOS_ConvertCardinal2
#define XOS_ConvertCardinal2 0x200D6
#undef OS_ConvertCardinal3
#define OS_ConvertCardinal3 0xD7
#undef XOS_ConvertCardinal3
#define XOS_ConvertCardinal3 0x200D7
#undef OS_ConvertCardinal4
#define OS_ConvertCardinal4 0xD8
#undef XOS_ConvertCardinal4
#define XOS_ConvertCardinal4 0x200D8
#undef OS_ConvertInteger1
#define OS_ConvertInteger1 0xD9
#undef XOS_ConvertInteger1
#define XOS_ConvertInteger1 0x200D9
#undef OS_ConvertInteger2
#define OS_ConvertInteger2 0xDA
#undef XOS_ConvertInteger2
#define XOS_ConvertInteger2 0x200DA
#undef OS_ConvertInteger3
#define OS_ConvertInteger3 0xDB
#undef XOS_ConvertInteger3
#define XOS_ConvertInteger3 0x200DB
#undef OS_ConvertInteger4
#define OS_ConvertInteger4 0xDC
#undef XOS_ConvertInteger4
#define XOS_ConvertInteger4 0x200DC
#undef OS_ConvertBinary1
#define OS_ConvertBinary1 0xDD
#undef XOS_ConvertBinary1
#define XOS_ConvertBinary1 0x200DD
#undef OS_ConvertBinary2
#define OS_ConvertBinary2 0xDE
#undef XOS_ConvertBinary2
#define XOS_ConvertBinary2 0x200DE
#undef OS_ConvertBinary3
#define OS_ConvertBinary3 0xDF
#undef XOS_ConvertBinary3
#define XOS_ConvertBinary3 0x200DF
#undef OS_ConvertBinary4
#define OS_ConvertBinary4 0xE0
#undef XOS_ConvertBinary4
#define XOS_ConvertBinary4 0x200E0
#undef OS_ConvertSpacedCardinal1
#define OS_ConvertSpacedCardinal1 0xE1
#undef XOS_ConvertSpacedCardinal1
#define XOS_ConvertSpacedCardinal1 0x200E1
#undef OS_ConvertSpacedCardinal2
#define OS_ConvertSpacedCardinal2 0xE2
#undef XOS_ConvertSpacedCardinal2
#define XOS_ConvertSpacedCardinal2 0x200E2
#undef OS_ConvertSpacedCardinal3
#define OS_ConvertSpacedCardinal3 0xE3
#undef XOS_ConvertSpacedCardinal3
#define XOS_ConvertSpacedCardinal3 0x200E3
#undef OS_ConvertSpacedCardinal4
#define OS_ConvertSpacedCardinal4 0xE4
#undef XOS_ConvertSpacedCardinal4
#define XOS_ConvertSpacedCardinal4 0x200E4
#undef OS_ConvertSpacedInteger1
#define OS_ConvertSpacedInteger1 0xE5
#undef XOS_ConvertSpacedInteger1
#define XOS_ConvertSpacedInteger1 0x200E5
#undef OS_ConvertSpacedInteger2
#define OS_ConvertSpacedInteger2 0xE6
#undef XOS_ConvertSpacedInteger2
#define XOS_ConvertSpacedInteger2 0x200E6
#undef OS_ConvertSpacedInteger3
#define OS_ConvertSpacedInteger3 0xE7
#undef XOS_ConvertSpacedInteger3
#define XOS_ConvertSpacedInteger3 0x200E7
#undef OS_ConvertSpacedInteger4
#define OS_ConvertSpacedInteger4 0xE8
#undef XOS_ConvertSpacedInteger4
#define XOS_ConvertSpacedInteger4 0x200E8
#undef OS_ConvertFixedNetStation
#define OS_ConvertFixedNetStation 0xE9
#undef XOS_ConvertFixedNetStation
#define XOS_ConvertFixedNetStation 0x200E9
#undef OS_ConvertNetStation
#define OS_ConvertNetStation 0xEA
#undef XOS_ConvertNetStation
#define XOS_ConvertNetStation 0x200EA
#undef OS_ConvertFixedFileSize
#define OS_ConvertFixedFileSize 0xEB
#undef XOS_ConvertFixedFileSize
#define XOS_ConvertFixedFileSize 0x200EB
#undef OS_ConvertFileSize
#define OS_ConvertFileSize 0xEC
#undef XOS_ConvertFileSize
#define XOS_ConvertFileSize 0x200EC
#undef OS_ConvertVariform
#define OS_ConvertVariform 0xED
#undef XOS_ConvertVariform
#define XOS_ConvertVariform 0x200ED
#undef OS_WriteI
#define OS_WriteI 0x100
#undef XOS_WriteI
#define XOS_WriteI 0x20100
#undef IIC_Control
#define IIC_Control 0x240
#undef XIIC_Control
#define XIIC_Control 0x20240
#undef Econet_CreateReceive
#define Econet_CreateReceive 0x40000
#undef XEconet_CreateReceive
#define XEconet_CreateReceive 0x60000
#undef Econet_ExamineReceive
#define Econet_ExamineReceive 0x40001
#undef XEconet_ExamineReceive
#define XEconet_ExamineReceive 0x60001
#undef Econet_ReadReceive
#define Econet_ReadReceive 0x40002
#undef XEconet_ReadReceive
#define XEconet_ReadReceive 0x60002
#undef Econet_AbandonReceive
#define Econet_AbandonReceive 0x40003
#undef XEconet_AbandonReceive
#define XEconet_AbandonReceive 0x60003
#undef Econet_WaitForReception
#define Econet_WaitForReception 0x40004
#undef XEconet_WaitForReception
#define XEconet_WaitForReception 0x60004
#undef Econet_EnumerateReceive
#define Econet_EnumerateReceive 0x40005
#undef XEconet_EnumerateReceive
#define XEconet_EnumerateReceive 0x60005
#undef Econet_StartTransmit
#define Econet_StartTransmit 0x40006
#undef XEconet_StartTransmit
#define XEconet_StartTransmit 0x60006
#undef Econet_PollTransmit
#define Econet_PollTransmit 0x40007
#undef XEconet_PollTransmit
#define XEconet_PollTransmit 0x60007
#undef Econet_AbandonTransmit
#define Econet_AbandonTransmit 0x40008
#undef XEconet_AbandonTransmit
#define XEconet_AbandonTransmit 0x60008
#undef Econet_DoTransmit
#define Econet_DoTransmit 0x40009
#undef XEconet_DoTransmit
#define XEconet_DoTransmit 0x60009
#undef Econet_ReadLocalStationAndNet
#define Econet_ReadLocalStationAndNet 0x4000A
#undef XEconet_ReadLocalStationAndNet
#define XEconet_ReadLocalStationAndNet 0x6000A
#undef Econet_ConvertStatusToString
#define Econet_ConvertStatusToString 0x4000B
#undef XEconet_ConvertStatusToString
#define XEconet_ConvertStatusToString 0x6000B
#undef Econet_ConvertStatusToError
#define Econet_ConvertStatusToError 0x4000C
#undef XEconet_ConvertStatusToError
#define XEconet_ConvertStatusToError 0x6000C
#undef Econet_ReadProtection
#define Econet_ReadProtection 0x4000D
#undef XEconet_ReadProtection
#define XEconet_ReadProtection 0x6000D
#undef Econet_SetProtection
#define Econet_SetProtection 0x4000E
#undef XEconet_SetProtection
#define XEconet_SetProtection 0x6000E
#undef Econet_ReadStationNumber
#define Econet_ReadStationNumber 0x4000F
#undef XEconet_ReadStationNumber
#define XEconet_ReadStationNumber 0x6000F
#undef Econet_PrintBanner
#define Econet_PrintBanner 0x40010
#undef XEconet_PrintBanner
#define XEconet_PrintBanner 0x60010
#undef Econet_ReadTransportType
#define Econet_ReadTransportType 0x40011
#undef XEconet_ReadTransportType
#define XEconet_ReadTransportType 0x60011
#undef Econet_ReleasePort
#define Econet_ReleasePort 0x40012
#undef XEconet_ReleasePort
#define XEconet_ReleasePort 0x60012
#undef Econet_AllocatePort
#define Econet_AllocatePort 0x40013
#undef XEconet_AllocatePort
#define XEconet_AllocatePort 0x60013
#undef Econet_DeAllocatePort
#define Econet_DeAllocatePort 0x40014
#undef XEconet_DeAllocatePort
#define XEconet_DeAllocatePort 0x60014
#undef Econet_ClaimPort
#define Econet_ClaimPort 0x40015
#undef XEconet_ClaimPort
#define XEconet_ClaimPort 0x60015
#undef Econet_StartImmediate
#define Econet_StartImmediate 0x40016
#undef XEconet_StartImmediate
#define XEconet_StartImmediate 0x60016
#undef Econet_DoImmediate
#define Econet_DoImmediate 0x40017
#undef XEconet_DoImmediate
#define XEconet_DoImmediate 0x60017
#undef Econet_AbandonAndReadReceive
#define Econet_AbandonAndReadReceive 0x40018
#undef XEconet_AbandonAndReadReceive
#define XEconet_AbandonAndReadReceive 0x60018
#undef Econet_Version
#define Econet_Version 0x40019
#undef XEconet_Version
#define XEconet_Version 0x60019
#undef Econet_NetworkState
#define Econet_NetworkState 0x4001A
#undef XEconet_NetworkState
#define XEconet_NetworkState 0x6001A
#undef Econet_PacketSize
#define Econet_PacketSize 0x4001B
#undef XEconet_PacketSize
#define XEconet_PacketSize 0x6001B
#undef Econet_ReadTransportName
#define Econet_ReadTransportName 0x4001C
#undef XEconet_ReadTransportName
#define XEconet_ReadTransportName 0x6001C
#undef Econet_InetRxDirect
#define Econet_InetRxDirect 0x4001D
#undef XEconet_InetRxDirect
#define XEconet_InetRxDirect 0x6001D
#undef Econet_EnumerateMap
#define Econet_EnumerateMap 0x4001E
#undef XEconet_EnumerateMap
#define XEconet_EnumerateMap 0x6001E
#undef Econet_EnumerateTransmit
#define Econet_EnumerateTransmit 0x4001F
#undef XEconet_EnumerateTransmit
#define XEconet_EnumerateTransmit 0x6001F
#undef Econet_HardwareAddresses
#define Econet_HardwareAddresses 0x40020
#undef XEconet_HardwareAddresses
#define XEconet_HardwareAddresses 0x60020
#undef Econet_NetworkParameters
#define Econet_NetworkParameters 0x40021
#undef XEconet_NetworkParameters
#define XEconet_NetworkParameters 0x60021
#undef NetFS_ReadFSNumber
#define NetFS_ReadFSNumber 0x40040
#undef XNetFS_ReadFSNumber
#define XNetFS_ReadFSNumber 0x60040
#undef NetFS_SetFSNumber
#define NetFS_SetFSNumber 0x40041
#undef XNetFS_SetFSNumber
#define XNetFS_SetFSNumber 0x60041
#undef NetFS_ReadFSName
#define NetFS_ReadFSName 0x40042
#undef XNetFS_ReadFSName
#define XNetFS_ReadFSName 0x60042
#undef NetFS_SetFSName
#define NetFS_SetFSName 0x40043
#undef XNetFS_SetFSName
#define XNetFS_SetFSName 0x60043
#undef NetFS_ReadCurrentContext
#define NetFS_ReadCurrentContext 0x40044
#undef XNetFS_ReadCurrentContext
#define XNetFS_ReadCurrentContext 0x60044
#undef NetFS_SetCurrentContext
#define NetFS_SetCurrentContext 0x40045
#undef XNetFS_SetCurrentContext
#define XNetFS_SetCurrentContext 0x60045
#undef NetFS_ReadFSTimeouts
#define NetFS_ReadFSTimeouts 0x40046
#undef XNetFS_ReadFSTimeouts
#define XNetFS_ReadFSTimeouts 0x60046
#undef NetFS_SetFSTimeouts
#define NetFS_SetFSTimeouts 0x40047
#undef XNetFS_SetFSTimeouts
#define XNetFS_SetFSTimeouts 0x60047
#undef NetFS_DoFSOp
#define NetFS_DoFSOp 0x40048
#undef XNetFS_DoFSOp
#define XNetFS_DoFSOp 0x60048
#undef NetFS_EnumerateFSList
#define NetFS_EnumerateFSList 0x40049
#undef XNetFS_EnumerateFSList
#define XNetFS_EnumerateFSList 0x60049
#undef NetFS_EnumerateFS
#define NetFS_EnumerateFS 0x4004A
#undef XNetFS_EnumerateFS
#define XNetFS_EnumerateFS 0x6004A
#undef NetFS_ConvertDate
#define NetFS_ConvertDate 0x4004B
#undef XNetFS_ConvertDate
#define XNetFS_ConvertDate 0x6004B
#undef NetFS_DoFSOpToGivenFS
#define NetFS_DoFSOpToGivenFS 0x4004C
#undef XNetFS_DoFSOpToGivenFS
#define XNetFS_DoFSOpToGivenFS 0x6004C
#undef NetFS_UpdateFSList
#define NetFS_UpdateFSList 0x4004D
#undef XNetFS_UpdateFSList
#define XNetFS_UpdateFSList 0x6004D
#undef NetFS_EnumerateFSContexts
#define NetFS_EnumerateFSContexts 0x4004E
#undef XNetFS_EnumerateFSContexts
#define XNetFS_EnumerateFSContexts 0x6004E
#undef NetFS_ReadUserId
#define NetFS_ReadUserId 0x4004F
#undef XNetFS_ReadUserId
#define XNetFS_ReadUserId 0x6004F
#undef NetFS_GetObjectUID
#define NetFS_GetObjectUID 0x40050
#undef XNetFS_GetObjectUID
#define XNetFS_GetObjectUID 0x60050
#undef NetFS_EnableCache
#define NetFS_EnableCache 0x40051
#undef XNetFS_EnableCache
#define XNetFS_EnableCache 0x60051
#undef Font_CacheAddr
#define Font_CacheAddr 0x40080
#undef XFont_CacheAddr
#define XFont_CacheAddr 0x60080
#undef Font_FindFont
#define Font_FindFont 0x40081
#undef XFont_FindFont
#define XFont_FindFont 0x60081
#undef Font_LoseFont
#define Font_LoseFont 0x40082
#undef XFont_LoseFont
#define XFont_LoseFont 0x60082
#undef Font_ReadDefn
#define Font_ReadDefn 0x40083
#undef XFont_ReadDefn
#define XFont_ReadDefn 0x60083
#undef Font_ReadInfo
#define Font_ReadInfo 0x40084
#undef XFont_ReadInfo
#define XFont_ReadInfo 0x60084
#undef Font_StringWidth
#define Font_StringWidth 0x40085
#undef XFont_StringWidth
#define XFont_StringWidth 0x60085
#undef Font_Paint
#define Font_Paint 0x40086
#undef XFont_Paint
#define XFont_Paint 0x60086
#undef Font_Caret
#define Font_Caret 0x40087
#undef XFont_Caret
#define XFont_Caret 0x60087
#undef Font_ConverttoOS
#define Font_ConverttoOS 0x40088
#undef XFont_ConverttoOS
#define XFont_ConverttoOS 0x60088
#undef Font_Converttopoints
#define Font_Converttopoints 0x40089
#undef XFont_Converttopoints
#define XFont_Converttopoints 0x60089
#undef Font_SetFont
#define Font_SetFont 0x4008A
#undef XFont_SetFont
#define XFont_SetFont 0x6008A
#undef Font_CurrentFont
#define Font_CurrentFont 0x4008B
#undef XFont_CurrentFont
#define XFont_CurrentFont 0x6008B
#undef Font_FutureFont
#define Font_FutureFont 0x4008C
#undef XFont_FutureFont
#define XFont_FutureFont 0x6008C
#undef Font_FindCaret
#define Font_FindCaret 0x4008D
#undef XFont_FindCaret
#define XFont_FindCaret 0x6008D
#undef Font_CharBBox
#define Font_CharBBox 0x4008E
#undef XFont_CharBBox
#define XFont_CharBBox 0x6008E
#undef Font_ReadScaleFactor
#define Font_ReadScaleFactor 0x4008F
#undef XFont_ReadScaleFactor
#define XFont_ReadScaleFactor 0x6008F
#undef Font_SetScaleFactor
#define Font_SetScaleFactor 0x40090
#undef XFont_SetScaleFactor
#define XFont_SetScaleFactor 0x60090
#undef Font_ListFonts
#define Font_ListFonts 0x40091
#undef XFont_ListFonts
#define XFont_ListFonts 0x60091
#undef Font_SetFontColours
#define Font_SetFontColours 0x40092
#undef XFont_SetFontColours
#define XFont_SetFontColours 0x60092
#undef Font_SetPalette
#define Font_SetPalette 0x40093
#undef XFont_SetPalette
#define XFont_SetPalette 0x60093
#undef Font_ReadThresholds
#define Font_ReadThresholds 0x40094
#undef XFont_ReadThresholds
#define XFont_ReadThresholds 0x60094
#undef Font_SetThresholds
#define Font_SetThresholds 0x40095
#undef XFont_SetThresholds
#define XFont_SetThresholds 0x60095
#undef Font_FindCaretJ
#define Font_FindCaretJ 0x40096
#undef XFont_FindCaretJ
#define XFont_FindCaretJ 0x60096
#undef Font_StringBBox
#define Font_StringBBox 0x40097
#undef XFont_StringBBox
#define XFont_StringBBox 0x60097
#undef Font_ReadColourTable
#define Font_ReadColourTable 0x40098
#undef XFont_ReadColourTable
#define XFont_ReadColourTable 0x60098
#undef Font_MakeBitmap
#define Font_MakeBitmap 0x40099
#undef XFont_MakeBitmap
#define XFont_MakeBitmap 0x60099
#undef Font_UnCacheFile
#define Font_UnCacheFile 0x4009A
#undef XFont_UnCacheFile
#define XFont_UnCacheFile 0x6009A
#undef Font_SetFontMax
#define Font_SetFontMax 0x4009B
#undef XFont_SetFontMax
#define XFont_SetFontMax 0x6009B
#undef Font_ReadFontMax
#define Font_ReadFontMax 0x4009C
#undef XFont_ReadFontMax
#define XFont_ReadFontMax 0x6009C
#undef Font_ReadFontPrefix
#define Font_ReadFontPrefix 0x4009D
#undef XFont_ReadFontPrefix
#define XFont_ReadFontPrefix 0x6009D
#undef Font_SwitchOutputToBuffer
#define Font_SwitchOutputToBuffer 0x4009E
#undef XFont_SwitchOutputToBuffer
#define XFont_SwitchOutputToBuffer 0x6009E
#undef Font_ReadFontMetrics
#define Font_ReadFontMetrics 0x4009F
#undef XFont_ReadFontMetrics
#define XFont_ReadFontMetrics 0x6009F
#undef Font_DecodeMenu
#define Font_DecodeMenu 0x400A0
#undef XFont_DecodeMenu
#define XFont_DecodeMenu 0x600A0
#undef Font_ScanString
#define Font_ScanString 0x400A1
#undef XFont_ScanString
#define XFont_ScanString 0x600A1
#undef Font_SetColourTable
#define Font_SetColourTable 0x400A2
#undef XFont_SetColourTable
#define XFont_SetColourTable 0x600A2
#undef Font_CurrentRGB
#define Font_CurrentRGB 0x400A3
#undef XFont_CurrentRGB
#define XFont_CurrentRGB 0x600A3
#undef Font_FutureRGB
#define Font_FutureRGB 0x400A4
#undef XFont_FutureRGB
#define XFont_FutureRGB 0x600A4
#undef Font_ReadEncodingFilename
#define Font_ReadEncodingFilename 0x400A5
#undef XFont_ReadEncodingFilename
#define XFont_ReadEncodingFilename 0x600A5
#undef Font_FindField
#define Font_FindField 0x400A6
#undef XFont_FindField
#define XFont_FindField 0x600A6
#undef Font_ApplyFields
#define Font_ApplyFields 0x400A7
#undef XFont_ApplyFields
#define XFont_ApplyFields 0x600A7
#undef Font_LookupFont
#define Font_LookupFont 0x400A8
#undef XFont_LookupFont
#define XFont_LookupFont 0x600A8
#undef Font_EnumerateCharacters
#define Font_EnumerateCharacters 0x400A9
#undef XFont_EnumerateCharacters
#define XFont_EnumerateCharacters 0x600A9
#undef Font_ChangeArea
#define Font_ChangeArea 0x400BF
#undef XFont_ChangeArea
#define XFont_ChangeArea 0x600BF
#undef Wimp_Initialise
#define Wimp_Initialise 0x400C0
#undef XWimp_Initialise
#define XWimp_Initialise 0x600C0
#undef Wimp_CreateWindow
#define Wimp_CreateWindow 0x400C1
#undef XWimp_CreateWindow
#define XWimp_CreateWindow 0x600C1
#undef Wimp_CreateIcon
#define Wimp_CreateIcon 0x400C2
#undef XWimp_CreateIcon
#define XWimp_CreateIcon 0x600C2
#undef Wimp_DeleteWindow
#define Wimp_DeleteWindow 0x400C3
#undef XWimp_DeleteWindow
#define XWimp_DeleteWindow 0x600C3
#undef Wimp_DeleteIcon
#define Wimp_DeleteIcon 0x400C4
#undef XWimp_DeleteIcon
#define XWimp_DeleteIcon 0x600C4
#undef Wimp_OpenWindow
#define Wimp_OpenWindow 0x400C5
#undef XWimp_OpenWindow
#define XWimp_OpenWindow 0x600C5
#undef Wimp_CloseWindow
#define Wimp_CloseWindow 0x400C6
#undef XWimp_CloseWindow
#define XWimp_CloseWindow 0x600C6
#undef Wimp_Poll
#define Wimp_Poll 0x400C7
#undef XWimp_Poll
#define XWimp_Poll 0x600C7
#undef Wimp_RedrawWindow
#define Wimp_RedrawWindow 0x400C8
#undef XWimp_RedrawWindow
#define XWimp_RedrawWindow 0x600C8
#undef Wimp_UpdateWindow
#define Wimp_UpdateWindow 0x400C9
#undef XWimp_UpdateWindow
#define XWimp_UpdateWindow 0x600C9
#undef Wimp_GetRectangle
#define Wimp_GetRectangle 0x400CA
#undef XWimp_GetRectangle
#define XWimp_GetRectangle 0x600CA
#undef Wimp_GetWindowState
#define Wimp_GetWindowState 0x400CB
#undef XWimp_GetWindowState
#define XWimp_GetWindowState 0x600CB
#undef Wimp_GetWindowInfo
#define Wimp_GetWindowInfo 0x400CC
#undef XWimp_GetWindowInfo
#define XWimp_GetWindowInfo 0x600CC
#undef Wimp_SetIconState
#define Wimp_SetIconState 0x400CD
#undef XWimp_SetIconState
#define XWimp_SetIconState 0x600CD
#undef Wimp_GetIconState
#define Wimp_GetIconState 0x400CE
#undef XWimp_GetIconState
#define XWimp_GetIconState 0x600CE
#undef Wimp_GetPointerInfo
#define Wimp_GetPointerInfo 0x400CF
#undef XWimp_GetPointerInfo
#define XWimp_GetPointerInfo 0x600CF
#undef Wimp_DragBox
#define Wimp_DragBox 0x400D0
#undef XWimp_DragBox
#define XWimp_DragBox 0x600D0
#undef Wimp_ForceRedraw
#define Wimp_ForceRedraw 0x400D1
#undef XWimp_ForceRedraw
#define XWimp_ForceRedraw 0x600D1
#undef Wimp_SetCaretPosition
#define Wimp_SetCaretPosition 0x400D2
#undef XWimp_SetCaretPosition
#define XWimp_SetCaretPosition 0x600D2
#undef Wimp_GetCaretPosition
#define Wimp_GetCaretPosition 0x400D3
#undef XWimp_GetCaretPosition
#define XWimp_GetCaretPosition 0x600D3
#undef Wimp_CreateMenu
#define Wimp_CreateMenu 0x400D4
#undef XWimp_CreateMenu
#define XWimp_CreateMenu 0x600D4
#undef Wimp_DecodeMenu
#define Wimp_DecodeMenu 0x400D5
#undef XWimp_DecodeMenu
#define XWimp_DecodeMenu 0x600D5
#undef Wimp_WhichIcon
#define Wimp_WhichIcon 0x400D6
#undef XWimp_WhichIcon
#define XWimp_WhichIcon 0x600D6
#undef Wimp_SetExtent
#define Wimp_SetExtent 0x400D7
#undef XWimp_SetExtent
#define XWimp_SetExtent 0x600D7
#undef Wimp_SetPointerShape
#define Wimp_SetPointerShape 0x400D8
#undef XWimp_SetPointerShape
#define XWimp_SetPointerShape 0x600D8
#undef Wimp_OpenTemplate
#define Wimp_OpenTemplate 0x400D9
#undef XWimp_OpenTemplate
#define XWimp_OpenTemplate 0x600D9
#undef Wimp_CloseTemplate
#define Wimp_CloseTemplate 0x400DA
#undef XWimp_CloseTemplate
#define XWimp_CloseTemplate 0x600DA
#undef Wimp_LoadTemplate
#define Wimp_LoadTemplate 0x400DB
#undef XWimp_LoadTemplate
#define XWimp_LoadTemplate 0x600DB
#undef Wimp_ProcessKey
#define Wimp_ProcessKey 0x400DC
#undef XWimp_ProcessKey
#define XWimp_ProcessKey 0x600DC
#undef Wimp_CloseDown
#define Wimp_CloseDown 0x400DD
#undef XWimp_CloseDown
#define XWimp_CloseDown 0x600DD
#undef Wimp_StartTask
#define Wimp_StartTask 0x400DE
#undef XWimp_StartTask
#define XWimp_StartTask 0x600DE
#undef Wimp_ReportError
#define Wimp_ReportError 0x400DF
#undef XWimp_ReportError
#define XWimp_ReportError 0x600DF
#undef Wimp_GetWindowOutline
#define Wimp_GetWindowOutline 0x400E0
#undef XWimp_GetWindowOutline
#define XWimp_GetWindowOutline 0x600E0
#undef Wimp_PollIdle
#define Wimp_PollIdle 0x400E1
#undef XWimp_PollIdle
#define XWimp_PollIdle 0x600E1
#undef Wimp_PlotIcon
#define Wimp_PlotIcon 0x400E2
#undef XWimp_PlotIcon
#define XWimp_PlotIcon 0x600E2
#undef Wimp_SetMode
#define Wimp_SetMode 0x400E3
#undef XWimp_SetMode
#define XWimp_SetMode 0x600E3
#undef Wimp_SetPalette
#define Wimp_SetPalette 0x400E4
#undef XWimp_SetPalette
#define XWimp_SetPalette 0x600E4
#undef Wimp_ReadPalette
#define Wimp_ReadPalette 0x400E5
#undef XWimp_ReadPalette
#define XWimp_ReadPalette 0x600E5
#undef Wimp_SetColour
#define Wimp_SetColour 0x400E6
#undef XWimp_SetColour
#define XWimp_SetColour 0x600E6
#undef Wimp_SendMessage
#define Wimp_SendMessage 0x400E7
#undef XWimp_SendMessage
#define XWimp_SendMessage 0x600E7
#undef Wimp_CreateSubMenu
#define Wimp_CreateSubMenu 0x400E8
#undef XWimp_CreateSubMenu
#define XWimp_CreateSubMenu 0x600E8
#undef Wimp_SpriteOp
#define Wimp_SpriteOp 0x400E9
#undef XWimp_SpriteOp
#define XWimp_SpriteOp 0x600E9
#undef Wimp_BaseOfSprites
#define Wimp_BaseOfSprites 0x400EA
#undef XWimp_BaseOfSprites
#define XWimp_BaseOfSprites 0x600EA
#undef Wimp_BlockCopy
#define Wimp_BlockCopy 0x400EB
#undef XWimp_BlockCopy
#define XWimp_BlockCopy 0x600EB
#undef Wimp_SlotSize
#define Wimp_SlotSize 0x400EC
#undef XWimp_SlotSize
#define XWimp_SlotSize 0x600EC
#undef Wimp_ReadPixTrans
#define Wimp_ReadPixTrans 0x400ED
#undef XWimp_ReadPixTrans
#define XWimp_ReadPixTrans 0x600ED
#undef Wimp_ClaimFreeMemory
#define Wimp_ClaimFreeMemory 0x400EE
#undef XWimp_ClaimFreeMemory
#define XWimp_ClaimFreeMemory 0x600EE
#undef Wimp_CommandWindow
#define Wimp_CommandWindow 0x400EF
#undef XWimp_CommandWindow
#define XWimp_CommandWindow 0x600EF
#undef Wimp_TextColour
#define Wimp_TextColour 0x400F0
#undef XWimp_TextColour
#define XWimp_TextColour 0x600F0
#undef Wimp_TransferBlock
#define Wimp_TransferBlock 0x400F1
#undef XWimp_TransferBlock
#define XWimp_TransferBlock 0x600F1
#undef Wimp_ReadSysInfo
#define Wimp_ReadSysInfo 0x400F2
#undef XWimp_ReadSysInfo
#define XWimp_ReadSysInfo 0x600F2
#undef Wimp_SetFontColours
#define Wimp_SetFontColours 0x400F3
#undef XWimp_SetFontColours
#define XWimp_SetFontColours 0x600F3
#undef Wimp_GetMenuState
#define Wimp_GetMenuState 0x400F4
#undef XWimp_GetMenuState
#define XWimp_GetMenuState 0x600F4
#undef Wimp_RegisterFilter
#define Wimp_RegisterFilter 0x400F5
#undef XWimp_RegisterFilter
#define XWimp_RegisterFilter 0x600F5
#undef Wimp_AddMessages
#define Wimp_AddMessages 0x400F6
#undef XWimp_AddMessages
#define XWimp_AddMessages 0x600F6
#undef Wimp_RemoveMessages
#define Wimp_RemoveMessages 0x400F7
#undef XWimp_RemoveMessages
#define XWimp_RemoveMessages 0x600F7
#undef Wimp_SetColourMapping
#define Wimp_SetColourMapping 0x400F8
#undef XWimp_SetColourMapping
#define XWimp_SetColourMapping 0x600F8
#undef Wimp_TextOp
#define Wimp_TextOp 0x400F9
#undef XWimp_TextOp
#define XWimp_TextOp 0x600F9
#undef Wimp_SetWatchdogState
#define Wimp_SetWatchdogState 0x400FA
#undef XWimp_SetWatchdogState
#define XWimp_SetWatchdogState 0x600FA
#undef Wimp_Extend
#define Wimp_Extend 0x400FB
#undef XWimp_Extend
#define XWimp_Extend 0x600FB
#undef Wimp_ResizeIcon
#define Wimp_ResizeIcon 0x400FC
#undef XWimp_ResizeIcon
#define XWimp_ResizeIcon 0x600FC
#undef Wimp_AutoScroll
#define Wimp_AutoScroll 0x400FD
#undef XWimp_AutoScroll
#define XWimp_AutoScroll 0x600FD
#undef HostFS_HostVdu
#define HostFS_HostVdu 0x40100
#undef XHostFS_HostVdu
#define XHostFS_HostVdu 0x60100
#undef HostFS_TubeVdu
#define HostFS_TubeVdu 0x40101
#undef XHostFS_TubeVdu
#define XHostFS_TubeVdu 0x60101
#undef HostFS_WriteC
#define HostFS_WriteC 0x40102
#undef XHostFS_WriteC
#define XHostFS_WriteC 0x60102
#undef Sound_Configure
#define Sound_Configure 0x40140
#undef XSound_Configure
#define XSound_Configure 0x60140
#undef Sound_Enable
#define Sound_Enable 0x40141
#undef XSound_Enable
#define XSound_Enable 0x60141
#undef Sound_Stereo
#define Sound_Stereo 0x40142
#undef XSound_Stereo
#define XSound_Stereo 0x60142
#undef Sound_Speaker
#define Sound_Speaker 0x40143
#undef XSound_Speaker
#define XSound_Speaker 0x60143
#undef Sound_Mode
#define Sound_Mode 0x40144
#undef XSound_Mode
#define XSound_Mode 0x60144
#undef Sound_LinearHandler
#define Sound_LinearHandler 0x40145
#undef XSound_LinearHandler
#define XSound_LinearHandler 0x60145
#undef Sound_SampleRate
#define Sound_SampleRate 0x40146
#undef XSound_SampleRate
#define XSound_SampleRate 0x60146
#undef Sound_ReadSysInfo
#define Sound_ReadSysInfo 0x40147
#undef XSound_ReadSysInfo
#define XSound_ReadSysInfo 0x60147
#undef Sound_SelectDefaultController
#define Sound_SelectDefaultController 0x40148
#undef XSound_SelectDefaultController
#define XSound_SelectDefaultController 0x60148
#undef Sound_EnumerateControllers
#define Sound_EnumerateControllers 0x40149
#undef XSound_EnumerateControllers
#define XSound_EnumerateControllers 0x60149
#undef Sound_ControllerInfo
#define Sound_ControllerInfo 0x4014A
#undef XSound_ControllerInfo
#define XSound_ControllerInfo 0x6014A
#undef Sound_Volume
#define Sound_Volume 0x40180
#undef XSound_Volume
#define XSound_Volume 0x60180
#undef Sound_SoundLog
#define Sound_SoundLog 0x40181
#undef XSound_SoundLog
#define XSound_SoundLog 0x60181
#undef Sound_LogScale
#define Sound_LogScale 0x40182
#undef XSound_LogScale
#define XSound_LogScale 0x60182
#undef Sound_InstallVoice
#define Sound_InstallVoice 0x40183
#undef XSound_InstallVoice
#define XSound_InstallVoice 0x60183
#undef Sound_RemoveVoice
#define Sound_RemoveVoice 0x40184
#undef XSound_RemoveVoice
#define XSound_RemoveVoice 0x60184
#undef Sound_AttachVoice
#define Sound_AttachVoice 0x40185
#undef XSound_AttachVoice
#define XSound_AttachVoice 0x60185
#undef Sound_ControlPacked
#define Sound_ControlPacked 0x40186
#undef XSound_ControlPacked
#define XSound_ControlPacked 0x60186
#undef Sound_Tuning
#define Sound_Tuning 0x40187
#undef XSound_Tuning
#define XSound_Tuning 0x60187
#undef Sound_Pitch
#define Sound_Pitch 0x40188
#undef XSound_Pitch
#define XSound_Pitch 0x60188
#undef Sound_Control
#define Sound_Control 0x40189
#undef XSound_Control
#define XSound_Control 0x60189
#undef Sound_AttachNamedVoice
#define Sound_AttachNamedVoice 0x4018A
#undef XSound_AttachNamedVoice
#define XSound_AttachNamedVoice 0x6018A
#undef Sound_ReadControlBlock
#define Sound_ReadControlBlock 0x4018B
#undef XSound_ReadControlBlock
#define XSound_ReadControlBlock 0x6018B
#undef Sound_WriteControlBlock
#define Sound_WriteControlBlock 0x4018C
#undef XSound_WriteControlBlock
#define XSound_WriteControlBlock 0x6018C
#undef Sound_QInit
#define Sound_QInit 0x401C0
#undef XSound_QInit
#define XSound_QInit 0x601C0
#undef Sound_QSchedule
#define Sound_QSchedule 0x401C1
#undef XSound_QSchedule
#define XSound_QSchedule 0x601C1
#undef Sound_QRemove
#define Sound_QRemove 0x401C2
#undef XSound_QRemove
#define XSound_QRemove 0x601C2
#undef Sound_QFree
#define Sound_QFree 0x401C3
#undef XSound_QFree
#define XSound_QFree 0x601C3
#undef Sound_QSDispatch
#define Sound_QSDispatch 0x401C4
#undef XSound_QSDispatch
#define XSound_QSDispatch 0x601C4
#undef Sound_QTempo
#define Sound_QTempo 0x401C5
#undef XSound_QTempo
#define XSound_QTempo 0x601C5
#undef Sound_QBeat
#define Sound_QBeat 0x401C6
#undef XSound_QBeat
#define XSound_QBeat 0x601C6
#undef Sound_QInterface
#define Sound_QInterface 0x401C7
#undef XSound_QInterface
#define XSound_QInterface 0x601C7
#undef NetPrint_ReadPSNumber
#define NetPrint_ReadPSNumber 0x40200
#undef XNetPrint_ReadPSNumber
#define XNetPrint_ReadPSNumber 0x60200
#undef NetPrint_SetPSNumber
#define NetPrint_SetPSNumber 0x40201
#undef XNetPrint_SetPSNumber
#define XNetPrint_SetPSNumber 0x60201
#undef NetPrint_ReadPSName
#define NetPrint_ReadPSName 0x40202
#undef XNetPrint_ReadPSName
#define XNetPrint_ReadPSName 0x60202
#undef NetPrint_SetPSName
#define NetPrint_SetPSName 0x40203
#undef XNetPrint_SetPSName
#define XNetPrint_SetPSName 0x60203
#undef NetPrint_ReadPSTimeouts
#define NetPrint_ReadPSTimeouts 0x40204
#undef XNetPrint_ReadPSTimeouts
#define XNetPrint_ReadPSTimeouts 0x60204
#undef NetPrint_SetPSTimeouts
#define NetPrint_SetPSTimeouts 0x40205
#undef XNetPrint_SetPSTimeouts
#define XNetPrint_SetPSTimeouts 0x60205
#undef NetPrint_BindPSName
#define NetPrint_BindPSName 0x40206
#undef XNetPrint_BindPSName
#define XNetPrint_BindPSName 0x60206
#undef NetPrint_ListServers
#define NetPrint_ListServers 0x40207
#undef XNetPrint_ListServers
#define XNetPrint_ListServers 0x60207
#undef NetPrint_ConvertStatusToString
#define NetPrint_ConvertStatusToString 0x40208
#undef XNetPrint_ConvertStatusToString
#define XNetPrint_ConvertStatusToString 0x60208
#undef ADFS_DiscOp
#define ADFS_DiscOp 0x40240
#undef XADFS_DiscOp
#define XADFS_DiscOp 0x60240
#undef ADFS_HDC
#define ADFS_HDC 0x40241
#undef XADFS_HDC
#define XADFS_HDC 0x60241
#undef ADFS_Drives
#define ADFS_Drives 0x40242
#undef XADFS_Drives
#define XADFS_Drives 0x60242
#undef ADFS_FreeSpace
#define ADFS_FreeSpace 0x40243
#undef XADFS_FreeSpace
#define XADFS_FreeSpace 0x60243
#undef ADFS_Retries
#define ADFS_Retries 0x40244
#undef XADFS_Retries
#define XADFS_Retries 0x60244
#undef ADFS_DescribeDisc
#define ADFS_DescribeDisc 0x40245
#undef XADFS_DescribeDisc
#define XADFS_DescribeDisc 0x60245
#undef ADFS_VetFormat
#define ADFS_VetFormat 0x40246
#undef XADFS_VetFormat
#define XADFS_VetFormat 0x60246
#undef ADFS_FlpProcessDCB
#define ADFS_FlpProcessDCB 0x40247
#undef XADFS_FlpProcessDCB
#define XADFS_FlpProcessDCB 0x60247
#undef ADFS_ControllerType
#define ADFS_ControllerType 0x40248
#undef XADFS_ControllerType
#define XADFS_ControllerType 0x60248
#undef ADFS_PowerControl
#define ADFS_PowerControl 0x40249
#undef XADFS_PowerControl
#define XADFS_PowerControl 0x60249
#undef ADFS_SetIDEController
#define ADFS_SetIDEController 0x4024A
#undef XADFS_SetIDEController
#define XADFS_SetIDEController 0x6024A
#undef ADFS_IDEUserOp
#define ADFS_IDEUserOp 0x4024B
#undef XADFS_IDEUserOp
#define XADFS_IDEUserOp 0x6024B
#undef ADFS_MiscOp
#define ADFS_MiscOp 0x4024C
#undef XADFS_MiscOp
#define XADFS_MiscOp 0x6024C
#undef ADFS_SectorDiscOp
#define ADFS_SectorDiscOp 0x4024D
#undef XADFS_SectorDiscOp
#define XADFS_SectorDiscOp 0x6024D
#undef ADFS_NOP2
#define ADFS_NOP2 0x4024E
#undef XADFS_NOP2
#define XADFS_NOP2 0x6024E
#undef ADFS_NOP3
#define ADFS_NOP3 0x4024F
#undef XADFS_NOP3
#define XADFS_NOP3 0x6024F
#undef ADFS_ECCSAndRetries
#define ADFS_ECCSAndRetries 0x40250
#undef XADFS_ECCSAndRetries
#define XADFS_ECCSAndRetries 0x60250
#undef ADFS_LockIDE
#define ADFS_LockIDE 0x40251
#undef XADFS_LockIDE
#define XADFS_LockIDE 0x60251
#undef ADFS_FreeSpace64
#define ADFS_FreeSpace64 0x40252
#undef XADFS_FreeSpace64
#define XADFS_FreeSpace64 0x60252
#undef ADFS_IDEDeviceInfo
#define ADFS_IDEDeviceInfo 0x40253
#undef XADFS_IDEDeviceInfo
#define XADFS_IDEDeviceInfo 0x60253
#undef ADFS_DiscOp64
#define ADFS_DiscOp64 0x40254
#undef XADFS_DiscOp64
#define XADFS_DiscOp64 0x60254
#undef ADFS_ATAPIOp
#define ADFS_ATAPIOp 0x40255
#undef XADFS_ATAPIOp
#define XADFS_ATAPIOp 0x60255
#undef Podule_ReadID
#define Podule_ReadID 0x40280
#undef XPodule_ReadID
#define XPodule_ReadID 0x60280
#undef Podule_ReadHeader
#define Podule_ReadHeader 0x40281
#undef XPodule_ReadHeader
#define XPodule_ReadHeader 0x60281
#undef Podule_EnumerateChunks
#define Podule_EnumerateChunks 0x40282
#undef XPodule_EnumerateChunks
#define XPodule_EnumerateChunks 0x60282
#undef Podule_ReadChunk
#define Podule_ReadChunk 0x40283
#undef XPodule_ReadChunk
#define XPodule_ReadChunk 0x60283
#undef Podule_ReadBytes
#define Podule_ReadBytes 0x40284
#undef XPodule_ReadBytes
#define XPodule_ReadBytes 0x60284
#undef Podule_WriteBytes
#define Podule_WriteBytes 0x40285
#undef XPodule_WriteBytes
#define XPodule_WriteBytes 0x60285
#undef Podule_CallLoader
#define Podule_CallLoader 0x40286
#undef XPodule_CallLoader
#define XPodule_CallLoader 0x60286
#undef Podule_RawRead
#define Podule_RawRead 0x40287
#undef XPodule_RawRead
#define XPodule_RawRead 0x60287
#undef Podule_RawWrite
#define Podule_RawWrite 0x40288
#undef XPodule_RawWrite
#define XPodule_RawWrite 0x60288
#undef Podule_HardwareAddress
#define Podule_HardwareAddress 0x40289
#undef XPodule_HardwareAddress
#define XPodule_HardwareAddress 0x60289
#undef Podule_EnumerateChunksWithInfo
#define Podule_EnumerateChunksWithInfo 0x4028A
#undef XPodule_EnumerateChunksWithInfo
#define XPodule_EnumerateChunksWithInfo 0x6028A
#undef Podule_HardwareAddresses
#define Podule_HardwareAddresses 0x4028B
#undef XPodule_HardwareAddresses
#define XPodule_HardwareAddresses 0x6028B
#undef Podule_ReturnNumber
#define Podule_ReturnNumber 0x4028C
#undef XPodule_ReturnNumber
#define XPodule_ReturnNumber 0x6028C
#undef Podule_ReadInfo
#define Podule_ReadInfo 0x4028D
#undef XPodule_ReadInfo
#define XPodule_ReadInfo 0x6028D
#undef Podule_SetSpeed
#define Podule_SetSpeed 0x4028E
#undef XPodule_SetSpeed
#define XPodule_SetSpeed 0x6028E
#undef Debugger_Disassemble
#define Debugger_Disassemble 0x40380
#undef XDebugger_Disassemble
#define XDebugger_Disassemble 0x60380
#undef Debugger_DisassembleThumb
#define Debugger_DisassembleThumb 0x40381
#undef XDebugger_DisassembleThumb
#define XDebugger_DisassembleThumb 0x60381
#undef SCSI_Version
#define SCSI_Version 0x403C0
#undef XSCSI_Version
#define XSCSI_Version 0x603C0
#undef SCSI_Initialise
#define SCSI_Initialise 0x403C1
#undef XSCSI_Initialise
#define XSCSI_Initialise 0x603C1
#undef SCSI_Control
#define SCSI_Control 0x403C2
#undef XSCSI_Control
#define XSCSI_Control 0x603C2
#undef SCSI_Op
#define SCSI_Op 0x403C3
#undef XSCSI_Op
#define XSCSI_Op 0x603C3
#undef SCSI_Status
#define SCSI_Status 0x403C4
#undef XSCSI_Status
#define XSCSI_Status 0x603C4
#undef SCSI_ReadControlLines
#define SCSI_ReadControlLines 0x403C5
#undef XSCSI_ReadControlLines
#define XSCSI_ReadControlLines 0x603C5
#undef SCSI_EEProm
#define SCSI_EEProm 0x403C6
#undef XSCSI_EEProm
#define XSCSI_EEProm 0x603C6
#undef SCSI_Reserve
#define SCSI_Reserve 0x403C7
#undef XSCSI_Reserve
#define XSCSI_Reserve 0x603C7
#undef SCSI_List
#define SCSI_List 0x403C8
#undef XSCSI_List
#define XSCSI_List 0x603C8
#undef SCSI_TargetControl
#define SCSI_TargetControl 0x403C9
#undef XSCSI_TargetControl
#define XSCSI_TargetControl 0x603C9
#undef SCSI_Deregister
#define SCSI_Deregister 0x403FE
#undef XSCSI_Deregister
#define XSCSI_Deregister 0x603FE
#undef SCSI_Register
#define SCSI_Register 0x403FF
#undef XSCSI_Register
#define XSCSI_Register 0x603FF
#undef FileCore_DiscOp
#define FileCore_DiscOp 0x40540
#undef XFileCore_DiscOp
#define XFileCore_DiscOp 0x60540
#undef FileCore_Create
#define FileCore_Create 0x40541
#undef XFileCore_Create
#define XFileCore_Create 0x60541
#undef FileCore_Drives
#define FileCore_Drives 0x40542
#undef XFileCore_Drives
#define XFileCore_Drives 0x60542
#undef FileCore_FreeSpace
#define FileCore_FreeSpace 0x40543
#undef XFileCore_FreeSpace
#define XFileCore_FreeSpace 0x60543
#undef FileCore_FloppyStructure
#define FileCore_FloppyStructure 0x40544
#undef XFileCore_FloppyStructure
#define XFileCore_FloppyStructure 0x60544
#undef FileCore_DescribeDisc
#define FileCore_DescribeDisc 0x40545
#undef XFileCore_DescribeDisc
#define XFileCore_DescribeDisc 0x60545
#undef FileCore_DiscardReadSectorsCache
#define FileCore_DiscardReadSectorsCache 0x40546
#undef XFileCore_DiscardReadSectorsCache
#define XFileCore_DiscardReadSectorsCache 0x60546
#undef FileCore_DiscFormat
#define FileCore_DiscFormat 0x40547
#undef XFileCore_DiscFormat
#define XFileCore_DiscFormat 0x60547
#undef FileCore_LayoutStructure
#define FileCore_LayoutStructure 0x40548
#undef XFileCore_LayoutStructure
#define XFileCore_LayoutStructure 0x60548
#undef FileCore_MiscOp
#define FileCore_MiscOp 0x40549
#undef XFileCore_MiscOp
#define XFileCore_MiscOp 0x60549
#undef FileCore_SectorDiscOp
#define FileCore_SectorDiscOp 0x4054A
#undef XFileCore_SectorDiscOp
#define XFileCore_SectorDiscOp 0x6054A
#undef FileCore_FreeSpace64
#define FileCore_FreeSpace64 0x4054B
#undef XFileCore_FreeSpace64
#define XFileCore_FreeSpace64 0x6054B
#undef FileCore_DiscOp64
#define FileCore_DiscOp64 0x4054C
#undef XFileCore_DiscOp64
#define XFileCore_DiscOp64 0x6054C
#undef FileCore_Features
#define FileCore_Features 0x4054D
#undef XFileCore_Features
#define XFileCore_Features 0x6054D
#undef Shell_Create
#define Shell_Create 0x405C0
#undef XShell_Create
#define XShell_Create 0x605C0
#undef Shell_Destroy
#define Shell_Destroy 0x405C1
#undef XShell_Destroy
#define XShell_Destroy 0x605C1
#undef Hourglass_On
#define Hourglass_On 0x406C0
#undef XHourglass_On
#define XHourglass_On 0x606C0
#undef Hourglass_Off
#define Hourglass_Off 0x406C1
#undef XHourglass_Off
#define XHourglass_Off 0x606C1
#undef Hourglass_Smash
#define Hourglass_Smash 0x406C2
#undef XHourglass_Smash
#define XHourglass_Smash 0x606C2
#undef Hourglass_Start
#define Hourglass_Start 0x406C3
#undef XHourglass_Start
#define XHourglass_Start 0x606C3
#undef Hourglass_Percentage
#define Hourglass_Percentage 0x406C4
#undef XHourglass_Percentage
#define XHourglass_Percentage 0x606C4
#undef Hourglass_LEDs
#define Hourglass_LEDs 0x406C5
#undef XHourglass_LEDs
#define XHourglass_LEDs 0x606C5
#undef Hourglass_Colours
#define Hourglass_Colours 0x406C6
#undef XHourglass_Colours
#define XHourglass_Colours 0x606C6
#undef Draw_ProcessPath
#define Draw_ProcessPath 0x40700
#undef XDraw_ProcessPath
#define XDraw_ProcessPath 0x60700
#undef Draw_ProcessPathFP
#define Draw_ProcessPathFP 0x40701
#undef XDraw_ProcessPathFP
#define XDraw_ProcessPathFP 0x60701
#undef Draw_Fill
#define Draw_Fill 0x40702
#undef XDraw_Fill
#define XDraw_Fill 0x60702
#undef Draw_FillFP
#define Draw_FillFP 0x40703
#undef XDraw_FillFP
#define XDraw_FillFP 0x60703
#undef Draw_Stroke
#define Draw_Stroke 0x40704
#undef XDraw_Stroke
#define XDraw_Stroke 0x60704
#undef Draw_StrokeFP
#define Draw_StrokeFP 0x40705
#undef XDraw_StrokeFP
#define XDraw_StrokeFP 0x60705
#undef Draw_StrokePath
#define Draw_StrokePath 0x40706
#undef XDraw_StrokePath
#define XDraw_StrokePath 0x60706
#undef Draw_StrokePathFP
#define Draw_StrokePathFP 0x40707
#undef XDraw_StrokePathFP
#define XDraw_StrokePathFP 0x60707
#undef Draw_FlattenPath
#define Draw_FlattenPath 0x40708
#undef XDraw_FlattenPath
#define XDraw_FlattenPath 0x60708
#undef Draw_FlattenPathFP
#define Draw_FlattenPathFP 0x40709
#undef XDraw_FlattenPathFP
#define XDraw_FlattenPathFP 0x60709
#undef Draw_TransformPath
#define Draw_TransformPath 0x4070A
#undef XDraw_TransformPath
#define XDraw_TransformPath 0x6070A
#undef Draw_TransformPathFP
#define Draw_TransformPathFP 0x4070B
#undef XDraw_TransformPathFP
#define XDraw_TransformPathFP 0x6070B
#undef Draw_FillClipped
#define Draw_FillClipped 0x4070C
#undef XDraw_FillClipped
#define XDraw_FillClipped 0x6070C
#undef Draw_FillClippedFP
#define Draw_FillClippedFP 0x4070D
#undef XDraw_FillClippedFP
#define XDraw_FillClippedFP 0x6070D
#undef Draw_StrokeClipped
#define Draw_StrokeClipped 0x4070E
#undef XDraw_StrokeClipped
#define XDraw_StrokeClipped 0x6070E
#undef Draw_StrokeClippedFP
#define Draw_StrokeClippedFP 0x4070F
#undef XDraw_StrokeClippedFP
#define XDraw_StrokeClippedFP 0x6070F
#undef ColourTrans_SelectTable
#define ColourTrans_SelectTable 0x40740
#undef XColourTrans_SelectTable
#define XColourTrans_SelectTable 0x60740
#undef ColourTrans_SelectGCOLTable
#define ColourTrans_SelectGCOLTable 0x40741
#undef XColourTrans_SelectGCOLTable
#define XColourTrans_SelectGCOLTable 0x60741
#undef ColourTrans_ReturnGCOL
#define ColourTrans_ReturnGCOL 0x40742
#undef XColourTrans_ReturnGCOL
#define XColourTrans_ReturnGCOL 0x60742
#undef ColourTrans_SetGCOL
#define ColourTrans_SetGCOL 0x40743
#undef XColourTrans_SetGCOL
#define XColourTrans_SetGCOL 0x60743
#undef ColourTrans_ReturnColourNumber
#define ColourTrans_ReturnColourNumber 0x40744
#undef XColourTrans_ReturnColourNumber
#define XColourTrans_ReturnColourNumber 0x60744
#undef ColourTrans_ReturnGCOLForMode
#define ColourTrans_ReturnGCOLForMode 0x40745
#undef XColourTrans_ReturnGCOLForMode
#define XColourTrans_ReturnGCOLForMode 0x60745
#undef ColourTrans_ReturnColourNumberForMode
#define ColourTrans_ReturnColourNumberForMode 0x40746
#undef XColourTrans_ReturnColourNumberForMode
#define XColourTrans_ReturnColourNumberForMode 0x60746
#undef ColourTrans_ReturnOppGCOL
#define ColourTrans_ReturnOppGCOL 0x40747
#undef XColourTrans_ReturnOppGCOL
#define XColourTrans_ReturnOppGCOL 0x60747
#undef ColourTrans_SetOppGCOL
#define ColourTrans_SetOppGCOL 0x40748
#undef XColourTrans_SetOppGCOL
#define XColourTrans_SetOppGCOL 0x60748
#undef ColourTrans_ReturnOppColourNumber
#define ColourTrans_ReturnOppColourNumber 0x40749
#undef XColourTrans_ReturnOppColourNumber
#define XColourTrans_ReturnOppColourNumber 0x60749
#undef ColourTrans_ReturnOppGCOLForMode
#define ColourTrans_ReturnOppGCOLForMode 0x4074A
#undef XColourTrans_ReturnOppGCOLForMode
#define XColourTrans_ReturnOppGCOLForMode 0x6074A
#undef ColourTrans_ReturnOppColourNumberForMode
#define ColourTrans_ReturnOppColourNumberForMode 0x4074B
#undef XColourTrans_ReturnOppColourNumberForMode
#define XColourTrans_ReturnOppColourNumberForMode 0x6074B
#undef ColourTrans_GCOLToColourNumber
#define ColourTrans_GCOLToColourNumber 0x4074C
#undef XColourTrans_GCOLToColourNumber
#define XColourTrans_GCOLToColourNumber 0x6074C
#undef ColourTrans_ColourNumberToGCOL
#define ColourTrans_ColourNumberToGCOL 0x4074D
#undef XColourTrans_ColourNumberToGCOL
#define XColourTrans_ColourNumberToGCOL 0x6074D
#undef ColourTrans_ReturnFontColours
#define ColourTrans_ReturnFontColours 0x4074E
#undef XColourTrans_ReturnFontColours
#define XColourTrans_ReturnFontColours 0x6074E
#undef ColourTrans_SetFontColours
#define ColourTrans_SetFontColours 0x4074F
#undef XColourTrans_SetFontColours
#define XColourTrans_SetFontColours 0x6074F
#undef ColourTrans_InvalidateCache
#define ColourTrans_InvalidateCache 0x40750
#undef XColourTrans_InvalidateCache
#define XColourTrans_InvalidateCache 0x60750
#undef ColourTrans_SetCalibration
#define ColourTrans_SetCalibration 0x40751
#undef XColourTrans_SetCalibration
#define XColourTrans_SetCalibration 0x60751
#undef ColourTrans_ReadCalibration
#define ColourTrans_ReadCalibration 0x40752
#undef XColourTrans_ReadCalibration
#define XColourTrans_ReadCalibration 0x60752
#undef ColourTrans_ConvertDeviceColour
#define ColourTrans_ConvertDeviceColour 0x40753
#undef XColourTrans_ConvertDeviceColour
#define XColourTrans_ConvertDeviceColour 0x60753
#undef ColourTrans_ConvertDevicePalette
#define ColourTrans_ConvertDevicePalette 0x40754
#undef XColourTrans_ConvertDevicePalette
#define XColourTrans_ConvertDevicePalette 0x60754
#undef ColourTrans_ConvertRGBToCIE
#define ColourTrans_ConvertRGBToCIE 0x40755
#undef XColourTrans_ConvertRGBToCIE
#define XColourTrans_ConvertRGBToCIE 0x60755
#undef ColourTrans_ConvertCIEToRGB
#define ColourTrans_ConvertCIEToRGB 0x40756
#undef XColourTrans_ConvertCIEToRGB
#define XColourTrans_ConvertCIEToRGB 0x60756
#undef ColourTrans_WriteCalibrationToFile
#define ColourTrans_WriteCalibrationToFile 0x40757
#undef XColourTrans_WriteCalibrationToFile
#define XColourTrans_WriteCalibrationToFile 0x60757
#undef ColourTrans_ConvertRGBToHSV
#define ColourTrans_ConvertRGBToHSV 0x40758
#undef XColourTrans_ConvertRGBToHSV
#define XColourTrans_ConvertRGBToHSV 0x60758
#undef ColourTrans_ConvertHSVToRGB
#define ColourTrans_ConvertHSVToRGB 0x40759
#undef XColourTrans_ConvertHSVToRGB
#define XColourTrans_ConvertHSVToRGB 0x60759
#undef ColourTrans_ConvertRGBToCMYK
#define ColourTrans_ConvertRGBToCMYK 0x4075A
#undef XColourTrans_ConvertRGBToCMYK
#define XColourTrans_ConvertRGBToCMYK 0x6075A
#undef ColourTrans_ConvertCMYKToRGB
#define ColourTrans_ConvertCMYKToRGB 0x4075B
#undef XColourTrans_ConvertCMYKToRGB
#define XColourTrans_ConvertCMYKToRGB 0x6075B
#undef ColourTrans_ReadPalette
#define ColourTrans_ReadPalette 0x4075C
#undef XColourTrans_ReadPalette
#define XColourTrans_ReadPalette 0x6075C
#undef ColourTrans_WritePalette
#define ColourTrans_WritePalette 0x4075D
#undef XColourTrans_WritePalette
#define XColourTrans_WritePalette 0x6075D
#undef ColourTrans_SetColour
#define ColourTrans_SetColour 0x4075E
#undef XColourTrans_SetColour
#define XColourTrans_SetColour 0x6075E
#undef ColourTrans_MiscOp
#define ColourTrans_MiscOp 0x4075F
#undef XColourTrans_MiscOp
#define XColourTrans_MiscOp 0x6075F
#undef ColourTrans_WriteLoadingsToFile
#define ColourTrans_WriteLoadingsToFile 0x40760
#undef XColourTrans_WriteLoadingsToFile
#define XColourTrans_WriteLoadingsToFile 0x60760
#undef ColourTrans_SetTextColour
#define ColourTrans_SetTextColour 0x40761
#undef XColourTrans_SetTextColour
#define XColourTrans_SetTextColour 0x60761
#undef ColourTrans_SetOppTextColour
#define ColourTrans_SetOppTextColour 0x40762
#undef XColourTrans_SetOppTextColour
#define XColourTrans_SetOppTextColour 0x60762
#undef ColourTrans_GenerateTable
#define ColourTrans_GenerateTable 0x40763
#undef XColourTrans_GenerateTable
#define XColourTrans_GenerateTable 0x60763
#undef RamFS_DiscOp
#define RamFS_DiscOp 0x40780
#undef XRamFS_DiscOp
#define XRamFS_DiscOp 0x60780
#undef RamFS_NOP1
#define RamFS_NOP1 0x40781
#undef XRamFS_NOP1
#define XRamFS_NOP1 0x60781
#undef RamFS_Drives
#define RamFS_Drives 0x40782
#undef XRamFS_Drives
#define XRamFS_Drives 0x60782
#undef RamFS_FreeSpace
#define RamFS_FreeSpace 0x40783
#undef XRamFS_FreeSpace
#define XRamFS_FreeSpace 0x60783
#undef RamFS_NOP2
#define RamFS_NOP2 0x40784
#undef XRamFS_NOP2
#define XRamFS_NOP2 0x60784
#undef RamFS_DescribeDisc
#define RamFS_DescribeDisc 0x40785
#undef XRamFS_DescribeDisc
#define XRamFS_DescribeDisc 0x60785
#undef RamFS_DiscOp64
#define RamFS_DiscOp64 0x40786
#undef XRamFS_DiscOp64
#define XRamFS_DiscOp64 0x60786
#undef RamFS_NOP3
#define RamFS_NOP3 0x40787
#undef XRamFS_NOP3
#define XRamFS_NOP3 0x60787
#undef RamFS_NOP4
#define RamFS_NOP4 0x40788
#undef XRamFS_NOP4
#define XRamFS_NOP4 0x60788
#undef RamFS_NOP5
#define RamFS_NOP5 0x40789
#undef XRamFS_NOP5
#define XRamFS_NOP5 0x60789
#undef RamFS_SectorDiscOp
#define RamFS_SectorDiscOp 0x4078A
#undef XRamFS_SectorDiscOp
#define XRamFS_SectorDiscOp 0x6078A
#undef SCSIFS_DiscOp
#define SCSIFS_DiscOp 0x40980
#undef XSCSIFS_DiscOp
#define XSCSIFS_DiscOp 0x60980
#undef SCSIFS_NOP1
#define SCSIFS_NOP1 0x40981
#undef XSCSIFS_NOP1
#define XSCSIFS_NOP1 0x60981
#undef SCSIFS_Drives
#define SCSIFS_Drives 0x40982
#undef XSCSIFS_Drives
#define XSCSIFS_Drives 0x60982
#undef SCSIFS_FreeSpace
#define SCSIFS_FreeSpace 0x40983
#undef XSCSIFS_FreeSpace
#define XSCSIFS_FreeSpace 0x60983
#undef SCSIFS_NOP2
#define SCSIFS_NOP2 0x40984
#undef XSCSIFS_NOP2
#define XSCSIFS_NOP2 0x60984
#undef SCSIFS_DescribeDisc
#define SCSIFS_DescribeDisc 0x40985
#undef XSCSIFS_DescribeDisc
#define XSCSIFS_DescribeDisc 0x60985
#undef SCSIFS_TestReady
#define SCSIFS_TestReady 0x40986
#undef XSCSIFS_TestReady
#define XSCSIFS_TestReady 0x60986
#undef SCSIFS_NOP3
#define SCSIFS_NOP3 0x40987
#undef XSCSIFS_NOP3
#define XSCSIFS_NOP3 0x60987
#undef SCSIFS_NOP4
#define SCSIFS_NOP4 0x40988
#undef XSCSIFS_NOP4
#define XSCSIFS_NOP4 0x60988
#undef SCSIFS_NOP5
#define SCSIFS_NOP5 0x40989
#undef XSCSIFS_NOP5
#define XSCSIFS_NOP5 0x60989
#undef SCSIFS_NOP6
#define SCSIFS_NOP6 0x4098A
#undef XSCSIFS_NOP6
#define XSCSIFS_NOP6 0x6098A
#undef SCSIFS_NOP7
#define SCSIFS_NOP7 0x4098B
#undef XSCSIFS_NOP7
#define XSCSIFS_NOP7 0x6098B
#undef SCSIFS_MiscOp
#define SCSIFS_MiscOp 0x4098C
#undef XSCSIFS_MiscOp
#define XSCSIFS_MiscOp 0x6098C
#undef SCSIFS_SectorDiscOp
#define SCSIFS_SectorDiscOp 0x4098D
#undef XSCSIFS_SectorDiscOp
#define XSCSIFS_SectorDiscOp 0x6098D
#undef SCSIFS_NOP8
#define SCSIFS_NOP8 0x4098E
#undef XSCSIFS_NOP8
#define XSCSIFS_NOP8 0x6098E
#undef SCSIFS_NOP9
#define SCSIFS_NOP9 0x4098F
#undef XSCSIFS_NOP9
#define XSCSIFS_NOP9 0x6098F
#undef SCSIFS_NOP10
#define SCSIFS_NOP10 0x40990
#undef XSCSIFS_NOP10
#define XSCSIFS_NOP10 0x60990
#undef SCSIFS_NOP11
#define SCSIFS_NOP11 0x40991
#undef XSCSIFS_NOP11
#define XSCSIFS_NOP11 0x60991
#undef SCSIFS_FreeSpace64
#define SCSIFS_FreeSpace64 0x40992
#undef XSCSIFS_FreeSpace64
#define XSCSIFS_FreeSpace64 0x60992
#undef SCSIFS_NOP12
#define SCSIFS_NOP12 0x40993
#undef XSCSIFS_NOP12
#define XSCSIFS_NOP12 0x60993
#undef SCSIFS_DiscOp64
#define SCSIFS_DiscOp64 0x40994
#undef XSCSIFS_DiscOp64
#define XSCSIFS_DiscOp64 0x60994
#undef SCSIFS_Partitions
#define SCSIFS_Partitions 0x40995
#undef XSCSIFS_Partitions
#define XSCSIFS_Partitions 0x60995
#undef Super_Sample90
#define Super_Sample90 0x40D80
#undef XSuper_Sample90
#define XSuper_Sample90 0x60D80
#undef Super_Sample45
#define Super_Sample45 0x40D81
#undef XSuper_Sample45
#define XSuper_Sample45 0x60D81
#undef FilerAction_SendSelectedDirectory
#define FilerAction_SendSelectedDirectory 0x40F80
#undef XFilerAction_SendSelectedDirectory
#define XFilerAction_SendSelectedDirectory 0x60F80
#undef FilerAction_SendSelectedFile
#define FilerAction_SendSelectedFile 0x40F81
#undef XFilerAction_SendSelectedFile
#define XFilerAction_SendSelectedFile 0x60F81
#undef FilerAction_SendStartOperation
#define FilerAction_SendStartOperation 0x40F82
#undef XFilerAction_SendStartOperation
#define XFilerAction_SendStartOperation 0x60F82
#undef SCSI_LogVersion
#define SCSI_LogVersion 0x41080
#undef XSCSI_LogVersion
#define XSCSI_LogVersion 0x61080
#undef SCSI_LogList
#define SCSI_LogList 0x41081
#undef XSCSI_LogList
#define XSCSI_LogList 0x61081
#undef CD_Version
#define CD_Version 0x41240
#undef XCD_Version
#define XCD_Version 0x61240
#undef CD_ReadData
#define CD_ReadData 0x41241
#undef XCD_ReadData
#define XCD_ReadData 0x61241
#undef CD_SeekTo
#define CD_SeekTo 0x41242
#undef XCD_SeekTo
#define XCD_SeekTo 0x61242
#undef CD_DriveStatus
#define CD_DriveStatus 0x41243
#undef XCD_DriveStatus
#define XCD_DriveStatus 0x61243
#undef CD_DriveReady
#define CD_DriveReady 0x41244
#undef XCD_DriveReady
#define XCD_DriveReady 0x61244
#undef CD_GetParameters
#define CD_GetParameters 0x41245
#undef XCD_GetParameters
#define XCD_GetParameters 0x61245
#undef CD_SetParameters
#define CD_SetParameters 0x41246
#undef XCD_SetParameters
#define XCD_SetParameters 0x61246
#undef CD_OpenDrawer
#define CD_OpenDrawer 0x41247
#undef XCD_OpenDrawer
#define XCD_OpenDrawer 0x61247
#undef CD_EjectButton
#define CD_EjectButton 0x41248
#undef XCD_EjectButton
#define XCD_EjectButton 0x61248
#undef CD_EnquireAddress
#define CD_EnquireAddress 0x41249
#undef XCD_EnquireAddress
#define XCD_EnquireAddress 0x61249
#undef CD_EnquireDataMode
#define CD_EnquireDataMode 0x4124A
#undef XCD_EnquireDataMode
#define XCD_EnquireDataMode 0x6124A
#undef CD_PlayAudio
#define CD_PlayAudio 0x4124B
#undef XCD_PlayAudio
#define XCD_PlayAudio 0x6124B
#undef CD_PlayTrack
#define CD_PlayTrack 0x4124C
#undef XCD_PlayTrack
#define XCD_PlayTrack 0x6124C
#undef CD_AudioPause
#define CD_AudioPause 0x4124D
#undef XCD_AudioPause
#define XCD_AudioPause 0x6124D
#undef CD_EnquireTrack
#define CD_EnquireTrack 0x4124E
#undef XCD_EnquireTrack
#define XCD_EnquireTrack 0x6124E
#undef CD_ReadSubChannel
#define CD_ReadSubChannel 0x4124F
#undef XCD_ReadSubChannel
#define XCD_ReadSubChannel 0x6124F
#undef CD_CheckDrive
#define CD_CheckDrive 0x41250
#undef XCD_CheckDrive
#define XCD_CheckDrive 0x61250
#undef CD_DiscChanged
#define CD_DiscChanged 0x41251
#undef XCD_DiscChanged
#define XCD_DiscChanged 0x61251
#undef CD_StopDisc
#define CD_StopDisc 0x41252
#undef XCD_StopDisc
#define XCD_StopDisc 0x61252
#undef CD_DiscUsed
#define CD_DiscUsed 0x41253
#undef XCD_DiscUsed
#define XCD_DiscUsed 0x61253
#undef CD_AudioStatus
#define CD_AudioStatus 0x41254
#undef XCD_AudioStatus
#define XCD_AudioStatus 0x61254
#undef CD_Inquiry
#define CD_Inquiry 0x41255
#undef XCD_Inquiry
#define XCD_Inquiry 0x61255
#undef CD_DiscHasChanged
#define CD_DiscHasChanged 0x41256
#undef XCD_DiscHasChanged
#define XCD_DiscHasChanged 0x61256
#undef CD_Control
#define CD_Control 0x41257
#undef XCD_Control
#define XCD_Control 0x61257
#undef CD_Supported
#define CD_Supported 0x41258
#undef XCD_Supported
#define XCD_Supported 0x61258
#undef CD_Prefetch
#define CD_Prefetch 0x41259
#undef XCD_Prefetch
#define XCD_Prefetch 0x61259
#undef CD_Reset
#define CD_Reset 0x4125A
#undef XCD_Reset
#define XCD_Reset 0x6125A
#undef CD_CloseDrawer
#define CD_CloseDrawer 0x4125B
#undef XCD_CloseDrawer
#define XCD_CloseDrawer 0x6125B
#undef CD_IsDrawerLocked
#define CD_IsDrawerLocked 0x4125C
#undef XCD_IsDrawerLocked
#define XCD_IsDrawerLocked 0x6125C
#undef CD_AudioControl
#define CD_AudioControl 0x4125D
#undef XCD_AudioControl
#define XCD_AudioControl 0x6125D
#undef CD_LastError
#define CD_LastError 0x4125E
#undef XCD_LastError
#define XCD_LastError 0x6125E
#undef CD_AudioLevel
#define CD_AudioLevel 0x4125F
#undef XCD_AudioLevel
#define XCD_AudioLevel 0x6125F
#undef CD_Register
#define CD_Register 0x41260
#undef XCD_Register
#define XCD_Register 0x61260
#undef CD_Unregister
#define CD_Unregister 0x41261
#undef XCD_Unregister
#define XCD_Unregister 0x61261
#undef CD_ByteCopy
#define CD_ByteCopy 0x41262
#undef XCD_ByteCopy
#define XCD_ByteCopy 0x61262
#undef CD_Identify
#define CD_Identify 0x41263
#undef XCD_Identify
#define XCD_Identify 0x61263
#undef CD_ConvertToLBA
#define CD_ConvertToLBA 0x41264
#undef XCD_ConvertToLBA
#define XCD_ConvertToLBA 0x61264
#undef CD_ConvertToMSF
#define CD_ConvertToMSF 0x41265
#undef XCD_ConvertToMSF
#define XCD_ConvertToMSF 0x61265
#undef CD_ReadAudio
#define CD_ReadAudio 0x41266
#undef XCD_ReadAudio
#define XCD_ReadAudio 0x61266
#undef CD_ReadUserData
#define CD_ReadUserData 0x41267
#undef XCD_ReadUserData
#define XCD_ReadUserData 0x61267
#undef CD_SeekUserData
#define CD_SeekUserData 0x41268
#undef XCD_SeekUserData
#define XCD_SeekUserData 0x61268
#undef CD_GetAudioParms
#define CD_GetAudioParms 0x41269
#undef XCD_GetAudioParms
#define XCD_GetAudioParms 0x61269
#undef CD_SetAudioParms
#define CD_SetAudioParms 0x4126A
#undef XCD_SetAudioParms
#define XCD_SetAudioParms 0x6126A
#undef CD_SCSIUserOp
#define CD_SCSIUserOp 0x4126B
#undef XCD_SCSIUserOp
#define XCD_SCSIUserOp 0x6126B
#undef MessageTrans_FileInfo
#define MessageTrans_FileInfo 0x41500
#undef XMessageTrans_FileInfo
#define XMessageTrans_FileInfo 0x61500
#undef MessageTrans_OpenFile
#define MessageTrans_OpenFile 0x41501
#undef XMessageTrans_OpenFile
#define XMessageTrans_OpenFile 0x61501
#undef MessageTrans_Lookup
#define MessageTrans_Lookup 0x41502
#undef XMessageTrans_Lookup
#define XMessageTrans_Lookup 0x61502
#undef MessageTrans_MakeMenus
#define MessageTrans_MakeMenus 0x41503
#undef XMessageTrans_MakeMenus
#define XMessageTrans_MakeMenus 0x61503
#undef MessageTrans_CloseFile
#define MessageTrans_CloseFile 0x41504
#undef XMessageTrans_CloseFile
#define XMessageTrans_CloseFile 0x61504
#undef MessageTrans_EnumerateTokens
#define MessageTrans_EnumerateTokens 0x41505
#undef XMessageTrans_EnumerateTokens
#define XMessageTrans_EnumerateTokens 0x61505
#undef MessageTrans_ErrorLookup
#define MessageTrans_ErrorLookup 0x41506
#undef XMessageTrans_ErrorLookup
#define XMessageTrans_ErrorLookup 0x61506
#undef MessageTrans_GSLookup
#define MessageTrans_GSLookup 0x41507
#undef XMessageTrans_GSLookup
#define XMessageTrans_GSLookup 0x61507
#undef MessageTrans_CopyError
#define MessageTrans_CopyError 0x41508
#undef XMessageTrans_CopyError
#define XMessageTrans_CopyError 0x61508
#undef MessageTrans_Dictionary
#define MessageTrans_Dictionary 0x41509
#undef XMessageTrans_Dictionary
#define XMessageTrans_Dictionary 0x61509
#undef PDumper_Info
#define PDumper_Info 0x41B00
#undef XPDumper_Info
#define XPDumper_Info 0x61B00
#undef PDumper_Claim
#define PDumper_Claim 0x41B01
#undef XPDumper_Claim
#define XPDumper_Claim 0x61B01
#undef PDumper_Free
#define PDumper_Free 0x41B02
#undef XPDumper_Free
#define XPDumper_Free 0x61B02
#undef PDumper_Find
#define PDumper_Find 0x41B03
#undef XPDumper_Find
#define XPDumper_Find 0x61B03
#undef PDumper_StartJob
#define PDumper_StartJob 0x41B04
#undef XPDumper_StartJob
#define XPDumper_StartJob 0x61B04
#undef PDumper_TidyJob
#define PDumper_TidyJob 0x41B05
#undef XPDumper_TidyJob
#define XPDumper_TidyJob 0x61B05
#undef PDumper_SetColour
#define PDumper_SetColour 0x41B06
#undef XPDumper_SetColour
#define XPDumper_SetColour 0x61B06
#undef PDumper_PrepareStrip
#define PDumper_PrepareStrip 0x41B07
#undef XPDumper_PrepareStrip
#define XPDumper_PrepareStrip 0x61B07
#undef PDumper_LookupError
#define PDumper_LookupError 0x41B08
#undef XPDumper_LookupError
#define XPDumper_LookupError 0x61B08
#undef PDumper_CopyFilename
#define PDumper_CopyFilename 0x41B09
#undef XPDumper_CopyFilename
#define XPDumper_CopyFilename 0x61B09
#undef ResourceFS_RegisterFiles
#define ResourceFS_RegisterFiles 0x41B40
#undef XResourceFS_RegisterFiles
#define XResourceFS_RegisterFiles 0x61B40
#undef ResourceFS_DeregisterFiles
#define ResourceFS_DeregisterFiles 0x41B41
#undef XResourceFS_DeregisterFiles
#define XResourceFS_DeregisterFiles 0x61B41
#undef Debugger_DebugAIF
#define Debugger_DebugAIF 0x41D40
#undef XDebugger_DebugAIF
#define XDebugger_DebugAIF 0x61D40
#undef Debugger_BeingDebugged
#define Debugger_BeingDebugged 0x41D41
#undef XDebugger_BeingDebugged
#define XDebugger_BeingDebugged 0x61D41
#undef Debugger_StartDebug
#define Debugger_StartDebug 0x41D42
#undef XDebugger_StartDebug
#define XDebugger_StartDebug 0x61D42
#undef Debugger_EndDebug
#define Debugger_EndDebug 0x41D43
#undef XDebugger_EndDebug
#define XDebugger_EndDebug 0x61D43
#undef CDFS_ConvertDriveToDevice
#define CDFS_ConvertDriveToDevice 0x41E80
#undef XCDFS_ConvertDriveToDevice
#define XCDFS_ConvertDriveToDevice 0x61E80
#undef CDFS_SetBufferSize
#define CDFS_SetBufferSize 0x41E81
#undef XCDFS_SetBufferSize
#define XCDFS_SetBufferSize 0x61E81
#undef CDFS_GetBufferSize
#define CDFS_GetBufferSize 0x41E82
#undef XCDFS_GetBufferSize
#define XCDFS_GetBufferSize 0x61E82
#undef CDFS_SetNumberOfDrives
#define CDFS_SetNumberOfDrives 0x41E83
#undef XCDFS_SetNumberOfDrives
#define XCDFS_SetNumberOfDrives 0x61E83
#undef CDFS_GetNumberOfDrives
#define CDFS_GetNumberOfDrives 0x41E84
#undef XCDFS_GetNumberOfDrives
#define XCDFS_GetNumberOfDrives 0x61E84
#undef CDFS_GiveFileType
#define CDFS_GiveFileType 0x41E85
#undef XCDFS_GiveFileType
#define XCDFS_GiveFileType 0x61E85
#undef CDFS_DescribeDisc
#define CDFS_DescribeDisc 0x41E86
#undef XCDFS_DescribeDisc
#define XCDFS_DescribeDisc 0x61E86
#undef DragASprite_Start
#define DragASprite_Start 0x42400
#undef XDragASprite_Start
#define XDragASprite_Start 0x62400
#undef DragASprite_Stop
#define DragASprite_Stop 0x42401
#undef XDragASprite_Stop
#define XDragASprite_Stop 0x62401
#undef DDEUtils_Prefix
#define DDEUtils_Prefix 0x42580
#undef XDDEUtils_Prefix
#define XDDEUtils_Prefix 0x62580
#undef DDEUtils_SetCLSize
#define DDEUtils_SetCLSize 0x42581
#undef XDDEUtils_SetCLSize
#define XDDEUtils_SetCLSize 0x62581
#undef DDEUtils_SetCL
#define DDEUtils_SetCL 0x42582
#undef XDDEUtils_SetCL
#define XDDEUtils_SetCL 0x62582
#undef DDEUtils_GetCLSize
#define DDEUtils_GetCLSize 0x42583
#undef XDDEUtils_GetCLSize
#define XDDEUtils_GetCLSize 0x62583
#undef DDEUtils_GetCl
#define DDEUtils_GetCl 0x42584
#undef XDDEUtils_GetCl
#define XDDEUtils_GetCl 0x62584
#undef DDEUtils_ThrowbackRegister
#define DDEUtils_ThrowbackRegister 0x42585
#undef XDDEUtils_ThrowbackRegister
#define XDDEUtils_ThrowbackRegister 0x62585
#undef DDEUtils_ThrowbackUnRegister
#define DDEUtils_ThrowbackUnRegister 0x42586
#undef XDDEUtils_ThrowbackUnRegister
#define XDDEUtils_ThrowbackUnRegister 0x62586
#undef DDEUtils_ThrowbackStart
#define DDEUtils_ThrowbackStart 0x42587
#undef XDDEUtils_ThrowbackStart
#define XDDEUtils_ThrowbackStart 0x62587
#undef DDEUtils_ThrowbackSent
#define DDEUtils_ThrowbackSent 0x42588
#undef XDDEUtils_ThrowbackSent
#define XDDEUtils_ThrowbackSent 0x62588
#undef DDEUtils_ThrowbackEnd
#define DDEUtils_ThrowbackEnd 0x42589
#undef XDDEUtils_ThrowbackEnd
#define XDDEUtils_ThrowbackEnd 0x62589
#undef DDEUtils_ReadPrefix
#define DDEUtils_ReadPrefix 0x4258A
#undef XDDEUtils_ReadPrefix
#define XDDEUtils_ReadPrefix 0x6258A
#undef DDEUtils_FlushCL
#define DDEUtils_FlushCL 0x4258B
#undef XDDEUtils_FlushCL
#define XDDEUtils_FlushCL 0x6258B
#undef Filter_RegisterPreFilter
#define Filter_RegisterPreFilter 0x42640
#undef XFilter_RegisterPreFilter
#define XFilter_RegisterPreFilter 0x62640
#undef Filter_RegisterPostFilter
#define Filter_RegisterPostFilter 0x42641
#undef XFilter_RegisterPostFilter
#define XFilter_RegisterPostFilter 0x62641
#undef Filter_DeRegisterPreFilter
#define Filter_DeRegisterPreFilter 0x42642
#undef XFilter_DeRegisterPreFilter
#define XFilter_DeRegisterPreFilter 0x62642
#undef Filter_DeRegisterPostFilter
#define Filter_DeRegisterPostFilter 0x42643
#undef XFilter_DeRegisterPostFilter
#define XFilter_DeRegisterPostFilter 0x62643
#undef Filter_RegisterRectFilter
#define Filter_RegisterRectFilter 0x42644
#undef XFilter_RegisterRectFilter
#define XFilter_RegisterRectFilter 0x62644
#undef Filter_DeRegisterRectFilter
#define Filter_DeRegisterRectFilter 0x42645
#undef XFilter_DeRegisterRectFilter
#define XFilter_DeRegisterRectFilter 0x62645
#undef Filter_RegisterCopyFilter
#define Filter_RegisterCopyFilter 0x42646
#undef XFilter_RegisterCopyFilter
#define XFilter_RegisterCopyFilter 0x62646
#undef Filter_DeRegisterCopyFilter
#define Filter_DeRegisterCopyFilter 0x42647
#undef XFilter_DeRegisterCopyFilter
#define XFilter_DeRegisterCopyFilter 0x62647
#undef Filter_RegisterPostRectFilter
#define Filter_RegisterPostRectFilter 0x42648
#undef XFilter_RegisterPostRectFilter
#define XFilter_RegisterPostRectFilter 0x62648
#undef Filter_DeRegisterPostRectFilter
#define Filter_DeRegisterPostRectFilter 0x42649
#undef XFilter_DeRegisterPostRectFilter
#define XFilter_DeRegisterPostRectFilter 0x62649
#undef Filter_RegisterPostIconFilter
#define Filter_RegisterPostIconFilter 0x4264A
#undef XFilter_RegisterPostIconFilter
#define XFilter_RegisterPostIconFilter 0x6264A
#undef Filter_DeRegisterPostIconFilter
#define Filter_DeRegisterPostIconFilter 0x4264B
#undef XFilter_DeRegisterPostIconFilter
#define XFilter_DeRegisterPostIconFilter 0x6264B
#undef TaskManager_TaskNameFromHandle
#define TaskManager_TaskNameFromHandle 0x42680
#undef XTaskManager_TaskNameFromHandle
#define XTaskManager_TaskNameFromHandle 0x62680
#undef TaskManager_EnumerateTasks
#define TaskManager_EnumerateTasks 0x42681
#undef XTaskManager_EnumerateTasks
#define XTaskManager_EnumerateTasks 0x62681
#undef TaskManager_Shutdown
#define TaskManager_Shutdown 0x42682
#undef XTaskManager_Shutdown
#define XTaskManager_Shutdown 0x62682
#undef TaskManager_StartTask
#define TaskManager_StartTask 0x42683
#undef XTaskManager_StartTask
#define XTaskManager_StartTask 0x62683
#undef Squash_Compress
#define Squash_Compress 0x42700
#undef XSquash_Compress
#define XSquash_Compress 0x62700
#undef Squash_Decompress
#define Squash_Decompress 0x42701
#undef XSquash_Decompress
#define XSquash_Decompress 0x62701
#undef DeviceFS_Register
#define DeviceFS_Register 0x42740
#undef XDeviceFS_Register
#define XDeviceFS_Register 0x62740
#undef DeviceFS_Deregister
#define DeviceFS_Deregister 0x42741
#undef XDeviceFS_Deregister
#define XDeviceFS_Deregister 0x62741
#undef DeviceFS_RegisterObjects
#define DeviceFS_RegisterObjects 0x42742
#undef XDeviceFS_RegisterObjects
#define XDeviceFS_RegisterObjects 0x62742
#undef DeviceFS_DeregisterObjects
#define DeviceFS_DeregisterObjects 0x42743
#undef XDeviceFS_DeregisterObjects
#define XDeviceFS_DeregisterObjects 0x62743
#undef DeviceFS_CallDevice
#define DeviceFS_CallDevice 0x42744
#undef XDeviceFS_CallDevice
#define XDeviceFS_CallDevice 0x62744
#undef DeviceFS_Threshold
#define DeviceFS_Threshold 0x42745
#undef XDeviceFS_Threshold
#define XDeviceFS_Threshold 0x62745
#undef DeviceFS_ReceivedCharacter
#define DeviceFS_ReceivedCharacter 0x42746
#undef XDeviceFS_ReceivedCharacter
#define XDeviceFS_ReceivedCharacter 0x62746
#undef DeviceFS_TransmitCharacter
#define DeviceFS_TransmitCharacter 0x42747
#undef XDeviceFS_TransmitCharacter
#define XDeviceFS_TransmitCharacter 0x62747
#undef Buffer_Create
#define Buffer_Create 0x42940
#undef XBuffer_Create
#define XBuffer_Create 0x62940
#undef Buffer_Remove
#define Buffer_Remove 0x42941
#undef XBuffer_Remove
#define XBuffer_Remove 0x62941
#undef Buffer_Register
#define Buffer_Register 0x42942
#undef XBuffer_Register
#define XBuffer_Register 0x62942
#undef Buffer_Deregister
#define Buffer_Deregister 0x42943
#undef XBuffer_Deregister
#define XBuffer_Deregister 0x62943
#undef Buffer_ModifyFlags
#define Buffer_ModifyFlags 0x42944
#undef XBuffer_ModifyFlags
#define XBuffer_ModifyFlags 0x62944
#undef Buffer_LinkDevice
#define Buffer_LinkDevice 0x42945
#undef XBuffer_LinkDevice
#define XBuffer_LinkDevice 0x62945
#undef Buffer_UnlinkDevice
#define Buffer_UnlinkDevice 0x42946
#undef XBuffer_UnlinkDevice
#define XBuffer_UnlinkDevice 0x62946
#undef Buffer_GetInfo
#define Buffer_GetInfo 0x42947
#undef XBuffer_GetInfo
#define XBuffer_GetInfo 0x62947
#undef Buffer_Threshold
#define Buffer_Threshold 0x42948
#undef XBuffer_Threshold
#define XBuffer_Threshold 0x62948
#undef Buffer_InternalInfo
#define Buffer_InternalInfo 0x42949
#undef XBuffer_InternalInfo
#define XBuffer_InternalInfo 0x62949
#undef Portable_Speed
#define Portable_Speed 0x42FC0
#undef XPortable_Speed
#define XPortable_Speed 0x62FC0
#undef Portable_Control
#define Portable_Control 0x42FC1
#undef XPortable_Control
#define XPortable_Control 0x62FC1
#undef Portable_ReadBMUVariable
#define Portable_ReadBMUVariable 0x42FC2
#undef XPortable_ReadBMUVariable
#define XPortable_ReadBMUVariable 0x62FC2
#undef Portable_WriteBMUVariable
#define Portable_WriteBMUVariable 0x42FC3
#undef XPortable_WriteBMUVariable
#define XPortable_WriteBMUVariable 0x62FC3
#undef Portable_CommandBMU
#define Portable_CommandBMU 0x42FC4
#undef XPortable_CommandBMU
#define XPortable_CommandBMU 0x62FC4
#undef Portable_ReadFeatures
#define Portable_ReadFeatures 0x42FC5
#undef XPortable_ReadFeatures
#define XPortable_ReadFeatures 0x62FC5
#undef Portable_Idle
#define Portable_Idle 0x42FC6
#undef XPortable_Idle
#define XPortable_Idle 0x62FC6
#undef Portable_Stop
#define Portable_Stop 0x42FC7
#undef XPortable_Stop
#define XPortable_Stop 0x62FC7
#undef Portable_Status
#define Portable_Status 0x42FC8
#undef XPortable_Status
#define XPortable_Status 0x62FC8
#undef Portable_Contrast
#define Portable_Contrast 0x42FC9
#undef XPortable_Contrast
#define XPortable_Contrast 0x62FC9
#undef Portable_Refresh
#define Portable_Refresh 0x42FCA
#undef XPortable_Refresh
#define XPortable_Refresh 0x62FCA
#undef Portable_Halt
#define Portable_Halt 0x42FCB
#undef XPortable_Halt
#define XPortable_Halt 0x62FCB
#undef Portable_SleepTime
#define Portable_SleepTime 0x42FCC
#undef XPortable_SleepTime
#define XPortable_SleepTime 0x62FCC
#undef Portable_SMBusOp
#define Portable_SMBusOp 0x42FCD
#undef XPortable_SMBusOp
#define XPortable_SMBusOp 0x62FCD
#undef Portable_Speed2
#define Portable_Speed2 0x42FCE
#undef XPortable_Speed2
#define XPortable_Speed2 0x62FCE
#undef Portable_WakeTime
#define Portable_WakeTime 0x42FCF
#undef XPortable_WakeTime
#define XPortable_WakeTime 0x62FCF
#undef Portable_EnumerateBMU
#define Portable_EnumerateBMU 0x42FD0
#undef XPortable_EnumerateBMU
#define XPortable_EnumerateBMU 0x62FD0
#undef Portable_ReadBMUVariables
#define Portable_ReadBMUVariables 0x42FD1
#undef XPortable_ReadBMUVariables
#define XPortable_ReadBMUVariables 0x62FD1
#undef Portable_ReadSensor
#define Portable_ReadSensor 0x42FD2
#undef XPortable_ReadSensor
#define XPortable_ReadSensor 0x62FD2
#undef Territory_Number
#define Territory_Number 0x43040
#undef XTerritory_Number
#define XTerritory_Number 0x63040
#undef Territory_Register
#define Territory_Register 0x43041
#undef XTerritory_Register
#define XTerritory_Register 0x63041
#undef Territory_Deregister
#define Territory_Deregister 0x43042
#undef XTerritory_Deregister
#define XTerritory_Deregister 0x63042
#undef Territory_NumberToName
#define Territory_NumberToName 0x43043
#undef XTerritory_NumberToName
#define XTerritory_NumberToName 0x63043
#undef Territory_Exists
#define Territory_Exists 0x43044
#undef XTerritory_Exists
#define XTerritory_Exists 0x63044
#undef Territory_AlphabetNumberToName
#define Territory_AlphabetNumberToName 0x43045
#undef XTerritory_AlphabetNumberToName
#define XTerritory_AlphabetNumberToName 0x63045
#undef Territory_SelectAlphabet
#define Territory_SelectAlphabet 0x43046
#undef XTerritory_SelectAlphabet
#define XTerritory_SelectAlphabet 0x63046
#undef Territory_SetTime
#define Territory_SetTime 0x43047
#undef XTerritory_SetTime
#define XTerritory_SetTime 0x63047
#undef Territory_ReadCurrentTimeZone
#define Territory_ReadCurrentTimeZone 0x43048
#undef XTerritory_ReadCurrentTimeZone
#define XTerritory_ReadCurrentTimeZone 0x63048
#undef Territory_ConvertTimeToUTCOrdinals
#define Territory_ConvertTimeToUTCOrdinals 0x43049
#undef XTerritory_ConvertTimeToUTCOrdinals
#define XTerritory_ConvertTimeToUTCOrdinals 0x63049
#undef Territory_ReadTimeZones
#define Territory_ReadTimeZones 0x4304A
#undef XTerritory_ReadTimeZones
#define XTerritory_ReadTimeZones 0x6304A
#undef Territory_ConvertDateAndTime
#define Territory_ConvertDateAndTime 0x4304B
#undef XTerritory_ConvertDateAndTime
#define XTerritory_ConvertDateAndTime 0x6304B
#undef Territory_ConvertStandardDateAndTime
#define Territory_ConvertStandardDateAndTime 0x4304C
#undef XTerritory_ConvertStandardDateAndTime
#define XTerritory_ConvertStandardDateAndTime 0x6304C
#undef Territory_ConvertStandardDate
#define Territory_ConvertStandardDate 0x4304D
#undef XTerritory_ConvertStandardDate
#define XTerritory_ConvertStandardDate 0x6304D
#undef Territory_ConvertStandardTime
#define Territory_ConvertStandardTime 0x4304E
#undef XTerritory_ConvertStandardTime
#define XTerritory_ConvertStandardTime 0x6304E
#undef Territory_ConvertTimeToOrdinals
#define Territory_ConvertTimeToOrdinals 0x4304F
#undef XTerritory_ConvertTimeToOrdinals
#define XTerritory_ConvertTimeToOrdinals 0x6304F
#undef Territory_ConvertTimeStringToOrdinals
#define Territory_ConvertTimeStringToOrdinals 0x43050
#undef XTerritory_ConvertTimeStringToOrdinals
#define XTerritory_ConvertTimeStringToOrdinals 0x63050
#undef Territory_ConvertOrdinalsToTime
#define Territory_ConvertOrdinalsToTime 0x43051
#undef XTerritory_ConvertOrdinalsToTime
#define XTerritory_ConvertOrdinalsToTime 0x63051
#undef Territory_Alphabet
#define Territory_Alphabet 0x43052
#undef XTerritory_Alphabet
#define XTerritory_Alphabet 0x63052
#undef Territory_AlphabetIdentifier
#define Territory_AlphabetIdentifier 0x43053
#undef XTerritory_AlphabetIdentifier
#define XTerritory_AlphabetIdentifier 0x63053
#undef Territory_SelectKeyboardHandler
#define Territory_SelectKeyboardHandler 0x43054
#undef XTerritory_SelectKeyboardHandler
#define XTerritory_SelectKeyboardHandler 0x63054
#undef Territory_WriteDirection
#define Territory_WriteDirection 0x43055
#undef XTerritory_WriteDirection
#define XTerritory_WriteDirection 0x63055
#undef Territory_CharacterPropertyTable
#define Territory_CharacterPropertyTable 0x43056
#undef XTerritory_CharacterPropertyTable
#define XTerritory_CharacterPropertyTable 0x63056
#undef Territory_LowerCaseTable
#define Territory_LowerCaseTable 0x43057
#undef XTerritory_LowerCaseTable
#define XTerritory_LowerCaseTable 0x63057
#undef Territory_UpperCaseTable
#define Territory_UpperCaseTable 0x43058
#undef XTerritory_UpperCaseTable
#define XTerritory_UpperCaseTable 0x63058
#undef Territory_ControlTable
#define Territory_ControlTable 0x43059
#undef XTerritory_ControlTable
#define XTerritory_ControlTable 0x63059
#undef Territory_PlainTable
#define Territory_PlainTable 0x4305A
#undef XTerritory_PlainTable
#define XTerritory_PlainTable 0x6305A
#undef Territory_ValueTable
#define Territory_ValueTable 0x4305B
#undef XTerritory_ValueTable
#define XTerritory_ValueTable 0x6305B
#undef Territory_RepresentationTable
#define Territory_RepresentationTable 0x4305C
#undef XTerritory_RepresentationTable
#define XTerritory_RepresentationTable 0x6305C
#undef Territory_Collate
#define Territory_Collate 0x4305D
#undef XTerritory_Collate
#define XTerritory_Collate 0x6305D
#undef Territory_ReadSymbols
#define Territory_ReadSymbols 0x4305E
#undef XTerritory_ReadSymbols
#define XTerritory_ReadSymbols 0x6305E
#undef Territory_ReadCalendarInformation
#define Territory_ReadCalendarInformation 0x4305F
#undef XTerritory_ReadCalendarInformation
#define XTerritory_ReadCalendarInformation 0x6305F
#undef Territory_NameToNumber
#define Territory_NameToNumber 0x43060
#undef XTerritory_NameToNumber
#define XTerritory_NameToNumber 0x63060
#undef Territory_TransformString
#define Territory_TransformString 0x43061
#undef XTerritory_TransformString
#define XTerritory_TransformString 0x63061
#undef Territory_IME
#define Territory_IME 0x43062
#undef XTerritory_IME
#define XTerritory_IME 0x63062
#undef Territory_DaylightRules
#define Territory_DaylightRules 0x43063
#undef XTerritory_DaylightRules
#define XTerritory_DaylightRules 0x63063
#undef Territory_ConvertTextToString
#define Territory_ConvertTextToString 0x43075
#undef XTerritory_ConvertTextToString
#define XTerritory_ConvertTextToString 0x63075
#undef Territory_Select
#define Territory_Select 0x43076
#undef XTerritory_Select
#define XTerritory_Select 0x63076
#undef Territory_DaylightSaving
#define Territory_DaylightSaving 0x43077
#undef XTerritory_DaylightSaving
#define XTerritory_DaylightSaving 0x63077
#undef Territory_ConvertTimeFormats
#define Territory_ConvertTimeFormats 0x43078
#undef XTerritory_ConvertTimeFormats
#define XTerritory_ConvertTimeFormats 0x63078
#undef ScreenBlanker_Control
#define ScreenBlanker_Control 0x43100
#undef XScreenBlanker_Control
#define XScreenBlanker_Control 0x63100
#undef TaskWindow_TaskInfo
#define TaskWindow_TaskInfo 0x43380
#undef XTaskWindow_TaskInfo
#define XTaskWindow_TaskInfo 0x63380
#undef MakePSFont_MakeFont
#define MakePSFont_MakeFont 0x43440
#undef XMakePSFont_MakeFont
#define XMakePSFont_MakeFont 0x63440
#undef Free_Register
#define Free_Register 0x444C0
#undef XFree_Register
#define XFree_Register 0x644C0
#undef Free_DeRegister
#define Free_DeRegister 0x444C1
#undef XFree_DeRegister
#define XFree_DeRegister 0x644C1
#undef FSLock_Version
#define FSLock_Version 0x44780
#undef XFSLock_Version
#define XFSLock_Version 0x64780
#undef FSLock_Status
#define FSLock_Status 0x44781
#undef XFSLock_Status
#define XFSLock_Status 0x64781
#undef FSLock_ChangeStatus
#define FSLock_ChangeStatus 0x44782
#undef XFSLock_ChangeStatus
#define XFSLock_ChangeStatus 0x64782
#undef DOSFS_DiscFormat
#define DOSFS_DiscFormat 0x44B00
#undef XDOSFS_DiscFormat
#define XDOSFS_DiscFormat 0x64B00
#undef DOSFS_LayoutStructure
#define DOSFS_LayoutStructure 0x44B01
#undef XDOSFS_LayoutStructure
#define XDOSFS_LayoutStructure 0x64B01
#undef Toolbox_CreateObject
#define Toolbox_CreateObject 0x44EC0
#undef XToolbox_CreateObject
#define XToolbox_CreateObject 0x64EC0
#undef Toolbox_DeleteObject
#define Toolbox_DeleteObject 0x44EC1
#undef XToolbox_DeleteObject
#define XToolbox_DeleteObject 0x64EC1
#undef Toolbox_CopyObject
#define Toolbox_CopyObject 0x44EC2
#undef XToolbox_CopyObject
#define XToolbox_CopyObject 0x64EC2
#undef Toolbox_ShowObject
#define Toolbox_ShowObject 0x44EC3
#undef XToolbox_ShowObject
#define XToolbox_ShowObject 0x64EC3
#undef Toolbox_HideObject
#define Toolbox_HideObject 0x44EC4
#undef XToolbox_HideObject
#define XToolbox_HideObject 0x64EC4
#undef Toolbox_GetObjectState
#define Toolbox_GetObjectState 0x44EC5
#undef XToolbox_GetObjectState
#define XToolbox_GetObjectState 0x64EC5
#undef Toolbox_ObjectMiscOp
#define Toolbox_ObjectMiscOp 0x44EC6
#undef XToolbox_ObjectMiscOp
#define XToolbox_ObjectMiscOp 0x64EC6
#undef Toolbox_SetClientHandle
#define Toolbox_SetClientHandle 0x44EC7
#undef XToolbox_SetClientHandle
#define XToolbox_SetClientHandle 0x64EC7
#undef Toolbox_GetClientHandle
#define Toolbox_GetClientHandle 0x44EC8
#undef XToolbox_GetClientHandle
#define XToolbox_GetClientHandle 0x64EC8
#undef Toolbox_GetObjectClass
#define Toolbox_GetObjectClass 0x44EC9
#undef XToolbox_GetObjectClass
#define XToolbox_GetObjectClass 0x64EC9
#undef Toolbox_GetParent
#define Toolbox_GetParent 0x44ECA
#undef XToolbox_GetParent
#define XToolbox_GetParent 0x64ECA
#undef Toolbox_GetAncestor
#define Toolbox_GetAncestor 0x44ECB
#undef XToolbox_GetAncestor
#define XToolbox_GetAncestor 0x64ECB
#undef Toolbox_GetTemplateName
#define Toolbox_GetTemplateName 0x44ECC
#undef XToolbox_GetTemplateName
#define XToolbox_GetTemplateName 0x64ECC
#undef Toolbox_RaiseToolboxEvent
#define Toolbox_RaiseToolboxEvent 0x44ECD
#undef XToolbox_RaiseToolboxEvent
#define XToolbox_RaiseToolboxEvent 0x64ECD
#undef Toolbox_GetSysInfo
#define Toolbox_GetSysInfo 0x44ECE
#undef XToolbox_GetSysInfo
#define XToolbox_GetSysInfo 0x64ECE
#undef Toolbox_Initialise
#define Toolbox_Initialise 0x44ECF
#undef XToolbox_Initialise
#define XToolbox_Initialise 0x64ECF
#undef Toolbox_LoadResources
#define Toolbox_LoadResources 0x44ED0
#undef XToolbox_LoadResources
#define XToolbox_LoadResources 0x64ED0
#undef Toolbox_Memory
#define Toolbox_Memory 0x44EF9
#undef XToolbox_Memory
#define XToolbox_Memory 0x64EF9
#undef Toolbox_DeRegisterObjectModule
#define Toolbox_DeRegisterObjectModule 0x44EFA
#undef XToolbox_DeRegisterObjectModule
#define XToolbox_DeRegisterObjectModule 0x64EFA
#undef Toolbox_TemplateLookUp
#define Toolbox_TemplateLookUp 0x44EFB
#undef XToolbox_TemplateLookUp
#define XToolbox_TemplateLookUp 0x64EFB
#undef Toolbox_GetInternalHandle
#define Toolbox_GetInternalHandle 0x44EFC
#undef XToolbox_GetInternalHandle
#define XToolbox_GetInternalHandle 0x64EFC
#undef Toolbox_RegisterObjectModule
#define Toolbox_RegisterObjectModule 0x44EFD
#undef XToolbox_RegisterObjectModule
#define XToolbox_RegisterObjectModule 0x64EFD
#undef Toolbox_RegisterPreFilter
#define Toolbox_RegisterPreFilter 0x44EFE
#undef XToolbox_RegisterPreFilter
#define XToolbox_RegisterPreFilter 0x64EFE
#undef Toolbox_RegisterPostFilter
#define Toolbox_RegisterPostFilter 0x44EFF
#undef XToolbox_RegisterPostFilter
#define XToolbox_RegisterPostFilter 0x64EFF
#undef DMA_RegisterChannel
#define DMA_RegisterChannel 0x46140
#undef XDMA_RegisterChannel
#define XDMA_RegisterChannel 0x66140
#undef DMA_DeregisterChannel
#define DMA_DeregisterChannel 0x46141
#undef XDMA_DeregisterChannel
#define XDMA_DeregisterChannel 0x66141
#undef DMA_QueueTransfer
#define DMA_QueueTransfer 0x46142
#undef XDMA_QueueTransfer
#define XDMA_QueueTransfer 0x66142
#undef DMA_TerminateTransfer
#define DMA_TerminateTransfer 0x46143
#undef XDMA_TerminateTransfer
#define XDMA_TerminateTransfer 0x66143
#undef DMA_SuspendTransfer
#define DMA_SuspendTransfer 0x46144
#undef XDMA_SuspendTransfer
#define XDMA_SuspendTransfer 0x66144
#undef DMA_ResumeTransfer
#define DMA_ResumeTransfer 0x46145
#undef XDMA_ResumeTransfer
#define XDMA_ResumeTransfer 0x66145
#undef DMA_ExamineTransfer
#define DMA_ExamineTransfer 0x46146
#undef XDMA_ExamineTransfer
#define XDMA_ExamineTransfer 0x66146
#undef DMA_AllocateLogicalChannels
#define DMA_AllocateLogicalChannels 0x46147
#undef XDMA_AllocateLogicalChannels
#define XDMA_AllocateLogicalChannels 0x66147
#undef PCCardFS_DiscOp
#define PCCardFS_DiscOp 0x47540
#undef XPCCardFS_DiscOp
#define XPCCardFS_DiscOp 0x67540
#undef PCCardFS_Version
#define PCCardFS_Version 0x47541
#undef XPCCardFS_Version
#define XPCCardFS_Version 0x67541
#undef PCCardFS_Drives
#define PCCardFS_Drives 0x47542
#undef XPCCardFS_Drives
#define XPCCardFS_Drives 0x67542
#undef PCCardFS_FreeSpace
#define PCCardFS_FreeSpace 0x47543
#undef XPCCardFS_FreeSpace
#define XPCCardFS_FreeSpace 0x67543
#undef PCCardFS_b
#define PCCardFS_b 0x47544
#undef XPCCardFS_b
#define XPCCardFS_b 0x67544
#undef PCCardFS_DescribeDisc
#define PCCardFS_DescribeDisc 0x47545
#undef XPCCardFS_DescribeDisc
#define XPCCardFS_DescribeDisc 0x67545
#undef PCCardFS_MiscOp
#define PCCardFS_MiscOp 0x47546
#undef XPCCardFS_MiscOp
#define XPCCardFS_MiscOp 0x67546
#undef ColourPicker_RegisterModel
#define ColourPicker_RegisterModel 0x47700
#undef XColourPicker_RegisterModel
#define XColourPicker_RegisterModel 0x67700
#undef ColourPicker_DeregisterModel
#define ColourPicker_DeregisterModel 0x47701
#undef XColourPicker_DeregisterModel
#define XColourPicker_DeregisterModel 0x67701
#undef ColourPicker_OpenDialogue
#define ColourPicker_OpenDialogue 0x47702
#undef XColourPicker_OpenDialogue
#define XColourPicker_OpenDialogue 0x67702
#undef ColourPicker_CloseDialogue
#define ColourPicker_CloseDialogue 0x47703
#undef XColourPicker_CloseDialogue
#define XColourPicker_CloseDialogue 0x67703
#undef ColourPicker_UpdateDialogue
#define ColourPicker_UpdateDialogue 0x47704
#undef XColourPicker_UpdateDialogue
#define XColourPicker_UpdateDialogue 0x67704
#undef ColourPicker_ReadDialogue
#define ColourPicker_ReadDialogue 0x47705
#undef XColourPicker_ReadDialogue
#define XColourPicker_ReadDialogue 0x67705
#undef ColourPicker_SetColour
#define ColourPicker_SetColour 0x47706
#undef XColourPicker_SetColour
#define XColourPicker_SetColour 0x67706
#undef ColourPicker_HelpReply
#define ColourPicker_HelpReply 0x47707
#undef XColourPicker_HelpReply
#define XColourPicker_HelpReply 0x67707
#undef ColourPicker_ModelSWI
#define ColourPicker_ModelSWI 0x47708
#undef XColourPicker_ModelSWI
#define XColourPicker_ModelSWI 0x67708
#undef Freeway_Register
#define Freeway_Register 0x47A80
#undef XFreeway_Register
#define XFreeway_Register 0x67A80
#undef Freeway_Write
#define Freeway_Write 0x47A81
#undef XFreeway_Write
#define XFreeway_Write 0x67A81
#undef Freeway_Read
#define Freeway_Read 0x47A82
#undef XFreeway_Read
#define XFreeway_Read 0x67A82
#undef Freeway_Enumerate
#define Freeway_Enumerate 0x47A83
#undef XFreeway_Enumerate
#define XFreeway_Enumerate 0x67A83
#undef Freeway_Status
#define Freeway_Status 0x47A84
#undef XFreeway_Status
#define XFreeway_Status 0x67A84
#undef Freeway_Serial
#define Freeway_Serial 0x47A85
#undef XFreeway_Serial
#define XFreeway_Serial 0x67A85
#undef ScreenModes_ReadInfo
#define ScreenModes_ReadInfo 0x487C0
#undef XScreenModes_ReadInfo
#define XScreenModes_ReadInfo 0x687C0
#undef ScreenModes_EnumerateAudioFormats
#define ScreenModes_EnumerateAudioFormats 0x487C1
#undef XScreenModes_EnumerateAudioFormats
#define XScreenModes_EnumerateAudioFormats 0x687C1
#undef ScreenModes_Features
#define ScreenModes_Features 0x487C2
#undef XScreenModes_Features
#define XScreenModes_Features 0x687C2
#undef JPEG_Info
#define JPEG_Info 0x49980
#undef XJPEG_Info
#define XJPEG_Info 0x69980
#undef JPEG_FileInfo
#define JPEG_FileInfo 0x49981
#undef XJPEG_FileInfo
#define XJPEG_FileInfo 0x69981
#undef JPEG_PlotScaled
#define JPEG_PlotScaled 0x49982
#undef XJPEG_PlotScaled
#define XJPEG_PlotScaled 0x69982
#undef JPEG_PlotFileScaled
#define JPEG_PlotFileScaled 0x49983
#undef XJPEG_PlotFileScaled
#define XJPEG_PlotFileScaled 0x69983
#undef JPEG_PlotTransformed
#define JPEG_PlotTransformed 0x49984
#undef XJPEG_PlotTransformed
#define XJPEG_PlotTransformed 0x69984
#undef JPEG_PlotFileTransformed
#define JPEG_PlotFileTransformed 0x49985
#undef XJPEG_PlotFileTransformed
#define XJPEG_PlotFileTransformed 0x69985
#undef JPEG_PDriverIntercept
#define JPEG_PDriverIntercept 0x49986
#undef XJPEG_PDriverIntercept
#define XJPEG_PDriverIntercept 0x69986
#undef DragAnObject_Start
#define DragAnObject_Start 0x49C40
#undef XDragAnObject_Start
#define XDragAnObject_Start 0x69C40
#undef DragAnObject_Stop
#define DragAnObject_Stop 0x49C41
#undef XDragAnObject_Stop
#define XDragAnObject_Stop 0x69C41
#undef InverseTable_Calculate
#define InverseTable_Calculate 0x4BF40
#undef XInverseTable_Calculate
#define XInverseTable_Calculate 0x6BF40
#undef InverseTable_SpriteTable
#define InverseTable_SpriteTable 0x4BF41
#undef XInverseTable_SpriteTable
#define XInverseTable_SpriteTable 0x6BF41
#undef URI_Version
#define URI_Version 0x4E380
#undef XURI_Version
#define XURI_Version 0x6E380
#undef URI_Dispatch
#define URI_Dispatch 0x4E381
#undef XURI_Dispatch
#define XURI_Dispatch 0x6E381
#undef URI_RequestURI
#define URI_RequestURI 0x4E382
#undef XURI_RequestURI
#define XURI_RequestURI 0x6E382
#undef URI_InvalidateURI
#define URI_InvalidateURI 0x4E383
#undef XURI_InvalidateURI
#define XURI_InvalidateURI 0x6E383
#undef SoundCtrl_ExamineMixer
#define SoundCtrl_ExamineMixer 0x50000
#undef XSoundCtrl_ExamineMixer
#define XSoundCtrl_ExamineMixer 0x70000
#undef SoundCtrl_SetMix
#define SoundCtrl_SetMix 0x50001
#undef XSoundCtrl_SetMix
#define XSoundCtrl_SetMix 0x70001
#undef SoundCtrl_GetMix
#define SoundCtrl_GetMix 0x50002
#undef XSoundCtrl_GetMix
#define XSoundCtrl_GetMix 0x70002
#undef PCI_ReadID
#define PCI_ReadID 0x50380
#undef XPCI_ReadID
#define XPCI_ReadID 0x70380
#undef PCI_ReadHeader
#define PCI_ReadHeader 0x50381
#undef XPCI_ReadHeader
#define XPCI_ReadHeader 0x70381
#undef PCI_ReturnNumber
#define PCI_ReturnNumber 0x50382
#undef XPCI_ReturnNumber
#define XPCI_ReturnNumber 0x70382
#undef PCI_EnumerateFunctions
#define PCI_EnumerateFunctions 0x50383
#undef XPCI_EnumerateFunctions
#define XPCI_EnumerateFunctions 0x70383
#undef PCI_IORead
#define PCI_IORead 0x50384
#undef XPCI_IORead
#define XPCI_IORead 0x70384
#undef PCI_IOWrite
#define PCI_IOWrite 0x50385
#undef XPCI_IOWrite
#define XPCI_IOWrite 0x70385
#undef PCI_MemoryRead
#define PCI_MemoryRead 0x50386
#undef XPCI_MemoryRead
#define XPCI_MemoryRead 0x70386
#undef PCI_MemoryWrite
#define PCI_MemoryWrite 0x50387
#undef XPCI_MemoryWrite
#define XPCI_MemoryWrite 0x70387
#undef PCI_ConfigurationRead
#define PCI_ConfigurationRead 0x50388
#undef XPCI_ConfigurationRead
#define XPCI_ConfigurationRead 0x70388
#undef PCI_ConfigurationWrite
#define PCI_ConfigurationWrite 0x50389
#undef XPCI_ConfigurationWrite
#define XPCI_ConfigurationWrite 0x70389
#undef PCI_HardwareAddress
#define PCI_HardwareAddress 0x5038A
#undef XPCI_HardwareAddress
#define XPCI_HardwareAddress 0x7038A
#undef PCI_ReadInfo
#define PCI_ReadInfo 0x5038B
#undef XPCI_ReadInfo
#define XPCI_ReadInfo 0x7038B
#undef PCI_SpecialCycle
#define PCI_SpecialCycle 0x5038C
#undef XPCI_SpecialCycle
#define XPCI_SpecialCycle 0x7038C
#undef PCI_FindByLocation
#define PCI_FindByLocation 0x5038D
#undef XPCI_FindByLocation
#define XPCI_FindByLocation 0x7038D
#undef PCI_FindByID
#define PCI_FindByID 0x5038E
#undef XPCI_FindByID
#define XPCI_FindByID 0x7038E
#undef PCI_FindByClass
#define PCI_FindByClass 0x5038F
#undef XPCI_FindByClass
#define XPCI_FindByClass 0x7038F
#undef PCI_RAMAlloc
#define PCI_RAMAlloc 0x50390
#undef XPCI_RAMAlloc
#define XPCI_RAMAlloc 0x70390
#undef PCI_RAMFree
#define PCI_RAMFree 0x50391
#undef XPCI_RAMFree
#define XPCI_RAMFree 0x70391
#undef PCI_LogicalAddress
#define PCI_LogicalAddress 0x50392
#undef XPCI_LogicalAddress
#define XPCI_LogicalAddress 0x70392
#undef MimeMap_Translate
#define MimeMap_Translate 0x50B00
#undef XMimeMap_Translate
#define XMimeMap_Translate 0x70B00
#undef PortMan_AccessBit
#define PortMan_AccessBit 0x52D80
#undef XPortMan_AccessBit
#define XPortMan_AccessBit 0x72D80
#undef BlendTable_GenerateTable
#define BlendTable_GenerateTable 0x56280
#undef XBlendTable_GenerateTable
#define XBlendTable_GenerateTable 0x76280
#undef BlendTable_UnlockTable
#define BlendTable_UnlockTable 0x56281
#undef XBlendTable_UnlockTable
#define XBlendTable_UnlockTable 0x76281
#undef RT_Register
#define RT_Register 0x575C0
#undef XRT_Register
#define XRT_Register 0x775C0
#undef RT_Deregister
#define RT_Deregister 0x575C1
#undef XRT_Deregister
#define XRT_Deregister 0x775C1
#undef RT_Yield
#define RT_Yield 0x575C2
#undef XRT_Yield
#define XRT_Yield 0x775C2
#undef RT_TimedYield
#define RT_TimedYield 0x575C3
#undef XRT_TimedYield
#define XRT_TimedYield 0x775C3
#undef RT_ChangePriority
#define RT_ChangePriority 0x575C4
#undef XRT_ChangePriority
#define XRT_ChangePriority 0x775C4
#undef RT_ReadInfo
#define RT_ReadInfo 0x575C5
#undef XRT_ReadInfo
#define XRT_ReadInfo 0x775C5
#undef VFPSupport_CheckContext
#define VFPSupport_CheckContext 0x58EC0
#undef XVFPSupport_CheckContext
#define XVFPSupport_CheckContext 0x78EC0
#undef VFPSupport_CreateContext
#define VFPSupport_CreateContext 0x58EC1
#undef XVFPSupport_CreateContext
#define XVFPSupport_CreateContext 0x78EC1
#undef VFPSupport_DestroyContext
#define VFPSupport_DestroyContext 0x58EC2
#undef XVFPSupport_DestroyContext
#define XVFPSupport_DestroyContext 0x78EC2
#undef VFPSupport_ChangeContext
#define VFPSupport_ChangeContext 0x58EC3
#undef XVFPSupport_ChangeContext
#define XVFPSupport_ChangeContext 0x78EC3
#undef VFPSupport_ExamineContext
#define VFPSupport_ExamineContext 0x58EC4
#undef XVFPSupport_ExamineContext
#define XVFPSupport_ExamineContext 0x78EC4
#undef VFPSupport_FastAPI
#define VFPSupport_FastAPI 0x58EC5
#undef XVFPSupport_FastAPI
#define XVFPSupport_FastAPI 0x78EC5
#undef VFPSupport_ActiveContext
#define VFPSupport_ActiveContext 0x58EC6
#undef XVFPSupport_ActiveContext
#define XVFPSupport_ActiveContext 0x78EC6
#undef VFPSupport_Version
#define VFPSupport_Version 0x58EC7
#undef XVFPSupport_Version
#define XVFPSupport_Version 0x78EC7
#undef VFPSupport_Features
#define VFPSupport_Features 0x58EC8
#undef XVFPSupport_Features
#define XVFPSupport_Features 0x78EC8
#undef VFPSupport_ExceptionDump
#define VFPSupport_ExceptionDump 0x58EC9
#undef XVFPSupport_ExceptionDump
#define XVFPSupport_ExceptionDump 0x78EC9
#undef VFPSupport_ElementaryFunctions
#define VFPSupport_ElementaryFunctions 0x58ECA
#undef XVFPSupport_ElementaryFunctions
#define XVFPSupport_ElementaryFunctions 0x78ECA
#undef SDIO_Initialise
#define SDIO_Initialise 0x59000
#undef XSDIO_Initialise
#define XSDIO_Initialise 0x79000
#undef SDIO_Control
#define SDIO_Control 0x59001
#undef XSDIO_Control
#define XSDIO_Control 0x79001
#undef SDIO_Enumerate
#define SDIO_Enumerate 0x59002
#undef XSDIO_Enumerate
#define XSDIO_Enumerate 0x79002
#undef SDIO_ControllerFeatures
#define SDIO_ControllerFeatures 0x59003
#undef XSDIO_ControllerFeatures
#define XSDIO_ControllerFeatures 0x79003
#undef SDIO_ReadRegister
#define SDIO_ReadRegister 0x59004
#undef XSDIO_ReadRegister
#define XSDIO_ReadRegister 0x79004
#undef SDIO_Op
#define SDIO_Op 0x59005
#undef XSDIO_Op
#define XSDIO_Op 0x79005
#undef SDIO_ClaimDeviceVector
#define SDIO_ClaimDeviceVector 0x59006
#undef XSDIO_ClaimDeviceVector
#define XSDIO_ClaimDeviceVector 0x79006
#undef SDIO_ReleaseDeviceVector
#define SDIO_ReleaseDeviceVector 0x59007
#undef XSDIO_ReleaseDeviceVector
#define XSDIO_ReleaseDeviceVector 0x79007
#undef SDIO_Status
#define SDIO_Status 0x59008
#undef XSDIO_Status
#define XSDIO_Status 0x79008
#undef SDFS_DiscOp
#define SDFS_DiscOp 0x59040
#undef XSDFS_DiscOp
#define XSDFS_DiscOp 0x79040
#undef SDFS_NOP01
#define SDFS_NOP01 0x59041
#undef XSDFS_NOP01
#define XSDFS_NOP01 0x79041
#undef SDFS_Drives
#define SDFS_Drives 0x59042
#undef XSDFS_Drives
#define XSDFS_Drives 0x79042
#undef SDFS_FreeSpace
#define SDFS_FreeSpace 0x59043
#undef XSDFS_FreeSpace
#define XSDFS_FreeSpace 0x79043
#undef SDFS_NOP04
#define SDFS_NOP04 0x59044
#undef XSDFS_NOP04
#define XSDFS_NOP04 0x79044
#undef SDFS_DescribeDisc
#define SDFS_DescribeDisc 0x59045
#undef XSDFS_DescribeDisc
#define XSDFS_DescribeDisc 0x79045
#undef SDFS_NOP06
#define SDFS_NOP06 0x59046
#undef XSDFS_NOP06
#define XSDFS_NOP06 0x79046
#undef SDFS_NOP07
#define SDFS_NOP07 0x59047
#undef XSDFS_NOP07
#define XSDFS_NOP07 0x79047
#undef SDFS_NOP08
#define SDFS_NOP08 0x59048
#undef XSDFS_NOP08
#define XSDFS_NOP08 0x79048
#undef SDFS_MiscOp
#define SDFS_MiscOp 0x59049
#undef XSDFS_MiscOp
#define XSDFS_MiscOp 0x79049
#undef SDFS_SectorDiscOp
#define SDFS_SectorDiscOp 0x5904A
#undef XSDFS_SectorDiscOp
#define XSDFS_SectorDiscOp 0x7904A
#undef SDFS_FreeSpace64
#define SDFS_FreeSpace64 0x5904B
#undef XSDFS_FreeSpace64
#define XSDFS_FreeSpace64 0x7904B
#undef SDFS_DiscOp64
#define SDFS_DiscOp64 0x5904C
#undef XSDFS_DiscOp64
#define XSDFS_DiscOp64 0x7904C
#undef SDFS_NOP13
#define SDFS_NOP13 0x5904D
#undef XSDFS_NOP13
#define XSDFS_NOP13 0x7904D
#undef SDFS_ReadCardInfo
#define SDFS_ReadCardInfo 0x59060
#undef XSDFS_ReadCardInfo
#define XSDFS_ReadCardInfo 0x79060
#undef BCMSupport_SendMBMessage
#define BCMSupport_SendMBMessage 0x591C0
#undef XBCMSupport_SendMBMessage
#define XBCMSupport_SendMBMessage 0x791C0
#undef BCMSupport_MBSync
#define BCMSupport_MBSync 0x591C1
#undef XBCMSupport_MBSync
#define XBCMSupport_MBSync 0x791C1
#undef BCMSupport_AllocPropertyBuffer
#define BCMSupport_AllocPropertyBuffer 0x591C2
#undef XBCMSupport_AllocPropertyBuffer
#define XBCMSupport_AllocPropertyBuffer 0x791C2
#undef BCMSupport_FreePropertyBuffer
#define BCMSupport_FreePropertyBuffer 0x591C3
#undef XBCMSupport_FreePropertyBuffer
#define XBCMSupport_FreePropertyBuffer 0x791C3
#undef BCMSupport_SendPropertyBuffer
#define BCMSupport_SendPropertyBuffer 0x591C4
#undef XBCMSupport_SendPropertyBuffer
#define XBCMSupport_SendPropertyBuffer 0x791C4
#undef BCMSupport_SendTempPropertyBuffer
#define BCMSupport_SendTempPropertyBuffer 0x591C5
#undef XBCMSupport_SendTempPropertyBuffer
#define XBCMSupport_SendTempPropertyBuffer 0x791C5
#undef VCHIQ_Initialise
#define VCHIQ_Initialise 0x59200
#undef XVCHIQ_Initialise
#define XVCHIQ_Initialise 0x79200
#undef VCHIQ_Connect
#define VCHIQ_Connect 0x59201
#undef XVCHIQ_Connect
#define XVCHIQ_Connect 0x79201
#undef VCHIQ_Disconnect
#define VCHIQ_Disconnect 0x59202
#undef XVCHIQ_Disconnect
#define XVCHIQ_Disconnect 0x79202
#undef VCHIQ_BulkQueueTransmit
#define VCHIQ_BulkQueueTransmit 0x59203
#undef XVCHIQ_BulkQueueTransmit
#define XVCHIQ_BulkQueueTransmit 0x79203
#undef VCHIQ_MsgDequeue
#define VCHIQ_MsgDequeue 0x59204
#undef XVCHIQ_MsgDequeue
#define XVCHIQ_MsgDequeue 0x79204
#undef VCHIQ_MsgQueue
#define VCHIQ_MsgQueue 0x59205
#undef XVCHIQ_MsgQueue
#define XVCHIQ_MsgQueue 0x79205
#undef VCHIQ_MsgQueueV
#define VCHIQ_MsgQueueV 0x59206
#undef XVCHIQ_MsgQueueV
#define XVCHIQ_MsgQueueV 0x79206
#undef VCHIQ_MsgPeek
#define VCHIQ_MsgPeek 0x59207
#undef XVCHIQ_MsgPeek
#define XVCHIQ_MsgPeek 0x79207
#undef VCHIQ_MsgRemove
#define VCHIQ_MsgRemove 0x59208
#undef XVCHIQ_MsgRemove
#define XVCHIQ_MsgRemove 0x79208
#undef VCHIQ_ServiceClose
#define VCHIQ_ServiceClose 0x59209
#undef XVCHIQ_ServiceClose
#define XVCHIQ_ServiceClose 0x79209
#undef VCHIQ_ServiceOpen
#define VCHIQ_ServiceOpen 0x5920A
#undef XVCHIQ_ServiceOpen
#define XVCHIQ_ServiceOpen 0x7920A
#undef VCHIQ_ServiceCreate
#define VCHIQ_ServiceCreate 0x5920B
#undef XVCHIQ_ServiceCreate
#define XVCHIQ_ServiceCreate 0x7920B
#undef VCHIQ_ServiceDestroy
#define VCHIQ_ServiceDestroy 0x5920C
#undef XVCHIQ_ServiceDestroy
#define XVCHIQ_ServiceDestroy 0x7920C
#undef VCHIQ_ServiceUse
#define VCHIQ_ServiceUse 0x5920D
#undef XVCHIQ_ServiceUse
#define XVCHIQ_ServiceUse 0x7920D
#undef VCHIQ_ServiceRelease
#define VCHIQ_ServiceRelease 0x5920E
#undef XVCHIQ_ServiceRelease
#define XVCHIQ_ServiceRelease 0x7920E
#undef VCHIQ_BulkQueueReceive
#define VCHIQ_BulkQueueReceive 0x5920F
#undef XVCHIQ_BulkQueueReceive
#define XVCHIQ_BulkQueueReceive 0x7920F
#undef RTC_Features
#define RTC_Features 0x594C0
#undef XRTC_Features
#define XRTC_Features 0x794C0
#undef RTC_Read
#define RTC_Read 0x594C1
#undef XRTC_Read
#define XRTC_Read 0x794C1
#undef RTC_Write
#define RTC_Write 0x594C2
#undef XRTC_Write
#define XRTC_Write 0x794C2
#undef RTC_Adjust
#define RTC_Adjust 0x594C3
#undef XRTC_Adjust
#define XRTC_Adjust 0x794C3
#undef NetMonitor_PrintChar
#define NetMonitor_PrintChar 0x80040
#undef XNetMonitor_PrintChar
#define XNetMonitor_PrintChar 0xA0040
#undef NetMonitor_DefineTask
#define NetMonitor_DefineTask 0x80041
#undef XNetMonitor_DefineTask
#define XNetMonitor_DefineTask 0xA0041
#undef NetMonitor_AbandonTask
#define NetMonitor_AbandonTask 0x80042
#undef XNetMonitor_AbandonTask
#define XNetMonitor_AbandonTask 0xA0042
#undef NetMonitor_ConvertFont
#define NetMonitor_ConvertFont 0x80043
#undef XNetMonitor_ConvertFont
#define XNetMonitor_ConvertFont 0xA0043
#undef NetMonitor_UseFont
#define NetMonitor_UseFont 0x80044
#undef XNetMonitor_UseFont
#define XNetMonitor_UseFont 0xA0044
#undef NetMonitor_RestoreFont
#define NetMonitor_RestoreFont 0x80045
#undef XNetMonitor_RestoreFont
#define XNetMonitor_RestoreFont 0xA0045
#undef NetMonitor_StartWithCurrentFont
#define NetMonitor_StartWithCurrentFont 0x80046
#undef XNetMonitor_StartWithCurrentFont
#define XNetMonitor_StartWithCurrentFont 0xA0046
#undef NetMonitor_StartWithInternalFont
#define NetMonitor_StartWithInternalFont 0x80047
#undef XNetMonitor_StartWithInternalFont
#define XNetMonitor_StartWithInternalFont 0xA0047
#undef PDriver_Info
#define PDriver_Info 0x80140
#undef XPDriver_Info
#define XPDriver_Info 0xA0140
#undef PDriver_SetInfo
#define PDriver_SetInfo 0x80141
#undef XPDriver_SetInfo
#define XPDriver_SetInfo 0xA0141
#undef PDriver_CheckFeatures
#define PDriver_CheckFeatures 0x80142
#undef XPDriver_CheckFeatures
#define XPDriver_CheckFeatures 0xA0142
#undef PDriver_PageSize
#define PDriver_PageSize 0x80143
#undef XPDriver_PageSize
#define XPDriver_PageSize 0xA0143
#undef PDriver_SetPageSize
#define PDriver_SetPageSize 0x80144
#undef XPDriver_SetPageSize
#define XPDriver_SetPageSize 0xA0144
#undef PDriver_SelectJob
#define PDriver_SelectJob 0x80145
#undef XPDriver_SelectJob
#define XPDriver_SelectJob 0xA0145
#undef PDriver_CurrentJob
#define PDriver_CurrentJob 0x80146
#undef XPDriver_CurrentJob
#define XPDriver_CurrentJob 0xA0146
#undef PDriver_FontSWI
#define PDriver_FontSWI 0x80147
#undef XPDriver_FontSWI
#define XPDriver_FontSWI 0xA0147
#undef PDriver_EndJob
#define PDriver_EndJob 0x80148
#undef XPDriver_EndJob
#define XPDriver_EndJob 0xA0148
#undef PDriver_AbortJob
#define PDriver_AbortJob 0x80149
#undef XPDriver_AbortJob
#define XPDriver_AbortJob 0xA0149
#undef PDriver_Reset
#define PDriver_Reset 0x8014A
#undef XPDriver_Reset
#define XPDriver_Reset 0xA014A
#undef PDriver_GiveRectangle
#define PDriver_GiveRectangle 0x8014B
#undef XPDriver_GiveRectangle
#define XPDriver_GiveRectangle 0xA014B
#undef PDriver_DrawPage
#define PDriver_DrawPage 0x8014C
#undef XPDriver_DrawPage
#define XPDriver_DrawPage 0xA014C
#undef PDriver_GetRectangle
#define PDriver_GetRectangle 0x8014D
#undef XPDriver_GetRectangle
#define XPDriver_GetRectangle 0xA014D
#undef PDriver_CancelJob
#define PDriver_CancelJob 0x8014E
#undef XPDriver_CancelJob
#define XPDriver_CancelJob 0xA014E
#undef PDriver_ScreenDump
#define PDriver_ScreenDump 0x8014F
#undef XPDriver_ScreenDump
#define XPDriver_ScreenDump 0xA014F
#undef PDriver_EnumerateJobs
#define PDriver_EnumerateJobs 0x80150
#undef XPDriver_EnumerateJobs
#define XPDriver_EnumerateJobs 0xA0150
#undef PDriver_SetPrinter
#define PDriver_SetPrinter 0x80151
#undef XPDriver_SetPrinter
#define XPDriver_SetPrinter 0xA0151
#undef PDriver_CancelJobWithError
#define PDriver_CancelJobWithError 0x80152
#undef XPDriver_CancelJobWithError
#define XPDriver_CancelJobWithError 0xA0152
#undef PDriver_SelectIllustration
#define PDriver_SelectIllustration 0x80153
#undef XPDriver_SelectIllustration
#define XPDriver_SelectIllustration 0xA0153
#undef PDriver_InsertIllustration
#define PDriver_InsertIllustration 0x80154
#undef XPDriver_InsertIllustration
#define XPDriver_InsertIllustration 0xA0154
#undef PDriver_DeclareFont
#define PDriver_DeclareFont 0x80155
#undef XPDriver_DeclareFont
#define XPDriver_DeclareFont 0xA0155
#undef PDriver_DeclareDriver
#define PDriver_DeclareDriver 0x80156
#undef XPDriver_DeclareDriver
#define XPDriver_DeclareDriver 0xA0156
#undef PDriver_RemoveDriver
#define PDriver_RemoveDriver 0x80157
#undef XPDriver_RemoveDriver
#define XPDriver_RemoveDriver 0xA0157
#undef PDriver_SelectDriver
#define PDriver_SelectDriver 0x80158
#undef XPDriver_SelectDriver
#define XPDriver_SelectDriver 0xA0158
#undef PDriver_EnumerateDrivers
#define PDriver_EnumerateDrivers 0x80159
#undef XPDriver_EnumerateDrivers
#define XPDriver_EnumerateDrivers 0xA0159
#undef PDriver_MiscOp
#define PDriver_MiscOp 0x8015A
#undef XPDriver_MiscOp
#define XPDriver_MiscOp 0xA015A
#undef PDriver_MiscOpForDriver
#define PDriver_MiscOpForDriver 0x8015B
#undef XPDriver_MiscOpForDriver
#define XPDriver_MiscOpForDriver 0xA015B
#undef PDriver_SetDriver
#define PDriver_SetDriver 0x8015C
#undef XPDriver_SetDriver
#define XPDriver_SetDriver 0xA015C
#undef PDriver_JPEGSWI
#define PDriver_JPEGSWI 0x8015D
#undef XPDriver_JPEGSWI
#define XPDriver_JPEGSWI 0xA015D
#undef SharedCLibrary_LibInitAPCS_A
#define SharedCLibrary_LibInitAPCS_A 0x80680
#undef XSharedCLibrary_LibInitAPCS_A
#define XSharedCLibrary_LibInitAPCS_A 0xA0680
#undef SharedCLibrary_LibInitAPCS_R
#define SharedCLibrary_LibInitAPCS_R 0x80681
#undef XSharedCLibrary_LibInitAPCS_R
#define XSharedCLibrary_LibInitAPCS_R 0xA0681
#undef SharedCLibrary_LibInitModule
#define SharedCLibrary_LibInitModule 0x80682
#undef XSharedCLibrary_LibInitModule
#define XSharedCLibrary_LibInitModule 0xA0682
#undef SharedCLibrary_LibInitAPCS_32
#define SharedCLibrary_LibInitAPCS_32 0x80683
#undef XSharedCLibrary_LibInitAPCS_32
#define XSharedCLibrary_LibInitAPCS_32 0xA0683
#undef SharedCLibrary_LibInitModuleAPCS_32
#define SharedCLibrary_LibInitModuleAPCS_32 0x80684
#undef XSharedCLibrary_LibInitModuleAPCS_32
#define XSharedCLibrary_LibInitModuleAPCS_32 0xA0684
#undef Profiler_FindRoutines
#define Profiler_FindRoutines 0x81F00
#undef XProfiler_FindRoutines
#define XProfiler_FindRoutines 0xA1F00
#undef Profiler_ReadTime
#define Profiler_ReadTime 0x81F01
#undef XProfiler_ReadTime
#define XProfiler_ReadTime 0xA1F01
#undef Profiler_AddCounter
#define Profiler_AddCounter 0x81F02
#undef XProfiler_AddCounter
#define XProfiler_AddCounter 0xA1F02
#undef Profiler_SetIndex
#define Profiler_SetIndex 0x81F03
#undef XProfiler_SetIndex
#define XProfiler_SetIndex 0xA1F03
#undef Window_ClassSWI
#define Window_ClassSWI 0x82880
#undef XWindow_ClassSWI
#define XWindow_ClassSWI 0xA2880
#undef Window_PostFilter
#define Window_PostFilter 0x82881
#undef XWindow_PostFilter
#define XWindow_PostFilter 0xA2881
#undef Window_PreFilter
#define Window_PreFilter 0x82882
#undef XWindow_PreFilter
#define XWindow_PreFilter 0xA2882
#undef Window_GetPointerInfo
#define Window_GetPointerInfo 0x82883
#undef XWindow_GetPointerInfo
#define XWindow_GetPointerInfo 0xA2883
#undef Window_WimpToToolbox
#define Window_WimpToToolbox 0x82884
#undef XWindow_WimpToToolbox
#define XWindow_WimpToToolbox 0xA2884
#undef Window_RegisterExternal
#define Window_RegisterExternal 0x82885
#undef XWindow_RegisterExternal
#define XWindow_RegisterExternal 0xA2885
#undef Window_DeregisterExternal
#define Window_DeregisterExternal 0x82886
#undef XWindow_DeregisterExternal
#define XWindow_DeregisterExternal 0xA2886
#undef Window_SupportExternal
#define Window_SupportExternal 0x82887
#undef XWindow_SupportExternal
#define XWindow_SupportExternal 0xA2887
#undef Window_RegisterFilter
#define Window_RegisterFilter 0x82888
#undef XWindow_RegisterFilter
#define XWindow_RegisterFilter 0xA2888
#undef Window_DeregisterFilter
#define Window_DeregisterFilter 0x82889
#undef XWindow_DeregisterFilter
#define XWindow_DeregisterFilter 0xA2889
#undef Window_EnumerateGadgets
#define Window_EnumerateGadgets 0x8288A
#undef XWindow_EnumerateGadgets
#define XWindow_EnumerateGadgets 0xA288A
#undef Window_GadgetGetIconList
#define Window_GadgetGetIconList 0x8288B
#undef XWindow_GadgetGetIconList
#define XWindow_GadgetGetIconList 0xA288B
#undef Window_InternalOp
#define Window_InternalOp 0x828A0
#undef XWindow_InternalOp
#define XWindow_InternalOp 0xA28A0
#undef Window_PreSubMenuShow
#define Window_PreSubMenuShow 0x828BD
#undef XWindow_PreSubMenuShow
#define XWindow_PreSubMenuShow 0xA28BD
#undef Window_ExtractGadgetInfo
#define Window_ExtractGadgetInfo 0x828BE
#undef XWindow_ExtractGadgetInfo
#define XWindow_ExtractGadgetInfo 0xA28BE
#undef Window_PlotGadget
#define Window_PlotGadget 0x828BF
#undef XWindow_PlotGadget
#define XWindow_PlotGadget 0xA28BF

#endif
