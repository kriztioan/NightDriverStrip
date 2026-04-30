//+--------------------------------------------------------------------------
//
// File:        deviceconfig.cpp
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
//    Implementation of DeviceConfig class methods
//
// History:     Apr-18-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <HTTPClient.h>
#include <algorithm>
#include <array>
#include <driver/gpio.h>
#include <memory>
#include <optional>
#include <UrlEncode.h>

#include "deviceconfig.h"
#include "effectmanager.h"
#include "jsonserializer.h"
#include "systemcontainer.h"

extern const char timezones_start[] asm("_binary_config_timezones_json_start");

namespace
{
    constexpr const char* kRecompileNeededMessage = "recompile needed";

    constexpr std::array<int8_t, NUM_CHANNELS> kCompiledWS281xPins = {
        #if NUM_CHANNELS >= 1
        LED_PIN0,
        #endif
        #if NUM_CHANNELS >= 2
        LED_PIN1,
        #endif
        #if NUM_CHANNELS >= 3
        LED_PIN2,
        #endif
        #if NUM_CHANNELS >= 4
        LED_PIN3,
        #endif
        #if NUM_CHANNELS >= 5
        LED_PIN4,
        #endif
        #if NUM_CHANNELS >= 6
        LED_PIN5,
        #endif
        #if NUM_CHANNELS >= 7
        LED_PIN6,
        #endif
        #if NUM_CHANNELS >= 8
        LED_PIN7,
        #endif
    };
}

// DeviceConfig holds, persists and loads device-wide configuration settings. Effect-specific settings should
// be managed using overrides of the respective methods in LEDStripEffect (mainly FillSettingSpecs(),
// SerializeSettingsToJSON() and SetSetting()).
//
// Adding a setting to the list of known/saved settings requires the following:
// 1. Adding the setting variable to the list at the top of the class definition
// 2. Adding a corresponding Tag to the list of static constexpr const char * strings further below
// 3. Adding a corresponding SettingSpec in the GetSettingSpecs() function
// 4. Adding logic to set a default in case the JSON load isn't possible in the DeviceConfig() constructor
//    (in deviceconfig.cpp)
// 5. Adding (de)serialization logic for the setting to the SerializeToJSON()/DeserializeFromJSON() methods
// 6. Adding a Get/Set method for the setting (and, where applicable, their implementation in deviceconfig.cpp)
// 7. If you've added an entry to secrets.example.h to define a default value for your setting then add a
//    test at the top of this file to confirm that the new #define is found. This prevents drift when users
//    have an existing tree and don't know to refresh their modified version of secrets.h.
//
// For the first 5 points, a comment has been added to the respective place in the existing code.
// Generally speaking, one will also want to add logic to the webserver to retrieve and set the setting.

void DeviceConfig::SaveToJSON() const
{
    g_ptrSystem->GetJSONWriter().FlagWriter(writerIndex);
}

std::array<int8_t, NUM_CHANNELS> DeviceConfig::GetCompiledWS281xPins()
{
    return kCompiledWS281xPins;
}

const char* DeviceConfig::DriverName(OutputDriver driver)
{
    switch (driver)
    {
        case OutputDriver::HUB75:
            return "hub75";

        case OutputDriver::WS281x:
        default:
            return "ws281x";
    }
}

bool DeviceConfig::IsHub75Build()
{
    return GetCompiledOutputDriver() == OutputDriver::HUB75;
}

void DeviceConfig::LogRuntimeConfig(const char* reason) const
{
    String activePins;
    for (size_t i = 0; i < runtimeOutputs.channelCount && i < runtimeOutputs.outputPins.size(); ++i)
    {
        if (!activePins.isEmpty())
            activePins += ',';
        activePins += String(runtimeOutputs.outputPins[i]);
    }

    debugI("Runtime config (%s): driver=%s matrix=%ux%u leds=%u serpentine=%d channels=%u audioPin=%d",
           reason,
           DriverName(runtimeOutputs.driver),
           runtimeTopology.width,
           runtimeTopology.height,
           static_cast<unsigned>(GetActiveLEDCount()),
           runtimeTopology.serpentine,
           static_cast<unsigned>(runtimeOutputs.channelCount),
           audioInputPin);
    debugI("Runtime config pins (%s): activeChannels=%s", reason, activePins.c_str());
}

