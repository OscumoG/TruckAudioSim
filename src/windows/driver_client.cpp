// SPDX-License-Identifier: MIT
// Windows user-mode reader for a FUTURE TruckAudioSim WaveRT driver.
// No driver installation; requires a separately implemented, signed WDK
// device that exposes the restricted TAS_IOCTL_GET_PACKET interface.
#include <windows.h>
#include <winioctl.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include "tas_ioctl_contract.h"
#include "tas_packet_fifo.h"
#include "driver_wire.hpp"
#include "device_select.hpp"
#include "pipe_writer.hpp"
#include "route_guard.hpp"

static_assert(TAS_IOCTL_GET_PACKET == CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_READ_ACCESS),
              "Kernel/user IOCTL ABI mismatch");
namespace {
struct Handle {
 HANDLE value=INVALID_HANDLE_VALUE;
 explicit Handle(HANDLE h):value(h){}
 ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle&operator=(const Handle&)=delete;
};
struct ComInit {
 ComInit(){if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))throw std::runtime_error("COM initialization failed");}
 ~ComInit(){CoUninitialize();}
};
struct Settings {
 std::wstring devicePath=L"\\\\.\\TruckAudioSimPcm";
 std::optional<uint32_t> sourceIndex;
 std::wstring sourceId;
 unsigned seconds=60;
};
Settings parse(int argc,char**argv){
 Settings s;
 for(int i=1;i<argc;i++){
  std::string arg=argv[i];
  if(arg=="--help"){
   std::cout<<"tas_driver_client --source-index N [--seconds 60]\n"
    "                 [--source-id ENDPOINT_ID] [--device-path DEVICE_PATH]\n"
    "Reads from a future signed TAS virtual audio driver via DeviceIoControl.\n"
    "This release DOES NOT install or contain such a driver.\n";
   std::exit(0);
  }
  if(i+1>=argc)throw std::runtime_error("Missing value: "+arg);
  std::string value=argv[++i];
  if(arg=="--source-id")s.sourceId.assign(value.begin(),value.end());
  else if(arg=="--device-path")s.devicePath.assign(value.begin(),value.end());
  else if(arg=="--source-index" || arg=="--seconds"){
   size_t consumed=0;unsigned long n=std::stoul(value,&consumed);
   if(consumed!=value.size()||n>UINT32_MAX)throw std::runtime_error("Bad number: "+value);
   if(arg=="--source-index")s.sourceIndex=static_cast<uint32_t>(n);
   else {if(n<1||n>3600)throw std::runtime_error("--seconds must be 1..3600");s.seconds=static_cast<unsigned>(n);}
  }else throw std::runtime_error("Unknown argument: "+arg);
 }
 if(bool(s.sourceIndex)==!s.sourceId.empty())throw std::runtime_error("Specify exactly one --source-index or --source-id");
 if(s.devicePath.empty())throw std::runtime_error("Device path required");
 return s;
}
int run(const Settings& s){
 ComInit com;
 Microsoft::WRL::ComPtr<IMMDeviceEnumerator> devices;
 if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices))))
  throw std::runtime_error("Cannot enumerate audio devices");
 auto source=tas::selectRenderDevice(devices.Get(),s.sourceIndex,s.sourceId);
 auto sourceToken=tas::audioEndpointToken(tas::renderDeviceId(source.Get()));
 if(!sourceToken)throw std::runtime_error("Cannot identify source endpoint");
 Handle device(CreateFileW(s.devicePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
                           OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
 if(device.value==INVALID_HANDLE_VALUE)throw std::runtime_error(
  "Cannot open virtual driver; it has not been built/installed, or its ACL blocks access");
 // Fail early if the kernel ABI, stream format, or device identity differs.
 std::array<uint8_t,TAS_IOCTL_STATUS_BYTES> status{};
 DWORD statusBytes=0;
 if(!DeviceIoControl(device.value,TAS_IOCTL_GET_STATUS,nullptr,0,status.data(),
     static_cast<DWORD>(status.size()),&statusBytes,nullptr) || statusBytes!=status.size())
  throw std::runtime_error("Driver does not support the v0.16 status handshake");
 const auto read16=[&](size_t i)->uint16_t{
  return uint16_t(status[i])|uint16_t(status[i+1])<<8;
 };
 const auto read32=[&](size_t i)->uint32_t{
  return uint32_t(status[i])|uint32_t(status[i+1])<<8|
         uint32_t(status[i+2])<<16|uint32_t(status[i+3])<<24;
 };
 if(status[0]!='T'||status[1]!='A'||status[2]!='S'||status[3]!='T'||
    read16(4)!=1||read16(6)!=TAS_IOCTL_STATUS_BYTES||
    read32(8)!=48000||read16(18)!=1||
    read32(20)!=sourceToken)
  throw std::runtime_error("Driver ABI or virtual endpoint identity mismatch");
 std::cout<<"Virtual driver reports "<<read16(16)<<" channels, mask "
          <<read32(12)<<" and PCM float32 48000 Hz\n";
 std::array<uint8_t,TAS_FIFO_MAX_PACKET_BYTES> buffer{};
 std::optional<tas::StreamFormat> streamFormat;
 std::unique_ptr<tas::PipeWriter> pipe;
 uint64_t expected=0,gaps=0,packets=0;
 bool started=false;
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(s.seconds);
 while(std::chrono::steady_clock::now()<deadline){
  DWORD size=0;
  BOOL ok=DeviceIoControl(device.value,TAS_IOCTL_GET_PACKET,nullptr,0,buffer.data(),
                          static_cast<DWORD>(buffer.size()),&size,nullptr);
  if(!ok){
   const DWORD code=GetLastError();
   if(code==ERROR_NO_MORE_ITEMS){std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
   throw std::runtime_error("Driver IOCTL_GET_PACKET failed, Windows error "+std::to_string(code));
  }
  if(size<tas::kDriverHeaderBytes || size>buffer.size())throw std::runtime_error("Invalid driver response length");
  const auto decoded=tas::decodeDriverPacket(std::span<const uint8_t>(buffer.data(),size));
  const auto& info=decoded.info;
  if(info.format.sourceEndpointToken!=sourceToken)
   throw std::runtime_error("Packet source endpoint identity does not match selected virtual device");
  if(!streamFormat){
   streamFormat=info.format;
   // WaitNamedPipe uses a local, test-only PCM pipe; start tas_spatial --live first.
   pipe=std::make_unique<tas::PipeWriter>(*streamFormat);
  }else if(info.format.channels!=streamFormat->channels ||
            info.format.channelMask!=streamFormat->channelMask ||
            info.format.sampleRate!=streamFormat->sampleRate)
   throw std::runtime_error("Stream format changed without restarting capture");
  if(started && info.firstFrame<expected)throw std::runtime_error("Rewound/overlapping driver packet");
  if(started && info.firstFrame>expected)gaps+=info.firstFrame-expected;
  expected=info.firstFrame+info.frameCount;started=true;packets++;
  pipe->frames(decoded.samples.data(),info.frameCount,false);
 }
 if(pipe)pipe->finish();
 std::cout<<"Received "<<packets<<" PCM packets; missing frames "<<gaps<<"\n";
 return packets?0:2;
}
}
int main(int argc,char**argv){
 try{return run(parse(argc,argv));}
 catch(const std::exception& e){std::cerr<<"tas_driver_client: "<<e.what()<<"\n";return 1;}
}
