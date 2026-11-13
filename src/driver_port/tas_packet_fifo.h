// SPDX-License-Identifier: MIT
// Portable, bounded, lock-external kernel/user boundary packet queue.
// NO allocation, synchronization, or calls into a Windows driver.
// Integrator MUST serialize all calls and copy only completed PCM data.
#ifndef TAS_PACKET_FIFO_H
#define TAS_PACKET_FIFO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum tas_fifo_result {
 TAS_FIFO_OK=0,
 TAS_FIFO_EMPTY=1,
 TAS_FIFO_BAD_ARGUMENT=2,
 TAS_FIFO_BAD_FORMAT=3,
 TAS_FIFO_OUTPUT_SMALL=4,
 TAS_FIFO_OUT_OF_ORDER=5
};
#define TAS_FIFO_HEADER_BYTES 40u
#define TAS_FIFO_MAX_FRAMES 960u
#define TAS_FIFO_MAX_CHANNELS 8u
#define TAS_FIFO_MAX_SLOTS 64u
#define TAS_FIFO_MAX_PACKET_BYTES (TAS_FIFO_HEADER_BYTES + TAS_FIFO_MAX_FRAMES*TAS_FIFO_MAX_CHANNELS*4u)

typedef struct tas_packet_fifo {
 uint8_t* data;
 size_t data_bytes;
 size_t stride;
 size_t* lengths;
 uint32_t capacity;
 uint16_t channels;
 uint32_t channel_mask;
 uint32_t source_token;
 uint64_t read_index;
 uint64_t write_index;
 uint64_t dropped_packets;
 uint64_t next_expected_frame;
 uint8_t has_expected_frame;
} tas_packet_fifo;

// Caller owns data and lengths for the ENTIRE lifespan of fifo.
// Each slot has a fixed stride, sized for a full 960-frame packet.
// No PCM is retained outside the FIFO's bounded allocations.
int tas_packet_fifo_init(tas_packet_fifo* fifo, void* data, size_t data_bytes,
                         size_t stride, size_t* lengths, uint32_t capacity,
                         uint16_t channels, uint32_t mask, uint32_t token);
// Packet must be version 1 TAS1, 48kHz float32, finite, no flags.
// When full, oldest packet is evicted so latency cannot grow without bound.
// firstFrame must not rewind or overlap any previously admitted packet.
int tas_packet_fifo_push(tas_packet_fifo* fifo, const void* packet, size_t bytes);
// Returns one exact packet, or EMPTY. A too-small destination does NOT pop.
int tas_packet_fifo_pop(tas_packet_fifo* fifo, void* out, size_t capacity,
                        size_t* written);
void tas_packet_fifo_clear(tas_packet_fifo* fifo);
uint32_t tas_packet_fifo_pending(const tas_packet_fifo* fifo);
#ifdef __cplusplus
}
#endif
#endif
