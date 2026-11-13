// SPDX-License-Identifier: MIT
#pragma once
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <optional>
#include <cstdint>
#include <string>
namespace tas {
// Indices can change when Windows devices connect/disconnect. Use stable IDs
// in persistent configuration.
void printRenderDevices(IMMDeviceEnumerator* devices);
std::wstring renderDeviceId(IMMDevice* device);
Microsoft::WRL::ComPtr<IMMDevice> selectRenderDevice(
    IMMDeviceEnumerator* devices, std::optional<uint32_t> index,
    const std::wstring& stableId = L"");
}
