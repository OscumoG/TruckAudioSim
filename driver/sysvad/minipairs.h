// Copyright (c) Microsoft Corporation.
// Adapted from Microsoft SysVAD under the Microsoft Public License.
#pragma once

#include "speakertopo.h"
#include "speakertoptable.h"
#include "taswavtable.h"

NTSTATUS CreateMiniportWaveRTSYSVAD(PUNKNOWN*, REFCLSID, PUNKNOWN, POOL_FLAGS, PUNKNOWN, PVOID, PENDPOINT_MINIPAIR);
NTSTATUS CreateMiniportTopologySYSVAD(PUNKNOWN*, REFCLSID, PUNKNOWN, POOL_FLAGS, PUNKNOWN, PVOID, PENDPOINT_MINIPAIR);

static PHYSICALCONNECTIONTABLE TasPhysicalConnections[] =
{
    { KSPIN_TOPO_WAVEOUT_SOURCE, KSPIN_WAVE_RENDER3_SOURCE, CONNECTIONTYPE_WAVE_OUTPUT }
};

static WCHAR TasTopologyName[] = L"TopologyTruckAudioSim";
static WCHAR TasWaveName[] = L"WaveTruckAudioSim";

static ENDPOINT_MINIPAIR TasRenderMiniports =
{
    eSpeakerDevice,
    TasTopologyName, nullptr,
    CreateMiniportTopologySYSVAD, &SpeakerTopoMiniportFilterDescriptor,
    0, nullptr,
    TasWaveName, nullptr,
    CreateMiniportWaveRTSYSVAD, &TasWaveFilter,
    0, nullptr,
    8, TasPinFormats, ARRAYSIZE(TasPinFormats),
    TasPhysicalConnections, ARRAYSIZE(TasPhysicalConnections),
    0, // No offload or hardware loopback. WASAPI captures the audio-engine mix.
    nullptr, 0, nullptr
};

static PENDPOINT_MINIPAIR g_RenderEndpoints[] = { &TasRenderMiniports };
static constexpr ULONG g_cRenderEndpoints = ARRAYSIZE(g_RenderEndpoints);
static PENDPOINT_MINIPAIR g_CaptureEndpoints[1] = { nullptr };
static constexpr ULONG g_cCaptureEndpoints = 0;
static constexpr ULONG g_MaxMiniports = 2;
