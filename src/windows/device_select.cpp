// SPDX-License-Identifier: MIT
#include "device_select.hpp"
#include <windows.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
namespace tas {
namespace {
void require(HRESULT hr,const char* action){if(FAILED(hr))throw std::runtime_error(std::string(action)+" failed");}

void writeDeviceList(const std::wstring& listing){
 const HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
 DWORD mode=0;
 if(GetConsoleMode(output,&mode)){
  DWORD written=0;
  if(!WriteConsoleW(output,listing.data(),static_cast<DWORD>(listing.size()),&written,nullptr) ||
     written!=listing.size())
   throw std::runtime_error("Could not write the device list to the console");
  return;
 }
 const auto characters=static_cast<int>(listing.size());
 const int bytes=WideCharToMultiByte(CP_UTF8,0,listing.data(),characters,nullptr,0,nullptr,nullptr);
 if(bytes<=0)throw std::runtime_error("Could not encode the device list as UTF-8");
 std::string utf8(static_cast<size_t>(bytes),'\0');
 if(WideCharToMultiByte(CP_UTF8,0,listing.data(),characters,utf8.data(),bytes,nullptr,nullptr)!=bytes)
  throw std::runtime_error("Could not encode the device list as UTF-8");
 std::cout<<utf8;
}
}
std::wstring renderDeviceId(IMMDevice* device){
 LPWSTR raw=nullptr;
 require(device->GetId(&raw),"IMMDevice::GetId");
 std::wstring id=raw?raw:L"";
 CoTaskMemFree(raw);
 return id;
}
void printRenderDevices(IMMDeviceEnumerator* devices){
 Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
 require(devices->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection),"EnumAudioEndpoints");
 UINT count=0;require(collection->GetCount(&count),"IMMDeviceCollection::GetCount");
 std::wostringstream listing;
 listing << L"Active Windows playback endpoints (index is temporary):\n";
 for(UINT i=0;i<count;++i){
  Microsoft::WRL::ComPtr<IMMDevice> device;
  require(collection->Item(i,&device),"IMMDeviceCollection::Item");
  Microsoft::WRL::ComPtr<IPropertyStore> props;
  std::wstring name=L"(unnamed)";
  if(SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))){
   PROPVARIANT value;PropVariantInit(&value);
   if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt==VT_LPWSTR && value.pwszVal)
    name=value.pwszVal;
   PropVariantClear(&value);
  }
  listing << L"  ["<<i<<L"] "<<name<<L"\n"
             << L"      ID: " << renderDeviceId(device.Get()) << L"\n";
 }
 writeDeviceList(listing.str());
}
Microsoft::WRL::ComPtr<IMMDevice> selectRenderDevice(IMMDeviceEnumerator* devices,
                                                      std::optional<uint32_t> index, const std::wstring& stableId){
 if(!stableId.empty()){
  Microsoft::WRL::ComPtr<IMMDevice> match;
  require(devices->GetDevice(stableId.c_str(), &match),"GetDevice(stable ID)");
  Microsoft::WRL::ComPtr<IMMEndpoint> endpoint;
  require(match.As(&endpoint),"IMMEndpoint");
  EDataFlow flow{};
  require(endpoint->GetDataFlow(&flow),"IMMEndpoint::GetDataFlow");
  if(flow!=eRender)throw std::runtime_error("Selected endpoint is not a playback device");
  DWORD state=0;require(match->GetState(&state),"IMMDevice::GetState");
  if(!(state&DEVICE_STATE_ACTIVE))throw std::runtime_error("Selected endpoint is not active");
  return match;
 }
 Microsoft::WRL::ComPtr<IMMDevice> result;
 if(!index){
  require(devices->GetDefaultAudioEndpoint(eRender,eMultimedia,&result),"GetDefaultAudioEndpoint");
  return result;
 }
 Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
 require(devices->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&collection),"EnumAudioEndpoints");
 UINT count=0;require(collection->GetCount(&count),"IMMDeviceCollection::GetCount");
 if(*index>=count)throw std::runtime_error("Endpoint index outside active playback devices");
 require(collection->Item(*index,&result),"IMMDeviceCollection::Item");
 return result;
}
}