DeviceConfig::DeviceConfig()
{
    runtimeTopology.serpentine = !IsHub75Build();
    runtimeOutputs.driver = GetCompiledOutputDriver();
    runtimeOutputs.channelCount = NUM_CHANNELS;
    runtimeOutputs.outputPins = GetCompiledWS281xPins();

    writerIndex = g_ptrSystem->GetJSONWriter().RegisterWriter(
        [this] { assert(SaveToJSONFile(DEVICE_CONFIG_FILE, *this)); }
    );

    auto jsonDoc = CreateJsonDocument();

    if (LoadJSONFile(DEVICE_CONFIG_FILE, jsonDoc))
    {
        debugI("Loading DeviceConfig from JSON");

        DeserializeFromJSON(jsonDoc.as<JsonObjectConst>(), true);
    }
    else
    {
        debugW("DeviceConfig could not be loaded from JSON, using defaults");

        SetTimeZone(timeZone, true);

        SaveToJSON();
    }

    LogRuntimeConfig("init");
}

bool DeviceConfig::SerializeToJSON(JsonObject& jsonObject)
{
    return SerializeToJSON(jsonObject, true);
}

bool DeviceConfig::SerializeToJSON(JsonObject& jsonObject, bool includeSensitive)
{
    auto jsonDoc = CreateJsonDocument();

    // Add serialization logic for additional settings to this code
    jsonDoc[HostnameTag] = hostname;
    jsonDoc[LocationTag] = location;
    jsonDoc[LocationIsZipTag] = locationIsZip;
    jsonDoc[CountryCodeTag] = countryCode;
    jsonDoc[TimeZoneTag] = timeZone;
    jsonDoc[Use24HourClockTag] = use24HourClock;
    jsonDoc[UseCelsiusTag] = useCelsius;
    jsonDoc[NTPServerTag] = ntpServer;
    jsonDoc[RememberCurrentEffectTag] = rememberCurrentEffect;
    jsonDoc[PowerLimitTag] = powerLimit;
    // Only serialize showVUMeter if the VU meter is enabled in the build
    #if SHOW_VU_METER
    jsonDoc[ShowVUMeterTag] = showVUMeter;
    #endif
    jsonDoc[BrightnessTag] = brightness;
    jsonDoc[GlobalColorTag] = globalColor;
    jsonDoc[ApplyGlobalColorsTag] = applyGlobalColors;
    jsonDoc[SecondColorTag] = secondColor;
    jsonDoc[AudioInputPinTag] = audioInputPin;
    jsonDoc[MatrixWidthTag] = runtimeTopology.width;
    jsonDoc[MatrixHeightTag] = runtimeTopology.height;
    jsonDoc[MatrixSerpentineTag] = runtimeTopology.serpentine;
    jsonDoc[OutputDriverTag] = DriverName(runtimeOutputs.driver);
    jsonDoc[WS281xChannelCountTag] = runtimeOutputs.channelCount;

    auto ws281xPins = jsonDoc[WS281xPinsTag].to<JsonArray>();
    for (auto pin : runtimeOutputs.outputPins)
        ws281xPins.add(pin);

    if (includeSensitive)
        jsonDoc[OpenWeatherApiKeyTag] = openWeatherApiKey;

    return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
}

bool DeviceConfig::DeserializeFromJSON(const JsonObjectConst& jsonObject)
{
    return DeserializeFromJSON(jsonObject, false);
}

