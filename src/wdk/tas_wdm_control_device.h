// SPDX-License-Identifier: MIT
// EXPERIMENTAL WDM/PortCls integration boundary. WDK and runtime verification required.
// This creates a private PCM CONTROL device, not a Windows playback endpoint.
#pragma once
#ifdef _KERNEL_MODE
#include <ntddk.h>

// Install only AFTER PcInitializeAdapterDriver, when PortCls has registered its
// major-function handlers. One instance per driver, one render stream supported.
// For security the link is only accessible to LocalSystem/Administrators.
NTSTATUS TasWdmControlInstall(PDRIVER_OBJECT driver, USHORT channels,
                              ULONG channelMask, ULONG sourceToken);
// For unload ONLY: stop all streams and drain existing IRPs first.
void TasWdmControlRemove(PDRIVER_OBJECT driver);
// Hooked to a SysVAD stream only after a stable, completed PCM ring snapshot
// is available. Invoke these at PASSIVE_LEVEL (not from timer/DPC).
NTSTATUS TasWdmControlStreamStart(ULONGLONG completedFrames);
void TasWdmControlStreamStop();
NTSTATUS TasWdmControlPassiveTick(const void* coherentRingSnapshot,
                                 SIZE_T ringBytes, ULONGLONG completedFrames,
                                 PULONG packetsWritten, PULONGLONG overwrittenFrames);
#endif
