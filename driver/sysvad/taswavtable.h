// Copyright (c) Microsoft Corporation.
// Adapted from Microsoft SysVAD under the Microsoft Public License.
#pragma once

// The render pin accepts PCM16. WASAPI exposes the shared-mode mix to
// TruckAudioSim, normally as float32 with the negotiated speaker mask.
constexpr ULONG TasSampleRate = 48000;
constexpr USHORT TasSampleBits = 16;

static constexpr KSDATAFORMAT_WAVEFORMATEXTENSIBLE TasPcmFormat(USHORT channels, ULONG mask)
{
    const USHORT alignment = channels * sizeof(SHORT);
    return {
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), 0, alignment, 0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE, channels, TasSampleRate, TasSampleRate * alignment,
                alignment, TasSampleBits, sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            { TasSampleBits }, mask, STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    };
}

static KSDATAFORMAT_WAVEFORMATEXTENSIBLE TasFormats[] =
{
    TasPcmFormat(2, KSAUDIO_SPEAKER_STEREO),
    TasPcmFormat(6, KSAUDIO_SPEAKER_5POINT1_SURROUND),
    TasPcmFormat(6, KSAUDIO_SPEAKER_5POINT1),
    TasPcmFormat(8, KSAUDIO_SPEAKER_7POINT1_SURROUND)
};

static MODE_AND_DEFAULT_FORMAT TasModes[] =
{
    { STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT, &TasFormats[0].DataFormat },
    { STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW, &TasFormats[0].DataFormat }
};

static PIN_DEVICE_FORMATS_AND_MODES TasPinFormats[] =
{
    { SystemRenderPin, TasFormats, ARRAYSIZE(TasFormats), TasModes, ARRAYSIZE(TasModes) },
    { BridgePin, nullptr, 0, nullptr, 0 }
};

static constexpr KSDATARANGE_AUDIO TasPcmRange(ULONG channels)
{
    return {
        {
            sizeof(KSDATARANGE_AUDIO), KSDATARANGE_ATTRIBUTES, 0, 0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        channels, TasSampleBits, TasSampleBits, TasSampleRate, TasSampleRate
    };
}

static KSDATARANGE_AUDIO TasStereoRange = TasPcmRange(2);
static KSDATARANGE_AUDIO TasSurround51Range = TasPcmRange(6);
static KSDATARANGE_AUDIO TasSurround71Range = TasPcmRange(8);

static KSDATARANGE TasBridgeRange =
{
    sizeof(KSDATARANGE), 0, 0, 0,
    STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
    STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
    STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
};

static PKSDATARANGE TasRenderRanges[] =
{
    &TasStereoRange.DataRange, reinterpret_cast<PKSDATARANGE>(&PinDataRangeAttributeList),
    &TasSurround51Range.DataRange, reinterpret_cast<PKSDATARANGE>(&PinDataRangeAttributeList),
    &TasSurround71Range.DataRange, reinterpret_cast<PKSDATARANGE>(&PinDataRangeAttributeList)
};

static PKSDATARANGE TasBridgeRanges[] = { &TasBridgeRange };

static PCPIN_DESCRIPTOR TasWavePins[] =
{
    {
        4, 4, 0, nullptr,
        {
            0, nullptr, 0, nullptr, ARRAYSIZE(TasRenderRanges), TasRenderRanges,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_SINK, &KSCATEGORY_AUDIO, nullptr, 0
        }
    },
    {
        0, 0, 0, nullptr,
        {
            0, nullptr, 0, nullptr, ARRAYSIZE(TasBridgeRanges), TasBridgeRanges,
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, nullptr, 0
        }
    }
};

static PCNODE_DESCRIPTOR TasNodes[] = { { 0, nullptr, &KSNODETYPE_DAC, nullptr } };

static PCCONNECTION_DESCRIPTOR TasConnections[] =
{
    { PCFILTER_NODE, KSPIN_WAVE_RENDER3_SINK_SYSTEM, 0, 1 },
    { 0, 0, PCFILTER_NODE, KSPIN_WAVE_RENDER3_SOURCE }
};

static PCPROPERTY_ITEM TasWaveProperties[] =
{
    {
        &KSPROPSETID_Pin, KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT, PropertyHandler_WaveFilter
    },
    {
        &KSPROPSETID_Pin, KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, PropertyHandler_WaveFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(TasWaveAutomation, TasWaveProperties);

static PCFILTER_DESCRIPTOR TasWaveFilter =
{
    0, &TasWaveAutomation, sizeof(PCPIN_DESCRIPTOR), ARRAYSIZE(TasWavePins), TasWavePins,
    sizeof(PCNODE_DESCRIPTOR), ARRAYSIZE(TasNodes), TasNodes,
    ARRAYSIZE(TasConnections), TasConnections, 0, nullptr
};
