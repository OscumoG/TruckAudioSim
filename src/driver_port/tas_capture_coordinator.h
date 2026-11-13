// SPDX-License-Identifier: MIT
// PASSIVE_LEVEL coordinator for an already-mixed WaveRT render buffer.
// Portable code; not a driver. All operations require the same external lock
// as the reader session, stop/reset and device teardown.
#ifndef TAS_CAPTURE_COORDINATOR_H
#define TAS_CAPTURE_COORDINATOR_H
#include "tas_stream_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif

enum tas_capture_result {
 TAS_CAPTURE_OK = 0,
 TAS_CAPTURE_IDLE = 1,
 TAS_CAPTURE_NO_READER = 2,
 TAS_CAPTURE_RESET = 3,
 TAS_CAPTURE_INVALID = 4,
 TAS_CAPTURE_BAD_PCM = 5
};

typedef struct tas_capture_coordinator {
 tas_dma_tap tap;
 tas_device_session* session; // owned by host, lifetime >= coordinator
 uint64_t observed_generation;
 uint64_t packets_emitted;
 uint64_t frames_overwritten;
 uint64_t reset_count;
 uint8_t running;
} tas_capture_coordinator;

// One coordinator per render stream/PCM format; requires initialized session
// with matching format and source token. Starts STOPPED.
int tas_capture_init(tas_capture_coordinator* c, tas_device_session* session,
                     uint16_t channels, uint32_t mask, uint32_t token);
// On STREAM RUN transition. Clears queued PCM and synchronizes to the
// *current completed* frame, so historical sound is not replayed.
int tas_capture_start(tas_capture_coordinator* c, uint64_t completed_frames);
// On STOP/PAUSE, format change, disconnect, or unload. Scrubs all queued PCM.
void tas_capture_stop(tas_capture_coordinator* c);
// Call only from a PASSIVE_LEVEL worker with stable, coherent DMA snapshot.
// The caller owns staging (>= one maximum packet) from nonpaged storage.
// max_packets bounds work while the session lock is held (1..8).
// packets_written and frames_skipped initialized to zero even on failure.
int tas_capture_poll(tas_capture_coordinator* c,
                     const void* dma, size_t dma_bytes, uint64_t completed_frames,
                     void* staging, size_t staging_bytes, unsigned max_packets,
                     unsigned* packets_written, uint64_t* frames_skipped);
#ifdef __cplusplus
}
#endif
#endif
