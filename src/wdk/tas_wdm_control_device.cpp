// SPDX-License-Identifier: MIT
// WDK-only control-device module for future SysVAD integration.
// NOT a standalone driver and NOT a working WaveRT playback miniport.
// Requires WDK /kernel compilation and real Windows runtime testing.
#include "tas_wdm_control_device.h"
#ifdef _KERNEL_MODE
#include <wdmsec.h> // IoCreateDeviceSecure
#include "tas_wdm_session_adapter.h"
#include "tas_wdm_capture_hooks.h"
extern "C" {
#include "../driver_port/tas_packet_fifo.h"
#include "../driver_port/tas_device_session.h"
}

#pragma comment(lib, "Wdmsec.lib")
namespace {
constexpr ULONG kPoolTag = 'CsAT';
constexpr ULONG kSlots = 8;
// This UUID is used for IoCreateDeviceSecure's default security class.
const GUID kControlClass = {0x6799be84, 0x7987, 0x4fe2, {0xa9,0x37,0x76,0x09,0x6a,0xec,0x14,0xef}};
struct ControlState {
    PDRIVER_OBJECT driver;
    PDEVICE_OBJECT device;
    PDRIVER_DISPATCH previousCreate;
    PDRIVER_DISPATCH previousClose;
    PDRIVER_DISPATCH previousCleanup;
    PDRIVER_DISPATCH previousIoctl;
    PDRIVER_UNLOAD previousUnload;
    FAST_MUTEX lock;
    tas_packet_fifo fifo;
    tas_device_session session;
    TAS_WDM_CAPTURE_CONTEXT capture;
    void* storage;
    size_t* lengths;
    void* staging;
    bool linkCreated;
    bool installed;
};
ControlState g{};
UNICODE_STRING gName = RTL_CONSTANT_STRING(L"\\Device\\TruckAudioSimPcm");
UNICODE_STRING gLink = RTL_CONSTANT_STRING(L"\\DosDevices\\TruckAudioSimPcm");

NTSTATUS Complete(PIRP irp, NTSTATUS status) {
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
NTSTATUS Forward(PDRIVER_DISPATCH saved, PDEVICE_OBJECT device, PIRP irp) {
    return saved ? saved(device, irp) : Complete(irp, STATUS_INVALID_DEVICE_REQUEST);
}
NTSTATUS DispatchCreate(PDEVICE_OBJECT device, PIRP irp) {
    return device == g.device ? TasControlCreate(irp,&g.session,&g.lock)
                              : Forward(g.previousCreate,device,irp);
}
NTSTATUS DispatchClose(PDEVICE_OBJECT device, PIRP irp) {
    return device == g.device ? TasControlClose(irp,&g.session,&g.lock)
                              : Forward(g.previousClose,device,irp);
}
NTSTATUS DispatchCleanup(PDEVICE_OBJECT device, PIRP irp) {
    return device == g.device ? TasControlCleanup(irp,&g.session,&g.lock)
                              : Forward(g.previousCleanup,device,irp);
}
NTSTATUS DispatchIoctl(PDEVICE_OBJECT device, PIRP irp) {
    return device == g.device ? TasControlIoctl(irp,&g.session,&g.lock)
                              : Forward(g.previousIoctl,device,irp);
}
void ReleaseStorage() {
    if(g.staging){ExFreePoolWithTag(g.staging,kPoolTag);g.staging=nullptr;}
    if(g.lengths){ExFreePoolWithTag(g.lengths,kPoolTag);g.lengths=nullptr;}
    if(g.storage){ExFreePoolWithTag(g.storage,kPoolTag);g.storage=nullptr;}
}
void UnloadWrapper(PDRIVER_OBJECT driver) {
    const auto previous=g.previousUnload;
    TasWdmControlRemove(driver);
    if(previous)previous(driver);
}
} // namespace

NTSTATUS TasWdmControlInstall(PDRIVER_OBJECT driver,USHORT channels,
                             ULONG mask,ULONG token) {
    if(!driver || KeGetCurrentIrql()!=PASSIVE_LEVEL || g.installed || g.storage)
        return STATUS_INVALID_DEVICE_STATE;
    // tas_packet_fifo_init validates the (channels, mask) combination.
    if(channels<2 || channels>8)return STATUS_INVALID_PARAMETER;
    ExInitializeFastMutex(&g.lock);
    const SIZE_T stride=TAS_FIFO_HEADER_BYTES+(SIZE_T)TAS_FIFO_MAX_FRAMES*channels*4;
    const SIZE_T bytes=stride*kSlots;
    g.storage=ExAllocatePool2(POOL_FLAG_NON_PAGED,bytes,kPoolTag);
    g.lengths=(size_t*)ExAllocatePool2(POOL_FLAG_NON_PAGED,kSlots*sizeof(size_t),kPoolTag);
    g.staging=ExAllocatePool2(POOL_FLAG_NON_PAGED,stride,kPoolTag);
    if(!g.storage || !g.lengths || !g.staging){ReleaseStorage();return STATUS_INSUFFICIENT_RESOURCES;}
    if(tas_packet_fifo_init(&g.fifo,g.storage,bytes,stride,g.lengths,kSlots,channels,mask,token)!=TAS_FIFO_OK ||
       tas_session_init(&g.session,&g.fifo)!=TAS_SESSION_OK){
        ReleaseStorage();return STATUS_INVALID_PARAMETER;
    }
    NTSTATUS status=TasWdmCaptureInitialize(&g.capture,&g.lock,&g.session,
                                             g.staging,stride,channels,mask,token);
    if(!NT_SUCCESS(status)){ReleaseStorage();return status;}
    // A separate named device object with a restrictive SDDL; ordinary user
    // processes cannot read another application's audio merely by opening it.
    UNICODE_STRING sddl=RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    status=IoCreateDeviceSecure(driver,0,&gName,FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,FALSE,&sddl,&kControlClass,&g.device);
    if(!NT_SUCCESS(status)){ReleaseStorage();return status;}
    g.device->Flags |= DO_BUFFERED_IO;
    status=IoCreateSymbolicLink(&gLink,&gName);
    if(!NT_SUCCESS(status)){
        IoDeleteDevice(g.device);g.device=nullptr;ReleaseStorage();return status;
    }
    g.linkCreated=true;
    g.driver=driver;
    g.previousCreate=driver->MajorFunction[IRP_MJ_CREATE];
    g.previousCleanup=driver->MajorFunction[IRP_MJ_CLEANUP];
    g.previousClose=driver->MajorFunction[IRP_MJ_CLOSE];
    g.previousIoctl=driver->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    g.previousUnload=driver->DriverUnload;
    driver->MajorFunction[IRP_MJ_CREATE]=DispatchCreate;
    driver->MajorFunction[IRP_MJ_CLEANUP]=DispatchCleanup;
    driver->MajorFunction[IRP_MJ_CLOSE]=DispatchClose;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL]=DispatchIoctl;
    driver->DriverUnload=UnloadWrapper;
    g.installed=true;
    g.device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}

