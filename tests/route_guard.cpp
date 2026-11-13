// SPDX-License-Identifier: MIT
#include "route_guard.hpp"
#include <stdexcept>
#include <iostream>
static void require(bool b){if(!b)throw std::runtime_error("Route guard check failed");}
int main(){
 using namespace tas;
 const auto a=audioEndpointToken(L"{0.0.0.00000000}.{B6677D77-0000-1111-2222-ABCDEF123456}");
 const auto b=audioEndpointToken(L"{0.0.0.00000000}.{B6677D77-0000-1111-2222-ABCDEF123457}");
 require(a!=0 && b!=0 && a!=b);
 require(a==audioEndpointToken(L"{0.0.0.00000000}.{B6677D77-0000-1111-2222-ABCDEF123456}"));
 require(!inputOutputRouteSafe(a,a));
 require(inputOutputRouteSafe(a,b));
 require(inputOutputRouteSafe(0,a));
 require(audioEndpointToken(L"")==0);
 std::cout<<"Stable endpoint token and anti-loop guard OK\n";
}
