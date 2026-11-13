// SPDX-License-Identifier: MIT
// Portable ready-buffer -> bounded TAS1 queue integration.
#ifndef TAS_STREAM_ADAPTER_H
#define TAS_STREAM_ADAPTER_H
#include "wavert_dma_tap.h"
#include "tas_packet_fifo.h"
#include "tas_device_session.h"
#ifdef __cplusplus
extern "C" {
#endif

enum tas_adapter_result {
 TAS_ADAPTER_OK=0,
 TAS_ADAPTER_EMPTY=1,
 TAS_ADAPTER_INVALID=2,
 TAS_ADAPTER_CORRUPT_PCM=3,
 TAS_ADAPTER_INACTIVE=4
};
// All storage is caller-owned, nonpaged in a WDK integration.
// DMA snapshot lifetime and synchronization are a caller responsibility.
// Do NOT invoke while holding a spinlock or at a high IRQL: this function
// scans/copies up to 20 ms of 8-channel audio and may take too long.
int tas_stream_adapter_poll(tas_dma_tap* tap,tas_packet_fifo* fifo,
                            const void* dma,size_t dma_bytes,uint64_t completed_frames,
                            void* staging,size_t staging_size,
                            uint64_t* overwritten_frames);
// WDM producer entry point: never enqueue/retain PCM without a reader.
// Call only under the same external lock as the session IOCTL/cleanup.
int tas_stream_adapter_poll_session(tas_dma_tap* tap,tas_device_session* session,
                            const void* dma,size_t dma_bytes,uint64_t completed_frames,
                            void* staging,size_t staging_size,
                            uint64_t* overwritten_frames);
#ifdef __cplusplus
}
#endif
#endif
