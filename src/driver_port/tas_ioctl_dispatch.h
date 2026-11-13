// SPDX-License-Identifier: MIT
// Portable implementation of the TAS1 METHOD_BUFFERED IOCTL contract.
// This code does not create a device and is NOT a Windows kernel driver.
#ifndef TAS_IOCTL_DISPATCH_H
#define TAS_IOCTL_DISPATCH_H
#include "tas_packet_fifo.h"
#include "tas_ioctl_contract.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum tas_ioctl_status {
 TAS_IOCTL_SUCCESS=0,
 TAS_IOCTL_EMPTY=1,
 TAS_IOCTL_BAD_CONTROL_CODE=2,
 TAS_IOCTL_INVALID_REQUEST=3,
 TAS_IOCTL_BUFFER_TOO_SMALL=4,
 TAS_IOCTL_BAD_QUEUE=5
};

// Producer and all consumers MUST hold the same external lock. Only one
// authorized reader is supported. The OS must check the handle's access rights
// and device ACL before invoking this function. No user pointer is retained.
// METHOD_BUFFERED: input_length MUST be 0; output_capacity is the actual
// system-buffer capacity, never a claimed user length. On BUFFER_TOO_SMALL,
// the packet remains queued; on EMPTY, no audio bytes are exposed.
int tas_ioctl_dispatch(tas_packet_fifo* fifo, uint32_t control_code,
                       size_t input_length, void* system_output,
                       size_t output_capacity, size_t* bytes_returned);
#ifdef __cplusplus
}
#endif
#endif
