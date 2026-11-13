// SPDX-License-Identifier: MIT
#include "tas_device_session.h"
#include "tas_ioctl_dispatch.h"
#include <limits.h>
#include <string.h>

static int valid_queue(const tas_device_session* s) {
 const tas_packet_fifo* q;
 if(!s || !(q=s->fifo) || !q->data || !q->lengths || !q->capacity ||
    q->capacity>TAS_FIFO_MAX_SLOTS || q->stride>TAS_FIFO_MAX_PACKET_BYTES ||
    q->stride<TAS_FIFO_HEADER_BYTES || q->data_bytes/q->stride<q->capacity ||
    q->write_index<q->read_index || q->write_index-q->read_index>q->capacity)
   return 0;
 return 1;
}
int tas_session_init(tas_device_session* s, tas_packet_fifo* q) {
 if(!s || !q) return TAS_SESSION_BAD_ARGUMENT;
 memset(s,0,sizeof(*s));s->fifo=q;
 if(!valid_queue(s)){s->fifo=0;return TAS_SESSION_BROKEN;}
 // No old PCM is retained when the control surface becomes available.
 tas_packet_fifo_clear(q);
 return TAS_SESSION_OK;
}
int tas_session_open(tas_device_session* s, uintptr_t key) {
 if(!key || !valid_queue(s))return TAS_SESSION_BAD_ARGUMENT;
 if(s->reader_active)return TAS_SESSION_BUSY;
 if(s->generation==UINT64_MAX)return TAS_SESSION_BROKEN;
 tas_packet_fifo_clear(s->fifo);
 s->reader_key=key;s->reader_active=1;s->generation++;
 return TAS_SESSION_OK;
}
int tas_session_close(tas_device_session* s, uintptr_t key) {
 if(!s || !key || !valid_queue(s))return TAS_SESSION_BAD_ARGUMENT;
 if(!s->reader_active)return TAS_SESSION_OK; // CLEANUP followed by CLOSE
 if(s->reader_key!=key)return TAS_SESSION_DENIED;
 s->reader_active=0;s->reader_key=0;
 tas_packet_fifo_clear(s->fifo); // scrub user audio on disconnect
 return TAS_SESSION_OK;
}
void tas_session_shutdown(tas_device_session* s) {
 if(!s)return;
 s->reader_active=0;s->reader_key=0;
 if(valid_queue(s))tas_packet_fifo_clear(s->fifo);
}
int tas_session_push(tas_device_session* s,const void* packet,size_t bytes) {
 if(!valid_queue(s))return TAS_SESSION_BROKEN;
 if(!s->reader_active)return TAS_SESSION_DENIED;
 return tas_packet_fifo_push(s->fifo,packet,bytes)==TAS_FIFO_OK?TAS_SESSION_OK:TAS_SESSION_BROKEN;
}
int tas_session_read(tas_device_session* s, uintptr_t key, uint32_t ioctl,
                     size_t input_length, void* system_output,
                     size_t output_capacity, size_t* written) {
 int r;
 if(!written)return TAS_SESSION_BAD_ARGUMENT;
 *written=0;
 if(!key || !valid_queue(s))return TAS_SESSION_BAD_ARGUMENT;
 if(!s->reader_active || s->reader_key!=key)return TAS_SESSION_DENIED;
 if(ioctl==TAS_IOCTL_GET_STATUS){
  // Status is encoded byte-by-byte to avoid kernel/user struct packing issues.
  uint8_t* b;
  uint64_t gen;
  uint32_t dropped;
  unsigned i;
  if(input_length!=0)return TAS_SESSION_BAD_ARGUMENT;
  if(!system_output || output_capacity<TAS_IOCTL_STATUS_BYTES)return TAS_SESSION_BUFFER_SMALL;
  b=(uint8_t*)system_output;
  gen=s->generation;
  dropped=s->fifo->dropped_packets>UINT32_MAX?UINT32_MAX:(uint32_t)s->fifo->dropped_packets;
  memset(b,0,TAS_IOCTL_STATUS_BYTES);
  b[0]='T';b[1]='A';b[2]='S';b[3]='T';
  b[4]=1;b[6]=TAS_IOCTL_STATUS_BYTES;
  b[8]=0x80;b[9]=0xbb; // 48000 Hz
  for(i=0;i<4;i++){
   b[12+i]=(uint8_t)(s->fifo->channel_mask>>(8*i));
   b[20+i]=(uint8_t)(s->fifo->source_token>>(8*i));
   b[32+i]=(uint8_t)(tas_packet_fifo_pending(s->fifo)>>(8*i));
   b[36+i]=(uint8_t)(dropped>>(8*i));
  }
  b[16]=(uint8_t)s->fifo->channels;
  b[18]=1;
  for(i=0;i<8;i++)b[24+i]=(uint8_t)(gen>>(8*i));
  *written=TAS_IOCTL_STATUS_BYTES;
  return TAS_SESSION_OK;
 }
 r=tas_ioctl_dispatch(s->fifo,ioctl,input_length,system_output,output_capacity,written);
 switch(r){
  case TAS_IOCTL_SUCCESS: return TAS_SESSION_OK;
  case TAS_IOCTL_EMPTY: return TAS_SESSION_EMPTY;
  case TAS_IOCTL_BUFFER_TOO_SMALL: return TAS_SESSION_BUFFER_SMALL;
  case TAS_IOCTL_BAD_CONTROL_CODE: return TAS_SESSION_BAD_CONTROL;
  case TAS_IOCTL_INVALID_REQUEST: return TAS_SESSION_BAD_ARGUMENT;
  default: return TAS_SESSION_BROKEN;
 }
}
