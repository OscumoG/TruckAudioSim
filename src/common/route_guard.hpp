// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string_view>
namespace tas {
// A non-secret diagnostic token derived from the stable Windows endpoint ID.
// 0 means 'not supplied' (older senders / WAV input).
// This guards against accidental audio feedback, NOT hostile senders.
uint32_t audioEndpointToken(std::wstring_view id);
bool inputOutputRouteSafe(uint32_t sourceToken, uint32_t outputToken) noexcept;
}