bool DeviceConfig::DeserializeFromJSON(const JsonObjectConst& jsonObject, bool skipWrite)
{
    // If we're told to ignore saved config, we shouldn't touch anything
    if (IGNORE_SAVED_DEVICE_CONFIG)
        return true;

    // Add deserialization logic for additional settings to this code
    SetIfPresentIn(jsonObject, hostname, HostnameTag);
    SetIfPresentIn(jsonObject, location, LocationTag);
    SetIfPresentIn(jsonObject, locationIsZip, LocationIsZipTag);
    SetIfPresentIn(jsonObject, countryCode, CountryCodeTag);
    SetIfPresentIn(jsonObject, openWeatherApiKey, OpenWeatherApiKeyTag);
    SetIfPresentIn(jsonObject, use24HourClock, Use24HourClockTag);
    SetIfPresentIn(jsonObject, useCelsius, UseCelsiusTag);
    SetIfPresentIn(jsonObject, ntpServer, NTPServerTag);
    SetIfPresentIn(jsonObject, rememberCurrentEffect, RememberCurrentEffectTag);
    SetIfPresentIn(jsonObject, powerLimit, PowerLimitTag);
    SetIfPresentIn(jsonObject, brightness, BrightnessTag);
    // Persisted config predates the newer brightness guardrails in some installs, so treat an invalid
    // saved brightness as "unset" and fall back to the normal 100% default instead of booting dark.
    if (brightness < BRIGHTNESS_MIN || brightness > BRIGHTNESS_MAX)
        brightness = BRIGHTNESS_MAX;
    // Only deserialize showVUMeter if the VU meter is enabled in the build
    #if SHOW_VU_METER
    SetIfPresentIn(jsonObject, showVUMeter, ShowVUMeterTag);
    #endif
    SetIfPresentIn(jsonObject, globalColor, GlobalColorTag);
    SetIfPresentIn(jsonObject, applyGlobalColors, ApplyGlobalColorsTag);
    SetIfPresentIn(jsonObject, secondColor, SecondColorTag);
    if (jsonObject[AudioInputPinTag].is<int>())
    {
        const int persistedAudioInputPin = jsonObject[AudioInputPinTag].as<int>();
        auto [pinValid, _] = ValidateAudioInputPin(persistedAudioInputPin);
        audioInputPin = pinValid ? persistedAudioInputPin : GetCompiledAudioInputPin();
    }

    RuntimeConfig updated = GetRuntimeConfig();

    SetIfPresentIn(jsonObject, updated.topology.width, MatrixWidthTag);
    SetIfPresentIn(jsonObject, updated.topology.height, MatrixHeightTag);
    SetIfPresentIn(jsonObject, updated.topology.serpentine, MatrixSerpentineTag);

    if (jsonObject[OutputDriverTag].is<String>())
    {
        const auto driverName = jsonObject[OutputDriverTag].as<String>();
        if (driverName == DriverName(OutputDriver::HUB75))
            updated.outputs.driver = OutputDriver::HUB75;
        else if (driverName == DriverName(OutputDriver::WS281x))
            updated.outputs.driver = OutputDriver::WS281x;
    }

    if (jsonObject[WS281xChannelCountTag].is<size_t>())
        updated.outputs.channelCount = jsonObject[WS281xChannelCountTag].as<size_t>();

    if (jsonObject[WS281xPinsTag].is<JsonArrayConst>())
    {
        auto pinArray = jsonObject[WS281xPinsTag].as<JsonArrayConst>();
        for (size_t i = 0; i < updated.outputs.outputPins.size() && i < pinArray.size(); ++i)
        {
            if (pinArray[i].is<int>())
                updated.outputs.outputPins[i] = pinArray[i].as<int>();
        }
    }

    String runtimeConfigError;
    if (!SetRuntimeConfig(updated, true, &runtimeConfigError))
        debugW("Ignoring invalid persisted runtime config: %s", runtimeConfigError.c_str());

    if (ntpServer.isEmpty())
        ntpServer = NTP_SERVER_DEFAULT;

    if (jsonObject[TimeZoneTag].is<String>())
        return SetTimeZoneInternal(jsonObject[TimeZoneTag], true);

    if (!skipWrite)
        SaveToJSON();

    return true;
}

void DeviceConfig::RemovePersisted()
{
    RemoveJSONFile(DEVICE_CONFIG_FILE);
}

