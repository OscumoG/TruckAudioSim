// SPDX-License-Identifier: MIT
#include "tas_stream_adapter.h"
#include "driver_wire.hpp"
#include "wav.hpp"
#include <cassert>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <array>
using namespace tas;
int main(){
 constexpr uint16_t channels=8;
 constexpr uint32_t mask=kSevenPointOne,token=77;
 constexpr size_t ringFrames=960;
 std::vector<uint8_t> dma(ringFrames*channels*4);
 std::vector<float> samples(960*channels);
 for(size_t frame=0;frame<960;frame++)for(size_t ch=0;ch<channels;ch++)
  samples[frame*channels+ch]=float(frame+ch)/4096.0f;
 std::memcpy(dma.data(),samples.data(),dma.size());
 tas_dma_tap tap{};
 assert(tas_dma_tap_init(&tap,channels,mask,token,0)==TAS_DMA_OK);
 constexpr size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*channels*4u;
 std::vector<uint8_t> storage(2*stride),out(stride),staging(stride);
 std::array<size_t,2> lengths{};
 tas_packet_fifo fifo{};
 assert(tas_packet_fifo_init(&fifo,storage.data(),storage.size(),stride,
                             lengths.data(),2,channels,mask,token)==TAS_FIFO_OK);
 uint64_t overwritten=99;
 assert(tas_stream_adapter_poll(&tap,&fifo,dma.data(),dma.size(),960,staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_OK);
 assert(overwritten==0 && tas_packet_fifo_pending(&fifo)==1);
 size_t copied=0;
 assert(tas_packet_fifo_pop(&fifo,out.data(),out.size(),&copied)==TAS_FIFO_OK);
 auto decoded=decodeDriverPacket(std::span<const uint8_t>(out.data(),copied));
 assert(decoded.samples==samples);
 assert(tas_stream_adapter_poll(&tap,&fifo,dma.data(),dma.size(),960,staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_EMPTY);
 assert(tas_stream_adapter_poll(&tap,&fifo,dma.data(),dma.size(),960,staging.data(),10,&overwritten)==TAS_ADAPTER_INVALID);
 tas_dma_tap wrong{};
 assert(tas_dma_tap_init(&wrong,2,kStereo,token,0)==TAS_DMA_OK);
 assert(tas_stream_adapter_poll(&wrong,&fifo,dma.data(),dma.size(),960,staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_INVALID);
 // Session-bound producer is gated: no capture at rest or after cleanup.
 tas_device_session session{};
 assert(tas_session_init(&session,&fifo)==TAS_SESSION_OK);
 assert(tas_dma_tap_init(&tap,channels,mask,token,0)==TAS_DMA_OK);
 assert(tas_stream_adapter_poll_session(&tap,&session,dma.data(),dma.size(),960,
                   staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_INACTIVE);
 assert(tas_packet_fifo_pending(&fifo)==0);
 assert(tas_session_open(&session,555)==TAS_SESSION_OK);
 assert(tas_stream_adapter_poll_session(&tap,&session,dma.data(),dma.size(),960,
                   staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_OK);
 assert(tas_packet_fifo_pending(&fifo)==1);
 assert(tas_session_close(&session,555)==TAS_SESSION_OK);
 assert(tas_packet_fifo_pending(&fifo)==0);
 for(uint8_t b:storage)assert(b==0);
 assert(tas_stream_adapter_poll_session(&tap,&session,dma.data(),dma.size(),960,
                   staging.data(),staging.size(),&overwritten)==TAS_ADAPTER_INACTIVE);
 assert(tas_packet_fifo_pending(&fifo)==0);
 std::cout<<"WaveRT DMA tap -> bounded packet queue -> verified 7.1 PCM OK\n";
}
