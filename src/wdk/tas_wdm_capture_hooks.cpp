// SPDX-License-Identifier: MIT
// Glue for the future SysVAD render stream owner. No DriverEntry or IOCTL here.
#include "tas_wdm_capture_hooks.h"
#ifdef _KERNEL_MODE
namespace {
 bool IsPassive() { return KeGetCurrentIrql() == PASSIVE_LEVEL; }
 NTSTATUS TranslateResult(int result) {
  switch(result) {
   case TAS_CAPTURE_OK: case TAS_CAPTURE_IDLE:return STATUS_SUCCESS;
   case TAS_CAPTURE_NO_READER: return STATUS_NO_MORE_ENTRIES;
   case TAS_CAPTURE_RESET:return STATUS_SUCCESS; // normal stream/session resynchronization
   case TAS_CAPTURE_BAD_PCM:return STATUS_DATA_ERROR;
   default:return STATUS_INVALID_PARAMETER;
  }
 }
}
NTSTATUS TasWdmCaptureInitialize(TAS_WDM_CAPTURE_CONTEXT* c,
    FAST_MUTEX* lock,tas_device_session* s,
    void* staging,size_t stagingBytes,USHORT channels,ULONG mask,ULONG token){
 if(!IsPassive())return STATUS_INVALID_DEVICE_STATE;
 if(!c||!lock||!s||!staging||
    stagingBytes<TAS_FIFO_HEADER_BYTES+(size_t)TAS_FIFO_MAX_FRAMES*channels*4u)
  return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(c,sizeof(*c));
 c->SessionLock=lock;c->Session=s;
 c->Staging=staging;c->StagingBytes=stagingBytes;
 ExAcquireFastMutex(lock);
 const int r=tas_capture_init(&c->Coordinator,s,channels,mask,token);
 ExReleaseFastMutex(lock);
 return r==TAS_CAPTURE_OK?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
}
NTSTATUS TasWdmCaptureStart(TAS_WDM_CAPTURE_CONTEXT* c,ULONGLONG completed){
 if(!IsPassive())return STATUS_INVALID_DEVICE_STATE;
 if(!c||!c->SessionLock)return STATUS_INVALID_PARAMETER;
 ExAcquireFastMutex(c->SessionLock);
 const int r=tas_capture_start(&c->Coordinator,completed);
 ExReleaseFastMutex(c->SessionLock);
 return TranslateResult(r);
}
void TasWdmCaptureStop(TAS_WDM_CAPTURE_CONTEXT* c){
 if(!IsPassive()||!c||!c->SessionLock)return;
 ExAcquireFastMutex(c->SessionLock);
 tas_capture_stop(&c->Coordinator);
 ExReleaseFastMutex(c->SessionLock);
}
NTSTATUS TasWdmCapturePassiveTick(TAS_WDM_CAPTURE_CONTEXT* c,
    const void* snapshot,size_t bytes,ULONGLONG completed,
    ULONG* packets,ULONGLONG* lost){
 if(packets)*packets=0;
 if(lost)*lost=0;
 if(!IsPassive())return STATUS_INVALID_DEVICE_STATE;
 if(!c||!c->SessionLock||!snapshot||!packets||!lost)return STATUS_INVALID_PARAMETER;
 unsigned n=0;uint64_t skipped=0;
 ExAcquireFastMutex(c->SessionLock);
 const int r=tas_capture_poll(&c->Coordinator,snapshot,bytes,completed,
       c->Staging,c->StagingBytes,4,&n,&skipped);
 ExReleaseFastMutex(c->SessionLock);
 *packets=(ULONG)n;*lost=(ULONGLONG)skipped;
 return TranslateResult(r);
}
#endif
