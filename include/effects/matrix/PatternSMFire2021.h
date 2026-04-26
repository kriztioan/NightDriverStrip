#pragma once

#include "effectmanager.h"

// Derived from https://editor.soulmatelights.com/gallery/388-fire2021

class PatternSMFire2021 : public EffectWithId<PatternSMFire2021>
{
  private:

    uint8_t Speed = 150; // 1-252 ...why is not 255?! // Setting
    uint8_t Scale = 9;   // 1-99 is palette and scale // Setting

    uint8_t pcnt;              // какой-то счётчик какого-то прогресса
    uint8_t deltaValue;        // просто повторно используемая переменная
    uint16_t ff_x {0} , ff_y {0} , ff_z {0} ; // большие счётчики
    uint8_t step; // какой-нибудь счётчик кадров или последовательностей операций

    const TProgmemRGBPalette16 *curPalette;

  public:

    PatternSMFire2021() : EffectWithId<PatternSMFire2021>("Fireplace") {}
    PatternSMFire2021(const JsonObjectConst &jsonObject) : EffectWithId<PatternSMFire2021>(jsonObject) {}

    size_t DesiredFramesPerSecond() const override
    {
        return 60;
    }

    void Start() override
    {
        g().Clear();
        if (Scale > 100U)
            Scale = 100U; // чтобы не было проблем при прошивке без очистки памяти
        deltaValue = Scale * 0.0899; // /100.0F * ((sizeof(palette_arr)
                                     // /sizeof(TProgmemRGBPalette16 *))-0.01F));
        deltaValue = (((Scale - 1U) % 11U + 1U));
        step = ::map(Speed * Speed, 1U, 65025U, (deltaValue - 1U) / 2U + 1U,
                   deltaValue * 18U + 44); // корректируем скорость эффекта в наш диапазон допустимых
        // deltaValue = (((Scale - 1U) % 11U + 2U) << 4U); // ширина языков пламени
        // (масштаб шума Перлина)
        deltaValue = 0.7 * deltaValue * deltaValue + 31.3; // ширина языков пламени (масштаб шума Перлина)
        pcnt = ::map(step, 1U, 255U, 20U, 128U); // nblend 3th param
    }

    void Draw() override
    {
        auto& graphics = g();
        auto* leds = graphics.leds;

        CRGB fireColors[256];
        fireColors[0] = CRGB::Black;

        for (uint16_t col = 1; col < 256; col++)
        {
            const uint8_t bri = 256 - (col * 0.2f);
            fireColors[col] = GetBlackBodyHeatColor(col / 255.0f, graphics.ColorFromCurrentPalette(0, bri)).fadeToBlackBy(255 - bri);
        }

        ff_x += step; // static uint32_t t += speed;
        for (unsigned x = 0; x < MATRIX_WIDTH; x++)
        {
            for (unsigned y = 0; y < MATRIX_HEIGHT; y++)
            {
                const int16_t bri = inoise8(x * deltaValue, (y * deltaValue) - ff_x, ff_z) - (y * (255 / MATRIX_HEIGHT));
                const uint8_t col = bri;

                // Get the flame color using the black body radiation approximation, but when the palette is paused
                // we make flame in that base color instead of the normal red
                // NightDriver mod - invert Y argument.

                nblend(leds[XY(x, MATRIX_HEIGHT - 1 - y)], bri > 0 ? fireColors[col] : CRGB::Black, pcnt);
            }
        }

        if (!random8())
            ff_z++;
    }
};
