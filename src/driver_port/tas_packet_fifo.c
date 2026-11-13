// SPDX-License-Identifier: MIT
#include "tas_packet_fifo.h"
#include <string.h>
#include <limits.h>

static uint16_t le16(const uint8_t*b,size_t i) {
 return (uint16_t)((uint16_t)b[i] | ((uint16_t)b[i+1]<<8));
}
static uint32_t le32(const uint8_t*b,size_t i) {
 return ((uint32_t)b[i] | ((uint32_t)b[i+1]<<8) | ((uint32_t)b[i+2]<<16) | ((uint32_t)b[i+3]<<24));
}
static uint64_t le64(const uint8_t*b,size_t i) {
 return (uint64_t)le32(b,i) | ((uint64_t)le32(b,i+4)<<32);
}
static int valid_format(uint16_t n,uint32_t m) {
 return (n==2 && m==0x3u) || (n==3 && m==0xBu) ||
        (n==6 && (m==0x3Fu || m==0x60Fu)) || (n==8 && m==0x63Fu);
}
int tas_packet_fifo_init(tas_packet_fifo*fifo,void*data,size_t data_bytes,
                         size_t stride,size_t*lengths,uint32_t capacity,
                         uint16_t channels,uint32_t mask,uint32_t token) {
 size_t min_stride;
 if(!fifo || !data || !lengths || capacity<2 || capacity>TAS_FIFO_MAX_SLOTS ||
    !valid_format(channels,mask))return TAS_FIFO_BAD_ARGUMENT;
 min_stride=TAS_FIFO_HEADER_BYTES+(size_t)TAS_FIFO_MAX_FRAMES*channels*4u;
 if(stride<min_stride || stride>TAS_FIFO_MAX_PACKET_BYTES ||
    data_bytes/stride<capacity)return TAS_FIFO_BAD_ARGUMENT;
 memset(fifo,0,sizeof(*fifo));
 memset(lengths,0,sizeof(*lengths)*capacity);
 fifo->data=(uint8_t*)data;fifo->data_bytes=data_bytes;
 fifo->stride=stride;fifo->lengths=lengths;fifo->capacity=capacity;
 fifo->channels=channels;fifo->channel_mask=mask;fifo->source_token=token;
 return TAS_FIFO_OK;
}
int tas_packet_fifo_push(tas_packet_fifo*fifo,const void*packet,size_t bytes) {
 const uint8_t*b=(const uint8_t*)packet;
 uint16_t channels;
 uint32_t count;
 uint64_t first;
 size_t expected;
 size_t n;
 if(!fifo || !fifo->data || !fifo->lengths || !packet)return TAS_FIFO_BAD_ARGUMENT;
 if(bytes<TAS_FIFO_HEADER_BYTES || bytes>fifo->stride || bytes>TAS_FIFO_MAX_PACKET_BYTES)return TAS_FIFO_BAD_FORMAT;
 if(le32(b,0)!=0x31534154u || le16(b,4)!=1 || le16(b,6)!=TAS_FIFO_HEADER_BYTES ||
    le32(b,8)!=48000u || le32(b,12)!=fifo->channel_mask ||
    le16(b,30)!=1 || le32(b,32)!=fifo->source_token || le32(b,36)!=0)
  return TAS_FIFO_BAD_FORMAT;
 channels=le16(b,28); count=le32(b,24); first=le64(b,16);
 if(channels!=fifo->channels || count==0 || count>TAS_FIFO_MAX_FRAMES ||
    first>UINT64_MAX-count)return TAS_FIFO_BAD_FORMAT;
 expected=TAS_FIFO_HEADER_BYTES+(size_t)count*channels*4u;
 if(bytes!=expected)return TAS_FIFO_BAD_FORMAT;
 for(n=TAS_FIFO_HEADER_BYTES;n<bytes;n+=4) {
  uint32_t bits=le32(b,n);
  if((bits&0x7f800000u)==0x7f800000u)return TAS_FIFO_BAD_FORMAT;
 }
 if(fifo->has_expected_frame && first<fifo->next_expected_frame)
  return TAS_FIFO_OUT_OF_ORDER;
 if(fifo->write_index-fifo->read_index >= fifo->capacity){
  fifo->read_index++;fifo->dropped_packets++;
 }
 {
  const size_t slot=(size_t)(fifo->write_index%fifo->capacity);
  // The previous packet may have been longer. Do not retain stale PCM.
  memset(fifo->data+slot*fifo->stride,0,fifo->stride);
  memcpy(fifo->data+slot*fifo->stride,b,bytes);
  fifo->lengths[slot]=bytes;
 }
 fifo->write_index++;
 fifo->next_expected_frame=first+count;
 fifo->has_expected_frame=1;
 return TAS_FIFO_OK;
}
int tas_packet_fifo_pop(tas_packet_fifo*fifo,void*out,size_t capacity,size_t*written) {
 size_t slot,len;
 if(!fifo || !fifo->data || !fifo->lengths || !out || !written)return TAS_FIFO_BAD_ARGUMENT;
 *written=0;
 if(fifo->write_index==fifo->read_index)return TAS_FIFO_EMPTY;
 slot=(size_t)(fifo->read_index%fifo->capacity);
 len=fifo->lengths[slot];
 if(!len || len>fifo->stride)return TAS_FIFO_BAD_FORMAT;
 if(capacity<len)return TAS_FIFO_OUTPUT_SMALL;
 memcpy(out,fifo->data+slot*fifo->stride,len);
 // Remove consumed audio from the kernel-side ring after the copy.
 memset(fifo->data+slot*fifo->stride,0,fifo->stride);
 fifo->lengths[slot]=0;
 fifo->read_index++;
 *written=len;
 return TAS_FIFO_OK;
}
void tas_packet_fifo_clear(tas_packet_fifo*fifo) {
 if(!fifo)return;
 fifo->read_index=0;fifo->write_index=0;
 fifo->dropped_packets=0;fifo->next_expected_frame=0;fifo->has_expected_frame=0;
 if(fifo->lengths)memset(fifo->lengths,0,sizeof(*fifo->lengths)*fifo->capacity);
 if(fifo->data && fifo->capacity && fifo->stride &&
    fifo->data_bytes/fifo->stride>=fifo->capacity)
  memset(fifo->data,0,fifo->capacity*fifo->stride);
}
uint32_t tas_packet_fifo_pending(const tas_packet_fifo*fifo) {
 if(!fifo || !fifo->data)return 0;
 if(fifo->write_index<fifo->read_index)return 0;
 return (uint32_t)(fifo->write_index-fifo->read_index);
}
