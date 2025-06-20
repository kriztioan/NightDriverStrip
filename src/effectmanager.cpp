//+--------------------------------------------------------------------------
//
// File:        effectmanager.cpp
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
//    Various functions related to EffectManager and its initialization
//
// History:     Sep-26-2023         Rbergen     Extracted from effects.cpp
//---------------------------------------------------------------------------

#include <FS.h>
#include <SPIFFS.h>

#include "globals.h"
#include "systemcontainer.h"

#include "effects/strip/misceffects.h"

// Variables we need further down

extern DRAM_ATTR std::unique_ptr<EffectFactories> g_ptrEffectFactories;
extern std::map<int, JSONEffectFactory> g_JsonStarryNightEffectFactories;
DRAM_ATTR size_t g_EffectsManagerJSONBufferSize = 0;
static DRAM_ATTR size_t l_EffectsManagerJSONWriterIndex = SIZE_MAX;
static DRAM_ATTR size_t l_CurrentEffectWriterIndex = SIZE_MAX;

//
// EffectManager initialization functions
//

#if USE_HUB75

    void InitSplashEffectManager()
    {
        debugW("InitSplashEffectManager");

        g_ptrSystem->SetupEffectManager(make_shared_psram<SplashLogoEffect>(), g_ptrSystem->Devices());
    }

#endif

// Declare these here just so InitEffectsManager can refer to them. They're defined elsewhere or further down.

void LoadEffectFactories();
std::optional<JsonObjectConst> LoadEffectsJSONFile(JsonDocument& jsonDoc);
void WriteCurrentEffectIndexFile();

// InitEffectsManager
//
// Initializes the effect manager.  Reboots on failure, since it's not optional
void InitEffectsManager()
{
    debugW("InitEffectsManager...");

    LoadEffectFactories();

    l_EffectsManagerJSONWriterIndex = g_ptrSystem->JSONWriter().RegisterWriter([]()
    {
        if (!SaveToJSONFile(EFFECTS_CONFIG_FILE, g_ptrSystem->EffectManager()) && EFFECT_PERSISTENCE_CRITICAL)
            throw std::runtime_error("Effects serialization failed");
    });
    l_CurrentEffectWriterIndex = g_ptrSystem->JSONWriter().RegisterWriter(WriteCurrentEffectIndexFile);

    auto jsonDoc = CreateJsonDocument();
    auto jsonObject = LoadEffectsJSONFile(jsonDoc);

    if (jsonObject)
    {
        debugI("Creating EffectManager from JSON config");

        if (g_ptrSystem->HasEffectManager())
            g_ptrSystem->EffectManager().DeserializeFromJSON(jsonObject.value());
        else
            g_ptrSystem->SetupEffectManager(jsonObject.value(), g_ptrSystem->Devices());
    }
    else
    {
        debugI("Creating EffectManager using default effects");

        if (g_ptrSystem->HasEffectManager())
            g_ptrSystem->EffectManager().LoadDefaultEffects();
        else
            g_ptrSystem->SetupEffectManager(g_ptrSystem->Devices());
    }

    if (false == g_ptrSystem->EffectManager().Init())
        throw std::runtime_error("Could not initialize effect manager");

    // We won't need the default factories anymore, so swipe them from memory
    g_ptrEffectFactories->ClearDefaultFactories();

    #if EFFECTS_WEB_SOCKET_ENABLED
        g_ptrSystem->EffectManager().AddEffectEventListener(g_ptrSystem->WebSocketServer());
    #endif
}

//
// EffectManager member function definitions
//

void EffectManager::SaveCurrentEffectIndex()
{
    if (g_ptrSystem->DeviceConfig().RememberCurrentEffect())
        // Default value for writer index is max value for size_t, so nothing will happen if writer has not yet been registered
        g_ptrSystem->JSONWriter().FlagWriter(l_CurrentEffectWriterIndex);
}

bool EffectManager::ReadCurrentEffectIndex(size_t& index)
{
    File file = SPIFFS.open(CURRENT_EFFECT_CONFIG_FILE);
    bool readIndex = false;

    if (file)
    {
        if (file.size() > 0)
        {
            debugI("Attempting to read file %s", CURRENT_EFFECT_CONFIG_FILE);

            auto valueString = file.readString();

            if (!valueString.isEmpty())
            {
                index = strtoul(valueString.c_str(), nullptr, 10);
                readIndex = true;
            }
        }

        file.close();
    }

    return readIndex;
}

