//+--------------------------------------------------------------------------
//
// File:        ws281xoutputmanager.cpp
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// ESP32 runtime WS281x output manager. This is the small indirection layer that
// lets us change pins/channel count/live LED count without changing effect code.
//
//---------------------------------------------------------------------------

#include "globals.h"

#if USE_WS281X

#include "ws281xoutputmanager.h"

#include <algorithm>
#include <cstdint>

#include "gfxbase.h"
#include "pixel_controller.h"

#ifndef FASTLED_RMT_BUILTIN_DRIVER
#define FASTLED_RMT_BUILTIN_DRIVER false
#endif

namespace
{
    static_assert(NUM_CHANNELS <= 8, "ESP32 RMT path supports up to 8 WS281x channels");

    ColorAdjustment MakeIdentityColorAdjustment()
    {
        ColorAdjustment adjustment{};
        adjustment.premixed = CRGB::White;
        #if FASTLED_HD_COLOR_MIXING
        adjustment.color = CRGB::White;
        adjustment.brightness = 255;
        #endif
        return adjustment;
    }

    void LogRuntimeWS281xConfiguration(const DeviceConfig& config, const std::vector<std::shared_ptr<GFXBase>>& devices, const char* reason)
    {
        debugI("WS281x config (%s): path=runtime driver=%s channels=%zu matrix=%ux%u serpentine=%d leds=%zu",
               reason ? reason : "update",
               config.GetRuntimeDriverName().c_str(),
               config.GetChannelCount(),
               static_cast<unsigned>(config.GetMatrixWidth()),
               static_cast<unsigned>(config.GetMatrixHeight()),
               config.IsMatrixSerpentine(),
               config.GetActiveLEDCount());

        const auto& pins = config.GetWS281xPins();
        for (size_t channel = 0; channel < config.GetChannelCount() && channel < devices.size(); ++channel)
        {
            const auto& graphics = *devices[channel];
            debugI("WS281x channel %zu (runtime): pin=%d leds=%zu matrix=%ux%u serpentine=%d buffer=%p",
                   channel,
                   pins[channel],
                   graphics.GetLEDCount(),
                   static_cast<unsigned>(graphics.GetMatrixWidth()),
                   static_cast<unsigned>(graphics.GetMatrixHeight()),
                   graphics.IsSerpentine(),
                   graphics.leds);
        }
    }
}

WS281xOutputManager::~WS281xOutputManager()
{
    for (size_t i = 0; i < _channels.size(); ++i)
        ReleaseChannel(i);
}

bool WS281xOutputManager::RecreateChannel(size_t channelIndex, int8_t pin, size_t ledCount, String* errorMessage)
{
    ReleaseChannel(channelIndex);

    auto& state = _channels[channelIndex];
    #if FASTLED_RMT5
    state.controller = std::make_unique<WS281xRuntimeController>(
        pin,
        FASTLED_WS2812_T1,
        FASTLED_WS2812_T2,
        FASTLED_WS2812_T3,
        WS281xRuntimeController::DMA_AUTO
    );
    #else
    state.controller = std::make_unique<RmtController>(pin, FASTLED_WS2812_T1, FASTLED_WS2812_T2, FASTLED_WS2812_T3, 8, FASTLED_RMT_BUILTIN_DRIVER);
    #endif
    if (!state.controller)
    {
        if (errorMessage)
            *errorMessage = "failed to allocate WS281x output";
        ReleaseChannel(channelIndex);
        return false;
    }

    state.pin = pin;
    state.ledCount = ledCount;
    state.active = true;
    pinMode(pin, OUTPUT);
    return true;
}

void WS281xOutputManager::ReleaseChannel(size_t channelIndex)
{
    auto& state = _channels[channelIndex];

    state.controller.reset();

    if (state.pin >= 0)
    {
        pinMode(state.pin, OUTPUT);
        digitalWrite(state.pin, LOW);
    }

    state.pin = -1;
    state.ledCount = 0;
    state.active = false;
}

bool WS281xOutputManager::ApplyConfig(const DeviceConfig& config, const std::vector<std::shared_ptr<GFXBase>>& devices, String* errorMessage)
{
    if (config.GetOutputDriver() != DeviceConfig::OutputDriver::WS281x)
    {
        if (errorMessage)
            *errorMessage = "recompile needed";
        return false;
    }

    const size_t channelCount = std::min(config.GetChannelCount(), devices.size());
    const size_t ledCount = config.GetActiveLEDCount();
    const auto& pins = config.GetWS281xPins();

    // The manager owns the runtime transport objects. Rebuilding them from DeviceConfig keeps GPIO and
    // channel changes local here instead of forcing the rest of the renderer through templated FastLED paths.
    for (size_t i = 0; i < _channels.size(); ++i)
    {
        const bool shouldBeActive = i < channelCount;
        if (!shouldBeActive)
        {
            ReleaseChannel(i);
            continue;
        }

        auto& state = _channels[i];
        if (!state.active || state.pin != pins[i] || state.ledCount != ledCount)
        {
            if (!RecreateChannel(i, pins[i], ledCount, errorMessage))
                return false;
        }
    }

    _activeChannelCount = channelCount;
    _activeLEDCount = ledCount;

    if (errorMessage)
        *errorMessage = "";

    LogRuntimeWS281xConfiguration(config, devices, "apply");

    return true;
}

void WS281xOutputManager::Show(const std::vector<std::shared_ptr<GFXBase>>& devices, uint16_t pixelsDrawn, uint8_t brightness)
{
    if (_activeChannelCount == 0 || _activeLEDCount == 0)
        return;

    const size_t pixelsToShow = std::min(static_cast<size_t>(pixelsDrawn), _activeLEDCount);
    const uint8_t scale = brightness;

    for (size_t channelIndex = 0; channelIndex < _activeChannelCount && channelIndex < devices.size(); ++channelIndex)
    {
        auto& state = _channels[channelIndex];
        if (!state.active || !state.controller)
            continue;

        const auto& device = devices[channelIndex];
        std::unique_ptr<CRGB[]> outputPixels = std::make_unique<CRGB[]>(_activeLEDCount);
        for (size_t pixelIndex = 0; pixelIndex < _activeLEDCount; ++pixelIndex)
        {
            CRGB color = pixelIndex < pixelsToShow ? device->leds[pixelIndex] : CRGB::Black;
            nscale8x3_video(color.r, color.g, color.b, scale);
            outputPixels[pixelIndex] = color;
        }

        PixelController<COLOR_ORDER> pixels(reinterpret_cast<const uint8_t*>(outputPixels.get()), _activeLEDCount, MakeIdentityColorAdjustment(), DISABLE_DITHER, true, 0);
        auto iterator = pixels.as_iterator(Rgbw());
        #if FASTLED_RMT5
        state.controller->loadPixelData(iterator);
        state.controller->showPixels();
        #else
        state.controller->showPixels(iterator);
        #endif
    }
}

#endif
