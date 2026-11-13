// SPDX-License-Identifier: MIT
// Portable single-reader lifecycle for a future WDM/PortCls control device.
// Does NOT create an audio endpoint, driver or Windows device object.
#ifndef TAS_DEVICE_SESSION_H
#define TAS_DEVICE_SESSION_H
#include "tas_packet_fifo.h"
#include "tas_ioctl_contract.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum tas_session_result {
 TAS_SESSION_OK=0,
 TAS_SESSION_EMPTY=1,
 TAS_SESSION_BUSY=2,
 TAS_SESSION_DENIED=3,
 TAS_SESSION_BAD_ARGUMENT=4,
 TAS_SESSION_BUFFER_SMALL=5,
 TAS_SESSION_BAD_CONTROL=6,
 TAS_SESSION_BROKEN=7
};

typedef struct tas_device_session {
 tas_packet_fifo* fifo;
 uintptr_t reader_key; // Control FILE_OBJECT identity; NOT an authentication credential.
 uint64_t generation;
 uint8_t reader_active;
} tas_device_session;

// ALL entry points require the SAME external lock on the producer and consumer.
// Host must enforce a restrictive device security descriptor and handle access.
int tas_session_init(tas_device_session* session, tas_packet_fifo* queue);
// Fail closed: only one reader at a time. Flush prior PCM before admitting reader.
int tas_session_open(tas_device_session* session, uintptr_t file_key);
// Idempotent release allows IRP_MJ_CLEANUP followed by IRP_MJ_CLOSE.
// A wrong file identity cannot close another reader's session.
int tas_session_close(tas_device_session* session, uintptr_t file_key);
// Clear on stop/unload: caller must first stop producers, then hold queue lock.
void tas_session_shutdown(tas_device_session* session);
// Producer may enqueue only during an active session (do not retain audio at rest).
int tas_session_push(tas_device_session* session, const void* packet, size_t length);
// METHOD_BUFFERED compatible. On failures, bytes_returned is 0.
int tas_session_read(tas_device_session* session, uintptr_t file_key,
                     uint32_t ioctl, size_t input_length,
                     void* system_output, size_t output_capacity,
                     size_t* bytes_returned);
#ifdef __cplusplus
}
#endif
#endif