void EffectManager::LoadJSONAndMissingEffects(const JsonArrayConst& effectsArray)
{
    std::set<int> loadedEffectNumbers;

    // Create effects from JSON objects, using the respective factories in g_EffectFactories
    auto& jsonFactories = g_ptrEffectFactories->GetJSONFactories();

    for (auto effectObject : effectsArray)
    {
        int effectNumber = effectObject[PTY_EFFECTNR];
        auto factoryEntry = jsonFactories.find(effectNumber);

        if (factoryEntry == jsonFactories.end())
            continue;

        auto pEffect = factoryEntry->second(effectObject);
        if (pEffect)
        {
            if (effectObject[PTY_COREEFFECT].as<int>())
                pEffect->MarkAsCoreEffect();

            _vEffects.push_back(pEffect);
            loadedEffectNumbers.insert(effectNumber);
        }
    }

    // Now add missing effects from the default factory list
    auto &defaultFactories = g_ptrEffectFactories->GetDefaultFactories();

    // We iterate manually, so we can use where we are as the starting point for a later inner loop
    for (auto iter = defaultFactories.begin(); iter != defaultFactories.end(); iter++)
    {
        int effectNumber = iter->EffectNumber();

        // If we've already loaded this effect (number) from JSON, we can move on to check the next one
        if (loadedEffectNumbers.count(effectNumber))
            continue;

        // We found an effect (number) in the default list that we have not yet loaded from JSON.
        //   So, we go through the rest of the default factory list to create and add to our effects
        //   list all instances of this effect.
        std::for_each(iter, defaultFactories.end(), [&](const EffectFactories::NumberedFactory& numberedFactory)
            {
                if (numberedFactory.EffectNumber() == effectNumber)
                    ProduceAndLoadDefaultEffect(numberedFactory);
            }
        );

        // Register that we added this effect number, so we don't add the respective effects more than once
        loadedEffectNumbers.insert(effectNumber);
    }
}

std::shared_ptr<LEDStripEffect> EffectManager::CopyEffect(size_t index)
{
    if (index >= _vEffects.size())
    {
        debugW("Invalid index for CopyEffect");
        return nullptr;
    }

    auto& sourceEffect = _vEffects[index];

    auto jsonEffectFactories = g_ptrEffectFactories->GetJSONFactories();
    auto factoryEntry = jsonEffectFactories.find(sourceEffect->EffectNumber());

    if (factoryEntry == jsonEffectFactories.end())
        return nullptr;

    auto jsonDoc = CreateJsonDocument();
    auto jsonObject = jsonDoc.to<JsonObject>();

    if (!sourceEffect->SerializeToJSON(jsonObject))
    {
        debugE("Could not serialize effect %s to JSON", sourceEffect->FriendlyName().c_str());
        return nullptr;
    }

    auto copiedEffect = factoryEntry->second(jsonDoc.as<JsonObjectConst>());

    if (!copiedEffect)
        return nullptr;

    copiedEffect->SetEnabled(false);

    return copiedEffect;
}

//
// Helper functions related to JSON persistence
//

void SaveEffectManagerConfig()
{
    debugV("Saving effect manager config...");
    // Default value for writer index is max value for size_t, so nothing will happen if writer has not yet been registered
    g_ptrSystem->JSONWriter().FlagWriter(l_EffectsManagerJSONWriterIndex);
}

void RemoveEffectManagerConfig()
{
    RemoveJSONFile(EFFECTS_CONFIG_FILE);
    // We take the liberty of also removing the file with the current effect config index
    SPIFFS.remove(CURRENT_EFFECT_CONFIG_FILE);
}

void WriteCurrentEffectIndexFile()
{
    SPIFFS.remove(CURRENT_EFFECT_CONFIG_FILE);

    File file = SPIFFS.open(CURRENT_EFFECT_CONFIG_FILE, FILE_WRITE);

    if (!file)
    {
        debugE("Unable to open file %s for writing!", CURRENT_EFFECT_CONFIG_FILE);
        return;
    }

    auto bytesWritten = file.print(g_ptrSystem->EffectManager().GetCurrentEffectIndex());
    debugI("Number of bytes written to file %s: %zu", CURRENT_EFFECT_CONFIG_FILE, bytesWritten);

    file.flush();
    file.close();

    if (bytesWritten == 0)
    {
        debugE("Unable to write to file %s!", CURRENT_EFFECT_CONFIG_FILE);
        SPIFFS.remove(CURRENT_EFFECT_CONFIG_FILE);
    }
}

