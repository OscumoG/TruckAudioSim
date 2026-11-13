// SPDX-License-Identifier: MIT
#include "wavert_dma_tap.h"
#include <string.h>

#define TAS_HEADER_SIZE 40u
#define TAS_PACKET_MAX_FRAMES 960u
#define TAS_RATE 48000u
#define TAS_MAGIC 0x31534154u

static void put16(uint8_t* b, size_t p, uint16_t v) {
 b[p]=(uint8_t)v; b[p+1]=(uint8_t)(v>>8);
}
static void put32(uint8_t* b, size_t p, uint32_t v) {
 unsigned i; for(i=0;i<4;i++) b[p+i]=(uint8_t)(v>>(8*i));
}
static void put64(uint8_t* b, size_t p, uint64_t v) {
 unsigned i; for(i=0;i<8;i++) b[p+i]=(uint8_t)(v>>(8*i));
}
static int valid_format(uint16_t channels, uint32_t mask) {
 return (channels==2 && mask==0x3u)
     || (channels==3 && mask==0xBu)
     || (channels==6 && (mask==0x3Fu || mask==0x60Fu))
     || (channels==8 && mask==0x63Fu);
}
int tas_dma_tap_init(tas_dma_tap* state, uint16_t channels,
                     uint32_t mask, uint32_t token, uint64_t first) {
 if(!state) return TAS_DMA_BAD_ARGUMENT;
 memset(state,0,sizeof(*state));
 if(!valid_format(channels,mask)) return TAS_DMA_BAD_FORMAT;
 state->next_frame=first;
 state->lost_frames=0;
 state->channels=channels;
 state->channel_mask=mask;
 state->source_token=token;
 state->initialized=1;
 return TAS_DMA_OK;
}
int tas_dma_tap_poll(tas_dma_tap* state,
                     const void* dma, size_t dma_bytes,
                     uint64_t completed,
                     void* packet, size_t capacity,
                     size_t* packet_bytes, uint64_t* skipped_frames) {
 uint64_t candidate, skip, ring_frames, available;
 uint32_t count;
 size_t frame_bytes, needed, f, c;
 uint8_t* out;
 const uint8_t* ring;
 if(!state||!state->initialized||!dma||!packet||!packet_bytes||!skipped_frames)
  return TAS_DMA_BAD_ARGUMENT;
 if(!valid_format(state->channels,state->channel_mask))return TAS_DMA_BAD_FORMAT;
 frame_bytes=(size_t)state->channels*4u;
 if(dma_bytes<frame_bytes || dma_bytes%frame_bytes!=0 || dma_bytes>128u*1024u*1024u)
  return TAS_DMA_BAD_ARGUMENT;
 if(completed<state->next_frame)return TAS_DMA_COUNTER_REWOUND;
 if(completed==state->next_frame){*packet_bytes=0;*skipped_frames=0;return TAS_DMA_EMPTY;}
 ring_frames=dma_bytes/frame_bytes;
 available=completed-state->next_frame;
 skip=available>ring_frames?available-ring_frames:0;
 candidate=state->next_frame+skip;
 available=completed-candidate;
 if(capacity<TAS_HEADER_SIZE+frame_bytes)return TAS_DMA_OUTPUT_SMALL;
 if(available>TAS_PACKET_MAX_FRAMES)available=TAS_PACKET_MAX_FRAMES;
 if(available>(capacity-TAS_HEADER_SIZE)/frame_bytes)available=(capacity-TAS_HEADER_SIZE)/frame_bytes;
 count=(uint32_t)available;
 needed=TAS_HEADER_SIZE+(size_t)count*frame_bytes;
 // Validate every sample before publishing the packet or advancing cursor.
 // This also avoids exposing unexpected NaN/Inf from a malformed DMA stream.
 ring=(const uint8_t*)dma;
 for(f=0;f<count;f++) {
  size_t off=(size_t)((candidate+f)%ring_frames)*frame_bytes;
  for(c=0;c<state->channels;c++) {
   const uint8_t* s=ring+off+c*4;
   uint32_t bits=(uint32_t)s[0]|((uint32_t)s[1]<<8)|((uint32_t)s[2]<<16)|((uint32_t)s[3]<<24);
   if((bits&0x7f800000u)==0x7f800000u)return TAS_DMA_NONFINITE_SAMPLE;
  }
 }
 out=(uint8_t*)packet;
 put32(out,0,TAS_MAGIC);put16(out,4,1);put16(out,6,TAS_HEADER_SIZE);
 put32(out,8,TAS_RATE);put32(out,12,state->channel_mask);
 put64(out,16,candidate);put32(out,24,count);
 put16(out,28,state->channels);put16(out,30,1);
 put32(out,32,state->source_token);put32(out,36,0);
 for(f=0;f<count;f++) {
  size_t off=(size_t)((candidate+f)%ring_frames)*frame_bytes;
  memcpy(out+TAS_HEADER_SIZE+f*frame_bytes,ring+off,frame_bytes);
 }
 state->next_frame=candidate+count;
 state->lost_frames+=skip;
 *packet_bytes=needed;
 *skipped_frames=skip;
 return TAS_DMA_OK;
}
