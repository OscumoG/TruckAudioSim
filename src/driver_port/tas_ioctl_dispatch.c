// SPDX-License-Identifier: MIT
#include "tas_ioctl_dispatch.h"

int tas_ioctl_dispatch(tas_packet_fifo* fifo, uint32_t control_code,
                       size_t input_length, void* system_output,
                       size_t output_capacity, size_t* bytes_returned) {
 int result;
 if(!bytes_returned) return TAS_IOCTL_INVALID_REQUEST;
 *bytes_returned=0;
 if(control_code!=TAS_IOCTL_GET_PACKET) return TAS_IOCTL_BAD_CONTROL_CODE;
 if(input_length!=0) return TAS_IOCTL_INVALID_REQUEST;
 if(!fifo || !fifo->data || !fifo->lengths || !fifo->capacity ||
    fifo->capacity>TAS_FIFO_MAX_SLOTS || fifo->stride>TAS_FIFO_MAX_PACKET_BYTES ||
    fifo->write_index<fifo->read_index ||
    fifo->write_index-fifo->read_index>fifo->capacity)
   return TAS_IOCTL_BAD_QUEUE;
 if(tas_packet_fifo_pending(fifo)==0)return TAS_IOCTL_EMPTY;
 if(!system_output)return TAS_IOCTL_BUFFER_TOO_SMALL;
 result=tas_packet_fifo_pop(fifo,system_output,output_capacity,bytes_returned);
 switch(result) {
  case TAS_FIFO_OK: return TAS_IOCTL_SUCCESS;
  case TAS_FIFO_EMPTY: return TAS_IOCTL_EMPTY;
  case TAS_FIFO_OUTPUT_SMALL: return TAS_IOCTL_BUFFER_TOO_SMALL;
  default: *bytes_returned=0; return TAS_IOCTL_BAD_QUEUE;
 }
}