// Helper function to create a StarryNightEffect from JSON.
//   It picks the actual effect factory from g_JsonStarryNightEffectFactories based on the star type number in the JSON blob.
std::shared_ptr<LEDStripEffect> CreateStarryNightEffectFromJSON(const JsonObjectConst& jsonObject)
{
    auto entry = g_JsonStarryNightEffectFactories.find(jsonObject[PTY_STARTYPENR]);

    return entry != g_JsonStarryNightEffectFactories.end()
        ? entry->second(jsonObject)
        : nullptr;
}

//
// Other helper functions
//

#if ENABLE_AUDIO

#include "effects/matrix/spectrumeffects.h"

// GetSpectrumAnalyzer
//
// A little factory that makes colored spectrum analyzers

std::shared_ptr<LEDStripEffect> GetSpectrumAnalyzer(CRGB color)
{
    CHSV hueColor = rgb2hsv_approximate(color);
    CRGB color2 = CRGB(CHSV(hueColor.hue + 64, 255, 255));
    auto object = make_shared_psram<SpectrumAnalyzerEffect>("Spectrum Clr", 24, CRGBPalette16(color, color2), true);
    if (object->Init(g_ptrSystem->Devices()))
        return object;
    throw std::runtime_error("Could not initialize new spectrum analyzer, one color version!");
}

#endif

#include "effects/strip/fireeffect.h"

bool EffectManager::Init()
{
    for (const auto & _vEffect : _vEffects)
    {
        debugV("About to init effect %s", _vEffect->FriendlyName().c_str());
        if (false == _vEffect->Init(_gfx))
        {
            debugW("Could not initialize effect: %s\n", _vEffect->FriendlyName().c_str());
            return false;
        }
        debugV("Loaded Effect: %s", _vEffect->FriendlyName().c_str());
    }
    debugV("First Effect: %s", GetCurrentEffectName().c_str());

    if (g_ptrSystem->DeviceConfig().ApplyGlobalColors())
        ApplyGlobalPaletteColors();

    return true;
}

bool EffectManager::ShowVU(bool bShow)
{
    auto& deviceConfig = g_ptrSystem->DeviceConfig();
    bool bResult = deviceConfig.ShowVUMeter();
    debugI("Setting ShowVU to %d\n", bShow);
    deviceConfig.SetShowVUMeter(bShow);

    // Erase any exising pixels since effects don't all clear each frame
    if (!bShow)
        _gfx[0]->setPixelsF(0, MATRIX_WIDTH, CRGB::Black);

    return bResult;
}

bool EffectManager::IsVUVisible() const
{
    return g_ptrSystem->DeviceConfig().ShowVUMeter() && GetCurrentEffect().CanDisplayVUMeter();
}


void EffectManager::ClearRemoteColor(bool retainRemoteEffect)
{
    if (!retainRemoteEffect)
        _tempEffect = nullptr;

    #if (USE_HUB75)
        g()->PausePalette(false);
    #endif

    g_ptrSystem->DeviceConfig().ClearApplyGlobalColors();
}

void EffectManager::ApplyGlobalColor(CRGB color) const
{
    debugI("Setting Global Color: %08X\n", (uint32_t) color);

    auto& deviceConfig = g_ptrSystem->DeviceConfig();
    deviceConfig.SetColorSettings(color, deviceConfig.GlobalColor());

    ApplyGlobalPaletteColors();
}

void EffectManager::ApplyGlobalPaletteColors() const
{
    #if (USE_HUB75)
        auto  pMatrix = g();
        auto& deviceConfig = g_ptrSystem->DeviceConfig();
        auto& globalColor = deviceConfig.GlobalColor();
        auto& secondColor = deviceConfig.SecondColor();

        // If the two colors are the same, we just shift the palette by 64 degrees to create a palette
        // based from where those colors sit on the spectrum
        if (secondColor == globalColor)
        {
            CHSV hsv = rgb2hsv_approximate(globalColor);
            pMatrix->setPalette(CRGBPalette16(globalColor, CRGB(CHSV(hsv.hue + 64, 255, 255))));
        }
        else
        {
            // But if we have two different colors, we create a palette spread between them
            pMatrix->setPalette(CRGBPalette16(secondColor, globalColor));
        }

        pMatrix->PausePalette(true);
    #endif
}
