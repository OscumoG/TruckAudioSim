// SPDX-License-Identifier: MIT
// WDM single-reader dispatch helpers; NOT wired to DriverEntry/AddDevice.
#include "tas_wdm_session_adapter.h"
#ifdef _KERNEL_MODE
namespace {
NTSTATUS Complete(PIRP irp,NTSTATUS status,ULONG_PTR bytes=0){
 irp->IoStatus.Status=status;
 irp->IoStatus.Information=NT_SUCCESS(status)?bytes:0;
 IoCompleteRequest(irp,IO_NO_INCREMENT);
 return status;
}
NTSTATUS Check(PIRP irp,tas_device_session* session,PFAST_MUTEX lock,
               PIO_STACK_LOCATION* stack){
 if(!irp||!session||!lock)return STATUS_INVALID_PARAMETER;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
 *stack=IoGetCurrentIrpStackLocation(irp);
 if(!*stack || !(*stack)->FileObject)return STATUS_INVALID_PARAMETER;
 return STATUS_SUCCESS;
}
NTSTATUS Lifecycle(PIRP irp,tas_device_session* session,PFAST_MUTEX lock,bool opening){
 PIO_STACK_LOCATION st=nullptr;
 NTSTATUS s=Check(irp,session,lock,&st);
 if(!NT_SUCCESS(s))return irp?Complete(irp,s):s;
 const uintptr_t key=(uintptr_t)st->FileObject;
 ExAcquireFastMutex(lock);
 const int r=opening?tas_session_open(session,key):tas_session_close(session,key);
 ExReleaseFastMutex(lock);
 if(r==TAS_SESSION_OK)s=STATUS_SUCCESS;
 else if(r==TAS_SESSION_BUSY)s=STATUS_SHARING_VIOLATION;
 else if(r==TAS_SESSION_DENIED)s=STATUS_ACCESS_DENIED;
 else s=STATUS_INVALID_DEVICE_STATE;
 return Complete(irp,s);
}
}
NTSTATUS TasControlCreate(PIRP irp,tas_device_session* s,PFAST_MUTEX l){
 return Lifecycle(irp,s,l,true);
}
NTSTATUS TasControlCleanup(PIRP irp,tas_device_session* s,PFAST_MUTEX l){
 return Lifecycle(irp,s,l,false);
}
NTSTATUS TasControlClose(PIRP irp,tas_device_session* session,PFAST_MUTEX lock){
 // IRP_MJ_CLOSE may arrive after CLEANUP while a NEW reader has opened.
 // The stale FILE_OBJECT must not revoke that new reader's lease.
 PIO_STACK_LOCATION st=nullptr;
 NTSTATUS status=Check(irp,session,lock,&st);
 if(!NT_SUCCESS(status))return irp?Complete(irp,status):status;
 ExAcquireFastMutex(lock);
 const int r=tas_session_close(session,(uintptr_t)st->FileObject);
 ExReleaseFastMutex(lock);
 // A stale close does not own the current lease: acknowledge without changes.
 status=(r==TAS_SESSION_OK || r==TAS_SESSION_DENIED)?
    STATUS_SUCCESS:STATUS_INVALID_DEVICE_STATE;
 return Complete(irp,status);
}
NTSTATUS TasControlIoctl(PIRP irp,tas_device_session* session,PFAST_MUTEX lock){
 PIO_STACK_LOCATION st=nullptr;
 NTSTATUS s=Check(irp,session,lock,&st);
 if(!NT_SUCCESS(s))return irp?Complete(irp,s):s;
 size_t bytes=0;
 ExAcquireFastMutex(lock);
 const int r=tas_session_read(session,(uintptr_t)st->FileObject,
      st->Parameters.DeviceIoControl.IoControlCode,
      st->Parameters.DeviceIoControl.InputBufferLength,
      irp->AssociatedIrp.SystemBuffer,
      st->Parameters.DeviceIoControl.OutputBufferLength,&bytes);
 ExReleaseFastMutex(lock);
 switch(r){
  case TAS_SESSION_OK:s=STATUS_SUCCESS;break;
  case TAS_SESSION_EMPTY:s=STATUS_NO_MORE_ENTRIES;break;
  case TAS_SESSION_DENIED:s=STATUS_ACCESS_DENIED;break;
  case TAS_SESSION_BUFFER_SMALL:s=STATUS_BUFFER_TOO_SMALL;break;
  case TAS_SESSION_BAD_CONTROL:s=STATUS_INVALID_DEVICE_REQUEST;break;
  case TAS_SESSION_BAD_ARGUMENT:s=STATUS_INVALID_PARAMETER;break;
  default:s=STATUS_INTERNAL_ERROR;break;
 }
 return Complete(irp,s,(ULONG_PTR)bytes);
}
#endif
