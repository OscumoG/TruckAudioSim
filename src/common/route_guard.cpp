// SPDX-License-Identifier: MIT
#include "route_guard.hpp"
#include <stdexcept>
namespace tas {
uint32_t audioEndpointToken(std::wstring_view id){
 if(id.empty())return 0;
 uint32_t hash=2166136261u;
 // Windows IMMDevice IDs are UTF-16: always hash both bytes per code unit.
 for(wchar_t c:id){
  if(static_cast<uint32_t>(c)>0xffffu)throw std::invalid_argument("Expected a Windows UTF-16 endpoint ID");
  const uint16_t u=static_cast<uint16_t>(c);
  hash=(hash ^ static_cast<uint8_t>(u))*16777619u;
  hash=(hash ^ static_cast<uint8_t>(u>>8))*16777619u;
 }
 return hash?hash:1; // reserve zero to mean 'unspecified'
}
bool inputOutputRouteSafe(uint32_t source, uint32_t output) noexcept {
 return source==0 || output==0 || source!=output;
}
}
