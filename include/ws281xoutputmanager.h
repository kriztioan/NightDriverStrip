#pragma once

//+--------------------------------------------------------------------------
//
// File:        ws281xoutputmanager.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// Runtime WS281x output manager. Keeps FastLED for effect math and CRGB buffers,
// but moves GPIO/channel binding into a reconfigurable ESP32 output layer.
//
//---------------------------------------------------------------------------

#include "globals.h"

#if USE_WS281X

#include <array>
#include <memory>
#include <vector>

#include "deviceconfig.h"
#if FASTLED_RMT5
#include "platforms/esp/32/rmt_5/idf5_rmt.h"
#else
#include "platforms/esp/32/rmt_4/idf4_rmt.h"
#endif

class GFXBase;

#if FASTLED_RMT5
using WS281xRuntimeController = fl::RmtController5;
#else
using WS281xRuntimeController = RmtController;
#endif

class WS281xOutputManager
{
    struct ChannelState
    {
        int8_t pin = -1;
        size_t ledCount = 0;
        bool active = false;
        std::unique_ptr<WS281xRuntimeController> controller;
    };

    std::array<ChannelState, NUM_CHANNELS> _channels{};
    size_t _activeChannelCount = 0;
    size_t _activeLEDCount = 0;

    bool RecreateChannel(size_t channelIndex, int8_t pin, size_t ledCount, String* errorMessage);
    void ReleaseChannel(size_t channelIndex);

  public:
    WS281xOutputManager() = default;
    ~WS281xOutputManager();

    bool ApplyConfig(const DeviceConfig& config, const std::vector<std::shared_ptr<GFXBase>>& devices, String* errorMessage = nullptr);
    void Show(const std::vector<std::shared_ptr<GFXBase>>& devices, uint16_t pixelsDrawn, uint8_t brightness);

    size_t GetActiveChannelCount() const { return _activeChannelCount; }
    size_t GetActiveLEDCount() const { return _activeLEDCount; }
};

#endif
