//+--------------------------------------------------------------------------
//
// File:        ws281xgfx.cpp
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
// Description:
//
//    Code for handling LED strips with the FastLED library
//
// History:     Jul-22-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <algorithm>
#include <cstring>

#include "deviceconfig.h"
#include "effectmanager.h"
#include "systemcontainer.h"
#include "values.h"
#include "ws281xgfx.h"
#if USE_WS281X
#include "ws281xoutputmanager.h"
#endif

namespace
{
    void LogWS281xConfiguration(const DeviceConfig& deviceConfig, const std::vector<std::shared_ptr<GFXBase>>& devices, bool compiledTransport, const char* reason)
    {
        debugI("WS281x config (%s): path=%s driver=%s channels=%zu matrix=%ux%u serpentine=%d leds=%zu",
               reason ? reason : "update",
               compiledTransport ? "compiled" : "runtime",
               deviceConfig.GetRuntimeDriverName().c_str(),
               deviceConfig.GetChannelCount(),
               static_cast<unsigned>(deviceConfig.GetMatrixWidth()),
               static_cast<unsigned>(deviceConfig.GetMatrixHeight()),
               deviceConfig.IsMatrixSerpentine(),
               deviceConfig.GetActiveLEDCount());

        const auto& configuredPins = deviceConfig.GetWS281xPins();
        for (size_t channel = 0; channel < deviceConfig.GetChannelCount() && channel < devices.size(); ++channel)
        {
            const auto pin = compiledTransport ? DeviceConfig::GetCompiledPins()[channel] : configuredPins[channel];
            const auto& graphics = *devices[channel];
            debugI("WS281x channel %zu (%s): pin=%d leds=%zu matrix=%ux%u serpentine=%d buffer=%p",
                   channel,
                   compiledTransport ? "compiled" : "runtime",
                   pin,
                   graphics.GetLEDCount(),
                   static_cast<unsigned>(graphics.GetMatrixWidth()),
                   static_cast<unsigned>(graphics.GetMatrixHeight()),
                   graphics.IsSerpentine(),
                   graphics.leds);
        }
    }

    void RebindCompiledLEDs(const std::vector<std::shared_ptr<GFXBase>>& devices)
    {
        for (size_t channel = 0; channel < devices.size() && channel < static_cast<size_t>(NUM_CHANNELS) && channel < static_cast<size_t>(FastLED.count()); ++channel)
            FastLED[channel].setLeds(devices[channel]->leds, devices[channel]->GetLEDCount());
    }

