// SPDX-License-Identifier: MIT
#include "tas_device_session.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

static uint16_t get16(const uint8_t* p) { return uint16_t(p[0])|uint16_t(p[1])<<8; }
static uint32_t get32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static uint64_t get64(const uint8_t* p) {return uint64_t(get32(p)) | uint64_t(get32(p+4))<<32;}
static void run(uint16_t n,uint32_t mask){
 const size_t stride=TAS_FIFO_HEADER_BYTES+TAS_FIFO_MAX_FRAMES*n*4;
 std::vector<uint8_t> storage(stride*3);
 std::array<size_t,3> lengths{};
 tas_packet_fifo q{};
 assert(tas_packet_fifo_init(&q, storage.data(),storage.size(),stride,lengths.data(),3,n,mask,0x193abedc)==TAS_FIFO_OK);
 tas_device_session s{};
 assert(tas_session_init(&s,&q)==TAS_SESSION_OK);
 std::array<uint8_t,TAS_IOCTL_STATUS_BYTES+8> result{};
 size_t written=10;
 assert(tas_session_read(&s,555,TAS_IOCTL_GET_STATUS,0,result.data(),result.size(),&written)==TAS_SESSION_DENIED && written==0);
 assert(tas_session_open(&s,555)==TAS_SESSION_OK);
 assert(tas_session_read(&s,444,TAS_IOCTL_GET_STATUS,0,result.data(),result.size(),&written)==TAS_SESSION_DENIED && written==0);
 assert(tas_session_read(&s,555,TAS_IOCTL_GET_STATUS,1,result.data(),result.size(),&written)==TAS_SESSION_BAD_ARGUMENT);
 assert(tas_session_read(&s,555,TAS_IOCTL_GET_STATUS,0,result.data(),TAS_IOCTL_STATUS_BYTES-1,&written)==TAS_SESSION_BUFFER_SMALL && written==0);
 result.fill(0xa5);
 assert(tas_session_read(&s,555,TAS_IOCTL_GET_STATUS,0,result.data(),result.size(),&written)==TAS_SESSION_OK);
 assert(written==TAS_IOCTL_STATUS_BYTES);
 assert(std::memcmp(result.data(),"TAST",4)==0);
 assert(get16(result.data()+4)==1 && get16(result.data()+6)==40);
 assert(get32(result.data()+8)==48000 && get32(result.data()+12)==mask);
 assert(get16(result.data()+16)==n && get16(result.data()+18)==1);
 assert(get32(result.data()+20)==0x193abedc);
 assert(get64(result.data()+24)==1);
 assert(get32(result.data()+32)==0 && get32(result.data()+36)==0);
 for(size_t i=40;i<result.size();++i)assert(result[i]==0xa5);
 assert(tas_session_close(&s,555)==TAS_SESSION_OK);
 assert(tas_session_open(&s,666)==TAS_SESSION_OK);
 assert(tas_session_read(&s,666,TAS_IOCTL_GET_STATUS,0,result.data(),result.size(),&written)==TAS_SESSION_OK);
 assert(get64(result.data()+24)==2);
}
int main(){
 run(2,0x3);run(3,0xB);run(6,0x60F);run(8,0x63F);
 static_assert(TAS_IOCTL_GET_STATUS==0x00226004u);
 std::cout << "TAST protocol handshake / session version / unauthorized access OK\n";
}
