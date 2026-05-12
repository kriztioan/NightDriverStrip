#pragma once

//+--------------------------------------------------------------------------
//
// File:        cct_effects.h
//
// NightDriverStrip - (c) 2026 Plummer's Software LLC.  All Rights Reserved.
//
// Description:
//
//   Reference effects that drive the CCT whites plane (cool-white /
//   warm-white) directly via GFXBase::setPixelCCT(). Useful as a known-
//   good test on 4-channel SK6812 strips and future 5-channel SM16825/WS2805
//   strips. On plain 3-channel WS2812 builds setPixelCCT() is a no-op
//   (whites plane isn't allocated), so the effect falls back to drawing
//   a Kelvin-tinted CRGB approximation through the RGB channels - the strip
//   stays useful even where the dedicated white LED isn't present.
//
//---------------------------------------------------------------------------

#include "effectsupport.h"

#include <algorithm>

// WarmGlowEffect
//
// Lights every pixel with a configurable color temperature and brightness.
// The default 4000K is neutral-ish white. On 4-channel SK6812 RGBW hardware
// Kelvin only expresses intent: both CW and WW components collapse into the
// one physical W channel in PixelFormat, so 2700K and 6500K are not distinct
// unless RGB fallback is being used. Future 5-channel formats can render the
// CW/WW split directly.
//
// On builds without a whites plane, falls back to filling RGB with a coarse
// Kelvin approximation so warm and cool presets are visibly distinct.

class WarmGlowEffect : public EffectWithId<WarmGlowEffect>
{
protected:

    uint16_t _kelvin     = 4000;     // default: neutral-ish single-white intent
    uint8_t  _brightness = 200;      // 0..255 white intensity

    static String NameForKelvin(uint16_t kelvin)
    {
        if (kelvin <= 3000)
            return "Warm White Glow";
        if (kelvin >= 6000)
            return "Cool White Glow";
        return "White Glow";
    }

    static CRGB ApproximateRgbForKelvin(uint16_t kelvin, uint8_t brightness)
    {
        constexpr uint16_t kKelvinWarm = 2700;
        constexpr uint16_t kKelvinCool = 6500;
        constexpr uint16_t kKelvinSpan = kKelvinCool - kKelvinWarm;

        const uint16_t clamped = std::min<uint16_t>(std::max<uint16_t>(kelvin, kKelvinWarm), kKelvinCool);
        const uint16_t coolPart = static_cast<uint16_t>(clamped - kKelvinWarm);
        const uint16_t warmPart = static_cast<uint16_t>(kKelvinSpan - coolPart);

        const auto mix = [&](uint8_t warm, uint8_t cool) -> uint8_t {
            const uint32_t channel = (static_cast<uint32_t>(warm) * warmPart)
                                   + (static_cast<uint32_t>(cool) * coolPart)
                                   + (kKelvinSpan / 2);
            return static_cast<uint8_t>((channel / kKelvinSpan) * brightness / 255);
        };

        return CRGB(mix(255, 205), mix(180, 225), mix(100, 255));
    }

public:

    WarmGlowEffect()
        : EffectWithId<WarmGlowEffect>(NameForKelvin(_kelvin)) {}

    WarmGlowEffect(uint16_t kelvin, uint8_t brightness)
        : EffectWithId<WarmGlowEffect>(NameForKelvin(kelvin)),
          _kelvin(kelvin),
          _brightness(brightness) {}

    WarmGlowEffect(const JsonObjectConst& jsonObject)
        : EffectWithId<WarmGlowEffect>(jsonObject),
          _kelvin    (jsonObject["kelvin"]     | 4000),
          _brightness(jsonObject["brightness"] |  200)
    {
    }

    bool SerializeToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();
        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeToJSON(root);
        jsonDoc["kelvin"]     = _kelvin;
        jsonDoc["brightness"] = _brightness;
        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    void Draw() override
    {
        // We hit each device's whites plane directly because the existing
        // setPixelOnAllChannels helpers only know about CRGB. Effects that
        // want CCT just iterate _GFX themselves; this is a deliberately
        // small surface for the first cut.
        for (auto& device : _GFX)
        {
            if (device->whites)
            {
                // Whites plane available: drive the dedicated white LED(s).
                for (size_t i = 0; i < _cLEDs; ++i)
                    device->setPixelCCT(static_cast<int>(i), _kelvin, _brightness);

                // Make sure RGB stays dark so the white doesn't get tinted
                // by leftover color from a previous effect.
                fill_solid(device->leds, _cLEDs, CRGB::Black);
            }
            else
            {
                // No whites plane (plain WS2812 build): fall back to a
                // coarse Kelvin approximation so the strip still shows
                // something visible and warm/cool presets remain distinct.
                const CRGB approx = ApproximateRgbForKelvin(_kelvin, _brightness);
                fill_solid(device->leds, _cLEDs, approx);
            }
        }
    }

    size_t DesiredFramesPerSecond() const override { return 5; }   // static scene; no need to redraw fast
};
