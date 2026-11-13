// SPDX-License-Identifier: MIT
#include "tas_stream_adapter.h"
int tas_stream_adapter_poll(tas_dma_tap* tap,tas_packet_fifo* fifo,
                            const void* dma,size_t dma_bytes,uint64_t completed_frames,
                            void* staging,size_t staging_size,
                            uint64_t* overwritten_frames){
 size_t bytes=0;uint64_t lost=0;
 int result;
 if(!tap||!fifo||!staging||!overwritten_frames||!tap->initialized||
    fifo->channels!=tap->channels||fifo->channel_mask!=tap->channel_mask||
    fifo->source_token!=tap->source_token)return TAS_ADAPTER_INVALID;
 // Avoid partial packet extraction due to a staging buffer smaller than the
 // largest packet. A driver allocates this from nonpaged pool, not stack.
 if(staging_size<TAS_FIFO_HEADER_BYTES+(size_t)TAS_FIFO_MAX_FRAMES*tap->channels*4u)
  return TAS_ADAPTER_INVALID;
 result=tas_dma_tap_poll(tap,dma,dma_bytes,completed_frames,staging,
                         staging_size,&bytes,&lost);
 if(result==TAS_DMA_EMPTY){*overwritten_frames=0;return TAS_ADAPTER_EMPTY;}
 if(result==TAS_DMA_NONFINITE_SAMPLE)return TAS_ADAPTER_CORRUPT_PCM;
 if(result!=TAS_DMA_OK)return TAS_ADAPTER_INVALID;
 // The tap cursor has advanced. If the push fails, a stream reset is required
 // by the host; do not conceal a broken queue as an empty stream.
 if(tas_packet_fifo_push(fifo,staging,bytes)!=TAS_FIFO_OK)return TAS_ADAPTER_INVALID;
 *overwritten_frames=lost;
 return TAS_ADAPTER_OK;
}

int tas_stream_adapter_poll_session(tas_dma_tap* tap,tas_device_session* session,
                            const void* dma,size_t dma_bytes,uint64_t completed_frames,
                            void* staging,size_t staging_size,
                            uint64_t* overwritten_frames){
 if(!tap||!session||!session->fifo||!overwritten_frames)return TAS_ADAPTER_INVALID;
 *overwritten_frames=0;
 if(!session->reader_active)return TAS_ADAPTER_INACTIVE;
 return tas_stream_adapter_poll(tap,session->fifo,dma,dma_bytes,completed_frames,
                                staging,staging_size,overwritten_frames);
}
