// SPDX-License-Identifier: MIT
#include "tas_capture_coordinator.h"
#include <string.h>

static int reset_cursor(tas_capture_coordinator* c, uint64_t completed) {
 int r;
 if(!c || !c->session || !c->session->fifo) return 0;
 tas_packet_fifo_clear(c->session->fifo);
 r=tas_dma_tap_init(&c->tap, c->session->fifo->channels,
                    c->session->fifo->channel_mask,
                    c->session->fifo->source_token, completed);
 if(r!=TAS_DMA_OK)return 0;
 c->observed_generation=c->session->generation;
 c->reset_count++;
 return 1;
}
int tas_capture_init(tas_capture_coordinator* c, tas_device_session* session,
                     uint16_t channels, uint32_t mask, uint32_t token){
 if(!c || !session || !session->fifo ||
    session->fifo->channels!=channels ||
    session->fifo->channel_mask!=mask ||
    session->fifo->source_token!=token)return TAS_CAPTURE_INVALID;
 memset(c,0,sizeof(*c));
 c->session=session;
 if(tas_dma_tap_init(&c->tap,channels,mask,token,0)!=TAS_DMA_OK){
  memset(c,0,sizeof(*c));return TAS_CAPTURE_INVALID;
 }
 c->observed_generation=session->generation;
 return TAS_CAPTURE_OK;
}
int tas_capture_start(tas_capture_coordinator* c, uint64_t completed){
 if(!c || !c->session || !c->session->fifo)return TAS_CAPTURE_INVALID;
 c->running=0;
 if(!reset_cursor(c,completed))return TAS_CAPTURE_INVALID;
 c->running=1;
 return TAS_CAPTURE_OK;
}
void tas_capture_stop(tas_capture_coordinator* c){
 if(!c)return;
 c->running=0;
 if(c->session && c->session->fifo) tas_packet_fifo_clear(c->session->fifo);
}
int tas_capture_poll(tas_capture_coordinator* c,
                     const void* dma,size_t dma_bytes,uint64_t completed,
                     void* staging,size_t staging_bytes,unsigned limit,
                     unsigned* produced,uint64_t* lost){
 unsigned i;
 size_t frame_bytes;
 if(produced)*produced=0;
 if(lost)*lost=0;
 if(!c || !produced || !lost || !c->session || !c->session->fifo ||
    !dma || !staging || !c->tap.initialized || limit==0 || limit>8 ||
    c->tap.channels==0)return TAS_CAPTURE_INVALID;
 frame_bytes=(size_t)c->tap.channels*4u;
 if(dma_bytes<frame_bytes || dma_bytes%frame_bytes!=0 ||
    dma_bytes>128u*1024u*1024u ||
    staging_bytes<TAS_FIFO_HEADER_BYTES+(size_t)TAS_FIFO_MAX_FRAMES*frame_bytes)
  return TAS_CAPTURE_INVALID;
 if(!c->running)return TAS_CAPTURE_IDLE;
 // Reopening a reader or switching Windows audio streams invalidates old PCM.
 if(c->observed_generation!=c->session->generation){
  if(!reset_cursor(c,completed))return TAS_CAPTURE_INVALID;
  return TAS_CAPTURE_RESET;
 }
 if(!c->session->reader_active){
  // Never replay sound rendered before the service connected.
  // Also avoid accumulating thousands of frames while no consumer exists.
  c->tap.next_frame=completed;
  return TAS_CAPTURE_NO_READER;
 }
 if(completed<c->tap.next_frame){
  // A stream restart can reset the OS frame counter. Do not read stale ring.
  if(!reset_cursor(c,completed))return TAS_CAPTURE_INVALID;
  return TAS_CAPTURE_RESET;
 }
 for(i=0;i<limit;++i){
  uint64_t skipped=0;
  const int result=tas_stream_adapter_poll_session(&c->tap,c->session,
      dma,dma_bytes,completed,staging,staging_bytes,&skipped);
  if(result==TAS_ADAPTER_EMPTY)break;
  if(result==TAS_ADAPTER_INACTIVE)return TAS_CAPTURE_NO_READER;
  if(result!=TAS_ADAPTER_OK){
   // Invalid audio must not leave old queued samples accessible.
   if(!reset_cursor(c,completed))return TAS_CAPTURE_INVALID;
   return result==TAS_ADAPTER_CORRUPT_PCM?TAS_CAPTURE_BAD_PCM:TAS_CAPTURE_INVALID;
  }
  (*produced)++;
  *lost+=skipped;
  c->packets_emitted++;
  c->frames_overwritten+=skipped;
 }
 return *produced?TAS_CAPTURE_OK:TAS_CAPTURE_IDLE;
}
