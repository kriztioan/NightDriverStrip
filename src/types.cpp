//+--------------------------------------------------------------------------
//
// File:        types.cpp
//
// NightDriverStrip - (c) 2023 Plummer's Software LLC.  All Rights Reserved.
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
//    Types of a somewhat general use
//
// History:     May-23-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"
#include "types.h"

#include <cassert>
#include <sys/time.h>

// AppTime
//
// A class that keeps track of the clock, how long the last frame took, calculating FPS, etc.

// NewFrame
//
// Call this at the start of every frame or update, and it'll figure out and keep track of how
// long between frames
void CAppTime::NewFrame()
{
    double current = CurrentTime();
    _deltaTime = current - _lastFrame;

    // Cap the delta time at one full second

    if (_deltaTime > 1.0)
        _deltaTime = 1.0;

    _lastFrame = current;
}

CAppTime::CAppTime()
{
    _lastFrame = CurrentTime();
    _deltaTime = 0;
}

double CAppTime::FrameStartTime() const
{
    return _lastFrame;
}

double CAppTime::CurrentTime()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return TimeFromTimeval(tv);
}

double CAppTime::FrameElapsedTime() const
{
    return CurrentTime() - _lastFrame;
}

double CAppTime::TimeFromTimeval(const timeval & tv)
{
    return tv.tv_sec + (tv.tv_usec/(double)MICROS_PER_SECOND);
}

timeval CAppTime::TimevalFromTime(double t)
{
    timeval tv;
    tv.tv_sec = (long)t;
    tv.tv_usec = t - tv.tv_sec;
    return tv;
}

double CAppTime::LastFrameTime() const
{
    return _deltaTime;
}

// Finishes the initialization of the spec, and then validates the consistency of its overall contents.
// Note that it does the latter quite rudely: it uses assert() on things it feels should be in order.
// This function is called by this struct's constructors that initialize values, but this being a struct
// allows itself to be called from the outside as well.
void SettingSpec::FinishAndValidateInitialization()
{
    // Default to front-end rejection of empty Strings, but only if the caller hasn't already
    // set EmptyAllowed explicitly (so FinishGuard re-runs don't clobber an intentional override)
    if (Type == SettingType::String && !EmptyAllowed.has_value())
        EmptyAllowed = false;

    // If min and max value are both set, min must be less or equal than max
    assert(!(MinimumValue.has_value() && MaximumValue.has_value()) || MinimumValue.value() <= MaximumValue.value());

    // For Slider widgets, display scale members must all be set or all be unset
    assert(Widget != WidgetKind::Slider || (DisplayRawMin.has_value() == DisplayRawMax.has_value() &&
           DisplayRawMin.has_value() == DisplayMin.has_value() &&
           DisplayRawMin.has_value() == DisplayMax.has_value()));

    // For Select widgets, validate options source-specific requirements
    if (Widget == WidgetKind::Select)
    {
        // For Inline select options, labels must be empty or match the number of values
        assert(Options != OptionsSource::Inline ||
               OptionLabels.empty() || OptionLabels.size() == OptionValues.size());

        // For SchemaPath select options, OptionsSchemaPath must be set, and
        // any label overrides must be provided as matched pairs (both non-empty, same length)
        assert(Options != OptionsSource::SchemaPath || OptionsSchemaPath != nullptr);
        assert(Options != OptionsSource::SchemaPath ||
               (OptionValues.empty() == OptionLabels.empty() &&
                (OptionValues.empty() || OptionValues.size() == OptionLabels.size())));

        // For ExternalTimeZones select options, OptionsExternalUrl must be set, and
        // any label overrides must be provided as matched pairs (both non-empty, same length)
        assert(Options != OptionsSource::ExternalTimeZones || OptionsExternalUrl != nullptr);
        assert(Options != OptionsSource::ExternalTimeZones ||
               (OptionValues.empty() == OptionLabels.empty() &&
                (OptionValues.empty() || OptionValues.size() == OptionLabels.size())));
    }
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type) : SettingSpec(name, friendlyName, nullptr, type)
{}

// Constructor that sets both minimum and maximum values
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type, double min, double max)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    MinimumValue(min),
    MaximumValue(max)
{
    FinishAndValidateInitialization();
}

// Constructor that sets both minimum and maximum values
SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type, double min, double max)
    : SettingSpec(name, friendlyName, nullptr, type, min, max)
{}

// Constructor A: basic positioned spec (section + apiPath, optional priority)
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         const char* section, const char* apiPath, std::optional<int> priority)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    Section(section),
    Priority(priority),
    ApiPath(apiPath)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         const char* section, const char* apiPath, std::optional<int> priority)
    : SettingSpec(name, friendlyName, nullptr, type, section, apiPath, priority)
{}

// Constructor B: positioned spec with non-default access and optional hasValidation
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         const char* section, const char* apiPath, SettingAccess access, bool hasValidation)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    HasValidation(hasValidation),
    Access(access),
    Section(section),
    ApiPath(apiPath)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         const char* section, const char* apiPath, SettingAccess access, bool hasValidation)
    : SettingSpec(name, friendlyName, nullptr, type, section, apiPath, access, hasValidation)
{}