const std::vector<std::reference_wrapper<SettingSpec>>& DeviceConfig::GetSettingSpecs()
{
    if (settingSpecs.empty())
    {
        // Add SettingSpec for additional settings to this list
        settingSpecs.emplace_back(
            HostnameTag,
            "Hostname",
            "The hostname of the device. A reboot is required after changing this.",
            SettingSpec::SettingType::String
        ).EmptyAllowed = true;
        settingSpecs.emplace_back(
            LocationTag,
            "Location",
            "The location (city or postal code) where the device is located.",
            SettingSpec::SettingType::String
        );
        settingSpecs.emplace_back(
            LocationIsZipTag,
            "Location is postal code",
            "Indicates if the value for the \"Location\" setting is a postal code (yes if checked) or not.",
            SettingSpec::SettingType::Boolean
        );
        settingSpecs.emplace_back(
            CountryCodeTag,
            "Country code",
            "The <a href=\"https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2\">ISO 3166-1 alpha-2</a> country "
            "code for the country that the device is located in.",
            SettingSpec::SettingType::String
        );

        auto weatherKeySpec = settingSpecs.emplace_back(
            OpenWeatherApiKeyTag,
            "Open Weather API key",
            "The API key for the <a href=\"https://openweathermap.org/api\">Weather API provided by Open Weather Map</a>.",
            SettingSpec::SettingType::String
        );
        weatherKeySpec.HasValidation = true;
        weatherKeySpec.Access = SettingSpec::SettingAccess::WriteOnly;
        weatherKeySpec.EmptyAllowed.reset();        // Silently ignore empty value at the front-end

        settingSpecs.emplace_back(
            TimeZoneTag,
            "Time zone",
            "The timezone the device resides in, in <a href=\"https://en.wikipedia.org/wiki/Tz_database\">tz database</a> format. "
            "The list of available timezone identifiers can be found in the <a href=\"/timezones.json\">timezones.json</a> file.",
            SettingSpec::SettingType::String
        );
        settingSpecs.emplace_back(
            Use24HourClockTag,
            "Use 24 hour clock",
            "Indicates if time should be shown in 24-hour format (yes if checked) or 12-hour AM/PM format.",
            SettingSpec::SettingType::Boolean
        );
        settingSpecs.emplace_back(
            UseCelsiusTag,
            "Use degrees Celsius",
            "Indicates if temperatures should be shown in degrees Celsius (yes if checked) or degrees Fahrenheit.",
            SettingSpec::SettingType::Boolean
        );
        settingSpecs.emplace_back(
            NTPServerTag,
            "NTP server address",
            "The hostname or IP address of the NTP server to be used for time synchronization.",
            SettingSpec::SettingType::String
        );
        settingSpecs.emplace_back(
            RememberCurrentEffectTag,
            "Remember current effect",
            "Indicates if the current effect index should be saved after an effect transition, so the device resumes "
            "from the same effect when restarted. Enabling this will lead to more wear on the flash chip of your device.",
            SettingSpec::SettingType::Boolean
        );
        settingSpecs.emplace_back(
            BrightnessTag,
            "Brightness",
            "Overall brightness the connected LEDs or matrix should be run at.",
            SettingSpec::SettingType::Slider,
            BRIGHTNESS_MIN,
            BRIGHTNESS_MAX
        ).HasValidation = true;

        // Only publish the VU meter setting if the VU meter is enabled in the build
        #if SHOW_VU_METER
        settingSpecs.emplace_back(
            ShowVUMeterTag,
            "Show VU meter",
            "Used to show (checked) or hide the VU meter at the top of the matrix.",
            SettingSpec::SettingType::Boolean
        );
        #endif

        auto& powerLimitSpec = settingSpecs.emplace_back(
            PowerLimitTag,
            "Power limit",
            "The maximum power in mW that the matrix attached to the board is allowed to use. As the previous sentence implies, this "
            "setting only applies if a matrix is used.",
            SettingSpec::SettingType::Integer
        );
        powerLimitSpec.MinimumValue = POWER_LIMIT_MIN;
        powerLimitSpec.HasValidation = true;

        settingSpecs.emplace_back(
            ClearGlobalColorTag,
            "Clear global color",
            "Stop applying the global color/derived palette. This takes precedence over the \"(Re)apply global color\" checkbox.",
            SettingSpec::SettingType::Boolean
        ).Access = SettingSpec::SettingAccess::WriteOnly;
        settingSpecs.emplace_back(
            GlobalColorTag,
            "Global color",
            "Main color that is applied to all those effects that support using it.",
            SettingSpec::SettingType::Color
        );
        settingSpecs.emplace_back(
            ApplyGlobalColorsTag,
            "(Re)apply global color",
            "You can use this to \"reselect\" and apply the current global color, to force the composition of the derived "
            "global palette. This checkbox is ignored if the \"Clear global color\" checkbox is selected.",
            SettingSpec::SettingType::Boolean
        ).Access = SettingSpec::SettingAccess::WriteOnly;
        settingSpecs.emplace_back(
            SecondColorTag,
            "Second color",
            "Second color that is used to create a global palette in combination with the current global color. That palette is used "
            "by some effects. Defaults to the <em>previous</em> global color if not explicitly set.",
            SettingSpec::SettingType::Color
        );
        settingSpecs.emplace_back(
            MatrixWidthTag,
            "Matrix width",
            "Active matrix width. WS281x builds validate this by total LED capacity, so width * height must stay within the compiled LED budget.",
            SettingSpec::SettingType::PositiveBigInteger,
            1,
            GetCompiledLEDCount()
        );
        settingSpecs.emplace_back(
            MatrixHeightTag,
            "Matrix height",
            "Active matrix height. WS281x builds validate this by total LED capacity, so width * height must stay within the compiled LED budget.",
            SettingSpec::SettingType::PositiveBigInteger,
            1,
            GetCompiledLEDCount()
        );
        settingSpecs.emplace_back(
            MatrixSerpentineTag,
            "Serpentine layout",
            "Controls the logical XY mapping for strip-based matrices. HUB75 ignores this because its panel mapping is build-defined.",
            SettingSpec::SettingType::Boolean
        );
        auto& audioInputPinSpec = settingSpecs.emplace_back(
            AudioInputPinTag,
            "Audio input pin",
            "External microphone input pin. This is boot-applied today because the audio task still owns the active DMA/I2S handles once sampling starts.",
            SettingSpec::SettingType::Integer,
            -1,
            48
        );
        audioInputPinSpec.HasValidation = true;
        settingSpecs.emplace_back(
            OutputDriverTag,
            "Output driver",
            "Runtime-selected driver. If this differs from the firmware's compiled driver, the API reports recompile required.",
            SettingSpec::SettingType::String
        );
        settingSpecs.emplace_back(
            WS281xChannelCountTag,
            "WS281x channel count",
            "Number of active strip channels within the compiled maximum. Live updates are limited to WS281x builds.",
            SettingSpec::SettingType::PositiveBigInteger,
            1,
            GetCompiledChannelCount()
        );

        settingSpecReferences.insert(settingSpecReferences.end(), settingSpecs.begin(), settingSpecs.end());
    }

    return settingSpecReferences;
}

