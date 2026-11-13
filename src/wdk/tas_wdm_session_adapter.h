// SPDX-License-Identifier: MIT
// WDK integration template. Requires a SEPARATE ACL-protected control DO.
// Not a complete SysVAD driver, no WDK build included.
#pragma once
#ifdef _KERNEL_MODE
#include <ntddk.h>
extern "C" {
#include "../driver_port/tas_device_session.h"
}
// Caller must route only requests for TruckAudioSim's control device here.
// All producer accesses MUST acquire the same FAST_MUTEX at PASSIVE_LEVEL.
NTSTATUS TasControlCreate(PIRP irp,tas_device_session* session,PFAST_MUTEX lock);
NTSTATUS TasControlCleanup(PIRP irp,tas_device_session* session,PFAST_MUTEX lock);
NTSTATUS TasControlClose(PIRP irp,tas_device_session* session,PFAST_MUTEX lock);
NTSTATUS TasControlIoctl(PIRP irp,tas_device_session* session,PFAST_MUTEX lock);
#endif
