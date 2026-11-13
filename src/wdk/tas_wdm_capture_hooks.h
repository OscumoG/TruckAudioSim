// SPDX-License-Identifier: MIT
// Experimental WDK-only adapter. NOT installed or wired into SysVAD.
// It must run at PASSIVE_LEVEL with a coherent, completed PCM snapshot.
#pragma once
#ifdef _KERNEL_MODE
#include <ntddk.h>
extern "C" {
#include "../driver_port/tas_capture_coordinator.h"
}
struct TAS_WDM_CAPTURE_CONTEXT {
    // Owner allocates this and the packet staging in nonpaged storage.
    // Must share this lock with TasControlIoctl/Create/Cleanup/Close.
    FAST_MUTEX* SessionLock;
    tas_capture_coordinator Coordinator;
    tas_device_session* Session;
    void* Staging;
    size_t StagingBytes;
};
// Call after initialized tas_device_session and nonpaged allocations.
NTSTATUS TasWdmCaptureInitialize(TAS_WDM_CAPTURE_CONTEXT* ctx,
    FAST_MUTEX* lock, tas_device_session* session,
    void* staging, size_t stagingBytes,
    USHORT channels, ULONG channelMask, ULONG sourceToken);
// Called at PASSIVE_LEVEL on render state RUN/STOP/PAUSE changes.
NTSTATUS TasWdmCaptureStart(TAS_WDM_CAPTURE_CONTEXT* ctx, ULONGLONG completedFrames);
void TasWdmCaptureStop(TAS_WDM_CAPTURE_CONTEXT* ctx);
// IMPORTANT: dmaSnapshot must already be coherent and remain valid for the
// entire call; raw DMA memory still modified by the OS is NOT a safe snapshot.
NTSTATUS TasWdmCapturePassiveTick(TAS_WDM_CAPTURE_CONTEXT* ctx,
    const void* dmaSnapshot, size_t snapshotBytes,
    ULONGLONG completedFrames, ULONG* packets, ULONGLONG* lostFrames);
#endif