const String& DeviceConfig::GetTimeZone() const
{
    return timeZone;
}

void DeviceConfig::Set24HourClock(bool new24HourClock)
{
    SetAndSave(use24HourClock, new24HourClock);
}

void DeviceConfig::SetHostname(const String &newHostname)
{
    SetAndSave(hostname, newHostname);
}

void DeviceConfig::SetLocation(const String &newLocation)
{
    SetAndSave(location, newLocation);
}

void DeviceConfig::SetCountryCode(const String &newCountryCode)
{
    SetAndSave(countryCode, newCountryCode);
}

void DeviceConfig::SetLocationIsZip(bool newLocationIsZip)
{
    SetAndSave(locationIsZip, newLocationIsZip);
}

void DeviceConfig::SetOpenWeatherAPIKey(const String &newOpenWeatherAPIKey)
{
    SetAndSave(openWeatherApiKey, newOpenWeatherAPIKey);
}

void DeviceConfig::SetUseCelsius(bool newUseCelsius)
{
    SetAndSave(useCelsius, newUseCelsius);
}

void DeviceConfig::SetNTPServer(const String &newNTPServer)
{
    SetAndSave(ntpServer, newNTPServer);
}

void DeviceConfig::SetRememberCurrentEffect(bool newRememberCurrentEffect)
{
    SetAndSave(rememberCurrentEffect, newRememberCurrentEffect);
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateBrightness(int newBrightness)
{
    if (newBrightness < BRIGHTNESS_MIN)
        return { false, String("brightness is below minimum value of ") + BRIGHTNESS_MIN };

    if (newBrightness > BRIGHTNESS_MAX)
        return { false, String("brightness is above maximum value of ") + BRIGHTNESS_MAX };

    return { true, "" };
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateBrightness(const String& newBrightness)
{
    return ValidateBrightness(newBrightness.toInt());
}

void DeviceConfig::SetBrightness(int newBrightness)
{
    SetAndSave(brightness, uint8_t(std::clamp<int>(newBrightness, BRIGHTNESS_MIN, BRIGHTNESS_MAX)));
}

void DeviceConfig::SetShowVUMeter(bool newShowVUMeter)
{
    // We only actually persist if the VU meter is enabled in the build
    #if SHOW_VU_METER
    SetAndSave(showVUMeter, newShowVUMeter);
    #else
    showVUMeter = newShowVUMeter;
    #endif
}

DeviceConfig::ValidateResponse DeviceConfig::ValidatePowerLimit(int newPowerLimit)
{
    if (newPowerLimit < POWER_LIMIT_MIN)
        return { false, String("powerLimit is below minimum value of ") + POWER_LIMIT_MIN };

    return { true, "" };
}

DeviceConfig::ValidateResponse DeviceConfig::ValidatePowerLimit(const String& newPowerLimit)
{
    return ValidatePowerLimit(newPowerLimit.toInt());
}

void DeviceConfig::SetPowerLimit(int newPowerLimit)
{
    if (newPowerLimit >= POWER_LIMIT_MIN)
        SetAndSave(powerLimit, newPowerLimit);
}

void DeviceConfig::SetApplyGlobalColors()
{
    SetAndSave(applyGlobalColors, true);
}

void DeviceConfig::ClearApplyGlobalColors()
{
    SetAndSave(applyGlobalColors, false);
}

void DeviceConfig::SetGlobalColor(const CRGB& newGlobalColor)
{
    SetAndSave(globalColor, newGlobalColor);
}

void DeviceConfig::SetSecondColor(const CRGB& newSecondColor)
{
    SetAndSave(secondColor, newSecondColor);
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateAudioInputPin(int pin) const
{
    if (pin < -1)
        return { false, "audio input pin must be -1 or a valid GPIO" };

    if (pin == GetCompiledAudioInputPin())
        return { true, "" };

    // The settings API now separates "compiled default" from "active value". External I2S mics can
    // move their DIN pin at boot, but the M5 onboard mic path and the current ADC path are still fixed.
    if (!SupportsConfigurableAudioInputPin())
        return { false, kRecompileNeededMessage };

    if (pin == -1)
        return { true, "" };

    if (!GPIO_IS_VALID_GPIO(static_cast<gpio_num_t>(pin)))
        return { false, "audio input pin must be a valid GPIO" };

    return { true, "" };
}

void DeviceConfig::SetAudioInputPin(int newAudioInputPin)
{
    auto [isValid, _] = ValidateAudioInputPin(newAudioInputPin);
    if (!isValid)
        return;

    if (audioInputPin == newAudioInputPin)
        return;

    SetAndSave(audioInputPin, static_cast<int8_t>(newAudioInputPin));
    LogRuntimeConfig("audio input pin changed");
}

// This helper separates "apply the timezone to the running process" from "persist a user edit".
// Startup/config-load needs to set TZ immediately so localtime() is correct, but it must not
// immediately rewrite device.cfg just because we re-applied the already-persisted value.
// The timezone JSON file used by this logic is generated using tools/gen-tz-json.py
bool DeviceConfig::SetTimeZoneInternal(const String& newTimeZone, bool skipWrite)
{
    String quotedTZ = "\n\"" + newTimeZone + '"';

    const char *start = strstr(timezones_start, quotedTZ.c_str());

    // If we can't find the new timezone as a timezone name, assume it's a literal value
    if (start == nullptr)
        setenv("TZ", newTimeZone.c_str(), 1);
    // We received a timezone name, so we extract and use its timezone value
    else
    {
        start += quotedTZ.length();
        start = strchr(start, '"');
        if (start == nullptr)      // Can't actually happen unless timezone file is malformed
            return false;

        start++;
        const char *end = strchr(start, '"');
        if (end == nullptr)        // Can't actually happen unless timezone file is malformed
            return false;

        size_t length = end - start;

        std::unique_ptr<char[]> value = make_unique_psram<char[]>(length + 1);
        strncpy(value.get(), start, length);
        value[length] = 0;

        setenv("TZ", value.get(), 1);
    }

    tzset();

    timeZone = newTimeZone;
    if (!skipWrite)
        SaveToJSON();

    return true;
}

bool DeviceConfig::SetTimeZone(const String& newTimeZone, bool skipWrite)
{
    return SetTimeZoneInternal(newTimeZone, skipWrite);
}

#if ENABLE_WIFI
DeviceConfig::ValidateResponse DeviceConfig::ValidateOpenWeatherAPIKey(const String &newOpenWeatherAPIKey)
{
    HTTPClient http;

    String url = "http://api.openweathermap.org/data/2.5/weather?lat=0&lon=0&appid=" + urlEncode(newOpenWeatherAPIKey);

    http.begin(url);

    switch (http.GET())
    {
        case HTTP_CODE_OK:
        {
            http.end();
            return { true, "" };
        }

        case HTTP_CODE_UNAUTHORIZED:
        {
            auto jsonDoc = CreateJsonDocument();
            deserializeJson(jsonDoc, http.getString());

            String message = "";
            if (jsonDoc["message"].is<String>())
                message = jsonDoc["message"].as<String>();

            http.end();
            return { false, message };
        }

        // Anything else
        default:
        {
            http.end();
            return { false, "Unable to validate" };
        }
    }
}
#endif  // ENABLE_WIFI

void DeviceConfig::SetColorSettings(const CRGB& newGlobalColor, const CRGB& newSecondColor)
{
    globalColor = newGlobalColor;
    secondColor = newSecondColor;
    applyGlobalColors = true;

    SaveToJSON();
}

// This function contains the logic for dealing with the various color-related settings we have.
// The logic effectively mimics the behavior of pressing a color button on the IR remote control when (only) the
// global color is set or (re)applied, but also allows the secondary global palette color to be specified directly.
// The code in this function figures out how to prioritize and combine the values of (optional) settings; the actual
// logic for applying the correct color(s) and palette is contained in a number of EffectManager member functions.
void DeviceConfig::ApplyColorSettings(std::optional<CRGB> newGlobalColor, std::optional<CRGB> newSecondColor, bool clearGlobalColor, bool forceApplyGlobalColor)
{
    // If we're asked to clear the global color, we'll remember any colors we were passed, but won't do anything with them
    if (clearGlobalColor)
    {
        if (newGlobalColor.has_value())
            globalColor = newGlobalColor.value();
        if (newSecondColor.has_value())
            secondColor = newSecondColor.value();

        g_ptrSystem->GetEffectManager().ClearRemoteColor();

        applyGlobalColors = false;

        SaveToJSON();

        return;
    }

    CRGB finalGlobalColor = newGlobalColor.has_value() ? newGlobalColor.value() : globalColor;
    forceApplyGlobalColor = forceApplyGlobalColor || newGlobalColor.has_value();

    // If we were given a second color, set it and the global one if necessary. Then have EffectManager do its thing...
    if (newSecondColor.has_value())
    {
        if (forceApplyGlobalColor)
        {
            applyGlobalColors = true;
            globalColor = finalGlobalColor;
        }

        secondColor = newSecondColor.value();

        g_ptrSystem->GetEffectManager().ApplyGlobalPaletteColors();

        SaveToJSON();
    }
    else if (forceApplyGlobalColor)
    {
        // ...otherwise, apply the "set global color" logic if we were asked to do so
        g_ptrSystem->GetEffectManager().ApplyGlobalColor(finalGlobalColor);
    }
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateTopology(uint16_t width, uint16_t height, bool serpentine) const
{
    if (width == 0 || height == 0)
        return { false, "matrix dimensions must be greater than zero" };

    // The strip path sizes its live buffers from total LED capacity, not the original compile-time aspect ratio.
    // That keeps reshaping flexible while still refusing requests that would outgrow the compiled backing store.
    if (static_cast<size_t>(width) * height > GetCompiledLEDCount())
        return { false, kRecompileNeededMessage };

    if (IsHub75Build())
    {
        if (width != GetCompiledMatrixWidth() || height != GetCompiledMatrixHeight())
            return { false, kRecompileNeededMessage };

        if (serpentine != runtimeTopology.serpentine)
            return { false, kRecompileNeededMessage };
    }

    return { true, "" };
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateOutputDriver(OutputDriver driver) const
{
    if (driver != GetCompiledOutputDriver())
        return { false, kRecompileNeededMessage };

    return { true, "" };
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateWS281xSettings(size_t channelCount, const std::array<int8_t, NUM_CHANNELS>& pins) const
{
    if (channelCount == 0)
        return { false, "channel count must be greater than zero" };

    if (channelCount > GetCompiledChannelCount())
        return { false, kRecompileNeededMessage };

    if (IsHub75Build())
    {
        if (channelCount != GetCompiledChannelCount())
            return { false, kRecompileNeededMessage };

        if (pins != GetCompiledWS281xPins())
            return { false, kRecompileNeededMessage };
    }

    for (size_t i = 0; i < channelCount; ++i)
    {
        if (pins[i] < 0)
            return { false, "active channels require valid GPIO pins" };

        for (size_t j = i + 1; j < channelCount; ++j)
        {
            if (pins[i] == pins[j])
                return { false, "WS281x channel pins must be unique" };
        }
    }

    return { true, "" };
}

DeviceConfig::ValidateResponse DeviceConfig::ValidateRuntimeConfig(const RuntimeConfig& config) const
{
    auto [driverValid, driverMessage] = ValidateOutputDriver(config.outputs.driver);
    if (!driverValid)
        return { false, driverMessage };

    auto [topologyValid, topologyMessage] = ValidateTopology(config.topology.width, config.topology.height, config.topology.serpentine);
    if (!topologyValid)
        return { false, topologyMessage };

    auto [ws281xValid, ws281xMessage] = ValidateWS281xSettings(config.outputs.channelCount, config.outputs.outputPins);
    if (!ws281xValid)
        return { false, ws281xMessage };

    return { true, "" };
}

bool DeviceConfig::SetRuntimeConfig(const RuntimeConfig& config, bool skipWrite, String* errorMessage)
{
    auto [isValid, validationMessage] = ValidateRuntimeConfig(config);
    if (!isValid)
    {
        if (errorMessage)
            *errorMessage = validationMessage;
        return false;
    }

    const bool changed =
        runtimeTopology.width != config.topology.width
        || runtimeTopology.height != config.topology.height
        || runtimeTopology.serpentine != config.topology.serpentine
        || runtimeOutputs.driver != config.outputs.driver
        || runtimeOutputs.channelCount != config.outputs.channelCount
        || runtimeOutputs.outputPins != config.outputs.outputPins;

    runtimeTopology = config.topology;
    runtimeOutputs = config.outputs;

    if (!skipWrite)
        SaveToJSON();

    if (changed && !skipWrite)
        LogRuntimeConfig("runtime config changed");

    if (errorMessage)
        *errorMessage = "";

    return true;
}
