// SPDX-License-Identifier: MIT
// Portable C adapter for a completed, already-mixed WaveRT render DMA ring.
// No allocation, exceptions, OS calls, or floating-point DSP; suitable for
// adaptation to a WDK driver after synchronization and lifecycle integration.
// NOT an installed audio driver. Caller guarantees DMA memory stays valid,
// is read-only during extraction, and completed_frames is a stable snapshot.
#ifndef TAS_WAVERT_DMA_TAP_H
#define TAS_WAVERT_DMA_TAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum tas_dma_status {
 TAS_DMA_OK=0,
 TAS_DMA_EMPTY=1,
 TAS_DMA_BAD_ARGUMENT=2,
 TAS_DMA_BAD_FORMAT=3,
 TAS_DMA_OUTPUT_SMALL=4,
 TAS_DMA_COUNTER_REWOUND=5,
 TAS_DMA_NONFINITE_SAMPLE=6
};

// The PCM stream is 48 kHz 32-bit IEEE float interleaved, little-endian.
// Channel masks: 2.0 0x3, 2.1 0xB (experimental), 5.1 back 0x3F,
// 5.1 side 0x60F, 7.1 0x63F. 2.1 is not a guaranteed Windows endpoint.
typedef struct tas_dma_tap {
 uint64_t next_frame;
 uint64_t lost_frames;
 uint32_t channel_mask;
 uint32_t source_token;
 uint16_t channels;
 uint16_t initialized;
} tas_dma_tap;

// Initialize at first render packet or after discontinuity / stream restart.
int tas_dma_tap_init(tas_dma_tap* state, uint16_t channels,
                     uint32_t channel_mask, uint32_t source_token,
                     uint64_t first_frame);

// dma is a circular buffer of completed 48k float32 render data. If this
// snapshot is > capacity_frames behind the latest completed frame, old
// frames are skipped (tracked via skipped_frames/lost_frames). Emits ONE TAS1
// packet up to 960 frames, payload preserving PCM sample bits exactly.
// The caller must invoke this repeatedly while status=OK to drain backlog.
// On errors state and counters are unchanged, and no packet is published.
// Caller must synchronize concurrent writes, stream stop/start, and lifetime.
int tas_dma_tap_poll(tas_dma_tap* state,
                     const void* dma, size_t dma_bytes,
                     uint64_t completed_frames,
                     void* packet, size_t packet_capacity,
                     size_t* packet_bytes, uint64_t* skipped_frames);

#ifdef __cplusplus
}
#endif
#endif