// Constructor C: positioned spec with min/max range
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         double min, double max, const char* section, const char* apiPath, std::optional<int> priority)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    MinimumValue(min),
    MaximumValue(max),
    Section(section),
    Priority(priority),
    ApiPath(apiPath)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         double min, double max, const char* section, const char* apiPath, std::optional<int> priority)
    : SettingSpec(name, friendlyName, nullptr, type, min, max, section, apiPath, priority)
{}

// Constructor D: positioned SchemaPath Select widget
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         const char* section, const char* apiPath, const char* optionsSchemaPath, std::optional<int> priority)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    Section(section),
    Priority(priority),
    ApiPath(apiPath),
    Widget(WidgetKind::Select),
    Options(OptionsSource::SchemaPath),
    OptionsSchemaPath(optionsSchemaPath)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         const char* section, const char* apiPath, const char* optionsSchemaPath, std::optional<int> priority)
    : SettingSpec(name, friendlyName, nullptr, type, section, apiPath, optionsSchemaPath, priority)
{}

// Constructor E: positioned Select widget with non-Inline source
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         const char* section, const char* apiPath, OptionsSource optionsSource,
                         const char* optionsExternalUrl)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    Section(section),
    ApiPath(apiPath),
    Widget(WidgetKind::Select),
    Options(optionsSource),
    OptionsExternalUrl(optionsExternalUrl)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         const char* section, const char* apiPath, OptionsSource optionsSource,
                         const char* optionsExternalUrl)
    : SettingSpec(name, friendlyName, nullptr, type, section, apiPath, optionsSource, optionsExternalUrl)
{}

// Constructor F: positioned Slider widget with display scale
SettingSpec::SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type,
                         const char* section, const char* apiPath,
                         double displayRawMin, double displayRawMax, double displayMin, double displayMax,
                         const char* displaySuffix, bool hasValidation)
    : Name(name),
    FriendlyName(friendlyName),
    Description(description),
    Type(type),
    HasValidation(hasValidation),
    Section(section),
    ApiPath(apiPath),
    Widget(WidgetKind::Slider),
    DisplayRawMin(displayRawMin),
    DisplayRawMax(displayRawMax),
    DisplayMin(displayMin),
    DisplayMax(displayMax),
    DisplaySuffix(displaySuffix)
{
    FinishAndValidateInitialization();
}

SettingSpec::SettingSpec(const char* name, const char* friendlyName, SettingType type,
                         const char* section, const char* apiPath,
                         double displayRawMin, double displayRawMax, double displayMin, double displayMax,
                         const char* displaySuffix, bool hasValidation)
    : SettingSpec(name, friendlyName, nullptr, type, section, apiPath,
                  displayRawMin, displayRawMax, displayMin, displayMax, displaySuffix, hasValidation)
{}

String SettingSpec::TypeName() const
{
    switch (Type)
    {
        case SettingType::Integer:              return "Integer";
        case SettingType::PositiveBigInteger:   return "PositiveBigInteger";
        case SettingType::Float:                return "Float";
        case SettingType::Boolean:              return "Boolean";
        case SettingType::String:               return "String";
        case SettingType::Palette:              return "Palette";
        case SettingType::Color:                return "Color";
        default:                                return "Unknown";
    }
}

const char* SettingSpec::WidgetName() const
{
    switch (Widget)
    {
        case WidgetKind::Slider:           return "slider";
        case WidgetKind::Select:           return "select";
        case WidgetKind::IntervalToggle:   return "intervalToggle";
        default:                           return "default";
    }
}

const char* SettingSpec::OptionsSourceName() const
{
    switch (Options)
    {
        case OptionsSource::Inline:             return "inline";
        case OptionsSource::SchemaPath:         return "schemaPath";
        case OptionsSource::IntlCountryCodes:   return "intlCountryCodes";
        case OptionsSource::ExternalTimeZones:  return "externalTimeZones";
        default:                                return "inline";
    }
}

// PreferPSRAMAlloc
//
// Will return PSRAM if it's available, regular ram otherwise

// Cache PSRAM availability and prefer it when allocating large buffers.
void * PreferPSRAMAlloc(size_t s)
{
    // Compute PSRAM availability once in a thread-safe way (I believe C++11+ guarantees thread-safe initialization of function-local statics).
    static const int s_psramAvailable = []() noexcept -> int {
        return psramInit() ? 1 : 0;
    }();

    if (s_psramAvailable)
    {
        debugV("PSRAM Array Request for %zu bytes\n", s);
        auto p = ps_malloc(s);
        if (!p)
        {
            debugE("PSRAM Allocation failed for %zu bytes\n", s);
            throw std::bad_alloc();
        }
        return p;
    }
    else
    {
        auto p = malloc(s);
        if (!p)
        {
            debugE("RAM Allocation failed for %zu bytes\n", s);
            throw std::bad_alloc();
        }
        return p;
    }
}