void TasWdmControlRemove(PDRIVER_OBJECT driver) {
    // Only call after all stream callbacks and user-mode handles have drained.
    // Never call during another IRP dispatch or at DPC level.
    if(!g.installed || !driver || driver!=g.driver || KeGetCurrentIrql()!=PASSIVE_LEVEL)return;
    driver->MajorFunction[IRP_MJ_CREATE]=g.previousCreate;
    driver->MajorFunction[IRP_MJ_CLEANUP]=g.previousCleanup;
    driver->MajorFunction[IRP_MJ_CLOSE]=g.previousClose;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL]=g.previousIoctl;
    driver->DriverUnload=g.previousUnload;
    TasWdmCaptureStop(&g.capture);
    ExAcquireFastMutex(&g.lock);
    tas_session_shutdown(&g.session);
    ExReleaseFastMutex(&g.lock);
    if(g.linkCreated){IoDeleteSymbolicLink(&gLink);g.linkCreated=false;}
    if(g.device){IoDeleteDevice(g.device);g.device=nullptr;}
    ReleaseStorage();
    g.installed=false;g.driver=nullptr;
}
NTSTATUS TasWdmControlStreamStart(ULONGLONG completedFrames) {
    return g.installed?TasWdmCaptureStart(&g.capture,completedFrames):STATUS_DEVICE_NOT_READY;
}
void TasWdmControlStreamStop() {
    if(g.installed)TasWdmCaptureStop(&g.capture);
}
NTSTATUS TasWdmControlPassiveTick(const void* stableRing,SIZE_T bytes,
                                 ULONGLONG completedFrames,
                                 PULONG packets,PULONGLONG overwritten) {
    if(!g.installed)return STATUS_DEVICE_NOT_READY;
    return TasWdmCapturePassiveTick(&g.capture,stableRing,bytes,
                                    completedFrames,packets,overwritten);
}
#endif