    void AddCompiledLEDs(std::vector<std::shared_ptr<GFXBase>>& devices)
    {
        #if FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED
            #define ADD_CHANNEL(channel) \
                debugI("Adding %zu LEDs to pin %d from channel %d on FastLED.", devices[channel]->GetLEDCount(), LED_PIN ## channel, channel); \
                FastLED.addLeds<WS2812, LED_PIN ## channel, COLOR_ORDER>(devices[channel]->leds, devices[channel]->GetLEDCount()).setRgbw(Rgbw(kRGBWDefaultColorTemp, FASTLED_EXPERIMENTAL_ESP32_RGBW_MODE )); \
                pinMode(LED_PIN ## channel, OUTPUT)
        #else
            #define ADD_CHANNEL(channel) \
                debugI("Adding %zu LEDs to pin %d from channel %d on FastLED.", devices[channel]->GetLEDCount(), LED_PIN ## channel, channel); \
                FastLED.addLeds<WS2812B, LED_PIN ## channel, COLOR_ORDER>(devices[channel]->leds, devices[channel]->GetLEDCount()); \
                pinMode(LED_PIN ## channel, OUTPUT)
        #endif

        debugI("Adding LEDs to FastLED...");

        #if NUM_CHANNELS >= 1 && LED_PIN0 >= 0
            ADD_CHANNEL(0);
        #endif
        #if NUM_CHANNELS >= 2 && LED_PIN1 >= 0
            ADD_CHANNEL(1);
        #endif
        #if NUM_CHANNELS >= 3 && LED_PIN2 >= 0
            ADD_CHANNEL(2);
        #endif
        #if NUM_CHANNELS >= 4 && LED_PIN3 >= 0
            ADD_CHANNEL(3);
        #endif
        #if NUM_CHANNELS >= 5 && LED_PIN4 >= 0
            ADD_CHANNEL(4);
        #endif
        #if NUM_CHANNELS >= 6 && LED_PIN5 >= 0
            ADD_CHANNEL(5);
        #endif
        #if NUM_CHANNELS >= 7 && LED_PIN6 >= 0
            ADD_CHANNEL(6);
        #endif
        #if NUM_CHANNELS >= 8 && LED_PIN7 >= 0
            ADD_CHANNEL(7);
        #endif

        #ifdef POWER_LIMIT_MW
            set_max_power_in_milliwatts(POWER_LIMIT_MW);
        #endif

        #undef ADD_CHANNEL
    }
}

WS281xGFX::WS281xGFX(size_t w, size_t h) : GFXBase(w, h)
{
    debugV("Creating Device of size %zu x %zu", w, h);
    leds = static_cast<CRGB *>(calloc(w * h, sizeof(CRGB)));
    if(!leds)
        throw std::runtime_error("Unable to allocate LEDs in WS281xGFX");
}

WS281xGFX::~WS281xGFX()
{
    free(leds);
    leds = nullptr;
}

void WS281xGFX::ApplyCompiledTransportConfiguration(const DeviceConfig& deviceConfig, const std::vector<std::shared_ptr<GFXBase>>& devices, const char* reason)
{
    RebindCompiledLEDs(devices);
    LogWS281xConfiguration(deviceConfig, devices, true, reason);
}

void WS281xGFX::ConfigureTopology(size_t width, size_t height, bool serpentine)
{
    const auto newLEDCount = width * height;
    if (newLEDCount != GetLEDCount())
    {
        auto* newPixels = static_cast<CRGB*>(calloc(newLEDCount, sizeof(CRGB)));
        if (!newPixels)
            throw std::runtime_error("Unable to resize LEDs in WS281xGFX");

        if (leds)
        {
            memcpy(newPixels, leds, std::min(GetLEDCount(), newLEDCount) * sizeof(CRGB));
            free(leds);
        }

        leds = newPixels;
    }

    GFXBase::ConfigureTopology(width, height, serpentine);
}

void WS281xGFX::InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices)
{
    // We don't support more than 8 parallel channels
    #if NUM_CHANNELS > 8
        #error The maximum value of NUM_CHANNELS (number of parallel channels) is 8
    #endif

    const auto& deviceConfig = g_ptrSystem->GetDeviceConfig();

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating WS281xGFX for channel %d", i);
        auto device = std::make_shared<WS281xGFX>(deviceConfig.GetMatrixWidth(), deviceConfig.GetMatrixHeight());
        device->ConfigureTopology(deviceConfig.GetMatrixWidth(), deviceConfig.GetMatrixHeight(), deviceConfig.IsMatrixSerpentine());
        devices.push_back(device);
    }

    // Keep the compiled FastLED transport alive for the default boot path so existing boards retain the
    // same hardware behavior. The runtime manager only takes over when the user truly changes pins/channels.
    if (deviceConfig.UsesCompiledWS281xTransport())
    {
        AddCompiledLEDs(devices);
        ApplyCompiledTransportConfiguration(deviceConfig, devices, "init");
    }
    else
    {
        #if USE_WS281X
        auto& outputManager = g_ptrSystem->SetupWS281xOutputManager();
        String errorMessage;
        if (!outputManager.ApplyConfig(deviceConfig, devices, &errorMessage))
            throw std::runtime_error(errorMessage.c_str());
        #endif
    }
}

// PostProcessFrame
//
// PostProcessFrame sends the data to the LED strip.  If it's fewer than the size of the strip, we only send that many.

void WS281xGFX::PostProcessFrame(uint16_t localPixelsDrawn, uint16_t wifiPixelsDrawn)
{
    auto pixelsDrawn = wifiPixelsDrawn > 0 ? wifiPixelsDrawn : localPixelsDrawn;

    // If we drew no pixels, there's nothing to post process
    if (pixelsDrawn == 0)
    {
        debugV("Frame draw ended without any pixels drawn.");
        return;
    }

    #if USE_WS281X
    auto& effectManager = g_ptrSystem->GetEffectManager();
    const auto& deviceConfig = g_ptrSystem->GetDeviceConfig();
    if (deviceConfig.UsesCompiledWS281xTransport())
    {
        for (int i = 0; i < NUM_CHANNELS; i++)
        {
            auto& graphics = effectManager.g(i);
            const auto ledCount = graphics.GetLEDCount();
            const auto activePixels = std::min<size_t>(pixelsDrawn, ledCount);

            fadeLightBy(graphics.leds, activePixels, 255 - deviceConfig.GetBrightness());
            if (activePixels < ledCount)
                fill_solid(graphics.leds + activePixels, ledCount - activePixels, CRGB::Black);
        }
        FastLED.show(g_Values.Fader);
        g_Values.FPS = FastLED.getFPS();
    }
    else
    {
        if (!g_ptrSystem->HasWS281xOutputManager())
        {
            static auto lastDrawTime = millis();
            g_Values.FPS = 1000.0 / max(1UL, millis() - lastDrawTime);
            lastDrawTime = millis();
            return;
        }

        static auto lastDrawTime = millis();
        auto& outputManager = g_ptrSystem->GetWS281xOutputManager();
        const auto targetBrightness = std::min<uint8_t>(deviceConfig.GetBrightness(), g_Values.Fader);
        outputManager.Show(g_ptrSystem->GetDevices(), pixelsDrawn, targetBrightness);
        g_Values.FPS = 1000.0 / max(1UL, millis() - lastDrawTime);
        lastDrawTime = millis();
    }
    #ifdef POWER_LIMIT_MW
        g_Values.Brite = 100.0 * calculate_max_brightness_for_power_mW(deviceConfig.GetBrightness(), POWER_LIMIT_MW) / 255;
    #else
        g_Values.Brite = 100.0 * deviceConfig.GetBrightness() / 255;
    #endif
    g_Values.Watts = calculate_unscaled_power_mW(effectManager.g().leds, pixelsDrawn) / 1000; // 1000 for mw->W
    #endif
}

#if HEXAGON

HexagonGFX::HexagonGFX(size_t numLeds) : WS281xGFX(numLeds, 1)
{
}

void HexagonGFX::InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices)
{
    // We don't support more than 8 parallel channels
    #if NUM_CHANNELS > 8
        #error The maximum value of NUM_CHANNELS (number of parallel channels) is 8
    #endif

    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        debugW("Allocating HexagonGFX for channel %d", i);
        devices.push_back(std::make_shared<HexagonGFX>(NUM_LEDS));
    }

    #if USE_WS281X
    auto& outputManager = g_ptrSystem->SetupWS281xOutputManager();
    String errorMessage;
    if (!outputManager.ApplyConfig(g_ptrSystem->GetDeviceConfig(), devices, &errorMessage))
        throw std::runtime_error(errorMessage.c_str());
    #endif
}

// filHexRing
//
// Fills a ring around the hexagon, inset by the indent specified and in the color provided

void HexagonGFX::fillHexRing(uint16_t indent, CRGB color)
{
    for (int i = indent; i < getRowWidth(indent)-indent; ++i)
        setPixel(getStartIndexOfRow(indent) + i, color);

    // Iterate over all rows
    for (int row = indent; row < HEX_MAX_DIMENSION - indent; ++row)
    {
        // Get the start index of the current row
        int startIndex = getStartIndexOfRow(row);

        setPixel(startIndex + indent, color);                           // first pixel
        setPixel(startIndex + getRowWidth(row) - 1 - indent, color);    // last pixel
    }

    for (int i = indent; i < getRowWidth(HEX_MAX_DIMENSION-indent-1)-indent; ++i)
        setPixel(getStartIndexOfRow(HEX_MAX_DIMENSION-indent-1) + i, color);
}

#endif
