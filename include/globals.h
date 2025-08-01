//+--------------------------------------------------------------------------
//
// File:        Globals.h
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
//    Adapted from old ESP32LEDStick box and BigBlueLCD code by Davepl
//
// History:     Apr-13-2019  Version    Davepl      Created for NightDriverStrip
//              Dec-09-2019  v001       Davepl      Unified from multiple projects
//
//               First version with color to indicate status, including
//               purple during flashing, red for no wifi, blue for no clock.
//               And first version with flash version display on TFT.
//
//              Dec-13-2019  v002       Davepl      Task priorities and RAM
//
//               Freed up more ram so we get 40 frames of 8x144 and tuned the
//               task priorities so they're constantly processed and rev'd
//               the UI to include a time range of buffer frames.  Also added
//               partial draw support and variable framerate.
//
//              Dec-16-2019  v003       Davepl      1st client using clockstream
//              Dec-19-2019  v004       Davepl      1st client using 128-bit clock
//              Dec-25-2019  v005       Davepl      Added PixelData64
//
//                Made the clock timestamps full 64 bit and transmit same in new
//                PIXELDATA64 command.  Idles in white now even without clock.
//
//              Jun-24-2020  v006       Davepl      Reverted to NTP
//              Sep-14-2020  v008       Davepl      Adding support for M5StickC
//              Nov-28-2020  v009       Davepl      Added FANSET scenario
//              Apr-10-2021  v010       Davepl      BUG in length, bad version
//              Apr-13-2021  v011       Davepl      Fixed length issue
//              Apr-18-2021  v012       Davepl      Multiple Fixes
//              Apr-21-2021  v013       Davepl      SPIFFs reference in SetupOTA
//              Apr-22-2021  v014       Davepl      Moved OTA pump to net thread
//              Apr-23-2021  v015       Davepl      Fixed fan effects
//              Apr-23-2021  v016       Davepl      Fix max power limit!
//              Apr-24-2021  v017       Davepl      Fix compressed frames - stable!
//              May-01-2021  v018       Davepl      Put receive timeout back in, cRec'd to 0
//              Jun-17-2021  v019       Davepl      Atomlight2 + variable FPS
//              Jul-08-2021  v020       Davepl      Particle System, Insulators, lib deps
//              Sep-18-2021  v021       Davepl      Github Release
//              Nov-07-2021  v022       Davepl      Rev'd with new Github PRs to date
//              Mar-16-2022  v023       Davepl      Response packet on socket with stats
//              Mar-17-2022  v024       Davepl      Catchup clock to server when in future
//              May-17-2022  v025       Davepl      After merge of RepsonsePacket into main
//              May-24-2022  v026       Davepl      Adding BaseGFX/LEDMatrixGFX/LEDStripGFX
//              Oct-01-2022  v027       Davepl      Mesmerizer integration and screen fixes
//              Oct-01-2022  v028       Davepl      Adjust buffer sizes due to lower mem free
//              Oct-02-2022  v029       Davepl      Change WiFiUDP to use free/malloc
//              Oct-03-2022  v030       Davepl      Smoother Draw and support for TFT S3 Feather
//              Oct-30-2022  v031       Davepl      Screen cleanup, core assignments moved around
//              Oct-30-2022  v032       Davepl      Better wait code, core assignments
//              Oct-30-2022  v033       Davepl      Fixed mistiming bug when no draw was ready
//              Nov-15-2022  v034       Davepl      Fixed buffer full condition
//              Jan-19-2023  v035       Davepl      After LaserLine episode merge
//              Jan-29-2023  v036       Davepl      After Char *, string, includes, soundanalyzer
//              Jun-10-2023  v037       Davepl      New Screen classes
//              Jul-24-2023  v038       Davepl      NTP clock fix
//              Jul-26-2023  v039       Davepl      NTP every minute, stack sizes
//              Jul-26-2023  v040       Davepl      NTP every 5 minutes, Wifi delay code
//
//---------------------------------------------------------------------------

#pragma once

#include <sstream>
#include <iomanip>

//  See https://github.com/PlummersSoftwareLLC/NightDriverStrip/issues/515
#define FASTLED_ESP32_FLASH_LOCK 1
#define FASTLED_INTERNAL 1               // Suppresses build banners
#include <FastLED.h>

#include <RemoteDebug.h>

// If we're not using GNU C, (unlikely in embedded, especially in this
// heavily ESP/Arduino-accented probject) elide __attribute__ - but even
// clang defines and supports this...
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

// The goal here is to get two variables, one numeric and one string, from the *same* version
// value.  So if version = 020,
//
// FLASH_VERSION==20
// FLASH_VERSION_NAME=="v020"
//
// BUGBUG (davepl): If you know a cleaner way, please improve this!

#define FLASH_VERSION          40   // Update ONLY this to increment the version number

#ifndef USE_HUB75                   // We support strips by default unless specifically defined out
    #ifndef USE_WS281X
        #define USE_WS281X 1
    #endif
#endif

#define XSTR(x) STR(x)              // The defs will generate the stringized version of it
#if FLASH_VERSION > 99
    #define STR(x) "v"#x
#else
    #define STR(x) "v0"#x
#endif
#define FLASH_VERSION_NAME_X(x) "v"#x
#define FLASH_VERSION_NAME XSTR(FLASH_VERSION)

#define FASTLED_INTERNAL        1       // Silence FastLED build banners
#define NTP_DELAY_SECONDS       (5*60)  // delay count for NTP update, in seconds
#define NTP_DELAY_ERROR_SECONDS 30      // delay count for NTP updates if no time was set, in seconds
#define NTP_PACKET_LENGTH       48      // ntp packet length

// C Helpers and Macros

#define NAME_OF(x)          #x

// NAME_OF with first character cut off - addresses underscore-prefixed (member) variables
#define ACTUAL_NAME_OF(x)   ((#x) + 1)

#define PERIOD_FROM_FREQ(f) (round(1000000 * (1.0 / f)))    // Calculate period in microseconds (us) from frequency in Hz
#define FREQ_FROM_PERIOD(p) (1.0 / p * 1000000)             // Calculate frequency in Hz given the period in microseconds (us)

// I've built and run this on the Heltec Wifi 32 module and the M5StickC.  The
// main difference is pinout and the OLED/LCD screen.  The presence of absence
// of the OLED/LCD is now controlled separately, but M5 is always equipped
// with one (but it doesn't have to be used!).

#if M5STICKC || M5STICKCPLUS || M5STACKCORE2 || M5STICKCPLUS2
    #define USE_M5 1
#endif

#if USE_M5
#include "M5Unified.h"
#undef min                                      // They define a min() on us
#endif

#define EFFECT_CROSS_FADE_TIME 1200.0    // How long for an effect to ramp brightness fader down and back during effect change

// Thread priorities
//
// We have a half-dozen workers and these are their relative priorities.  It might survive if all were set equal,
// but I think drawing should be lower than audio so that a bad or greedy effect doesn't starve the audio system.
//
// Idle tasks in taskmgr run at IDLE_PRIORITY+1 so you want to be at least +2

#define DRAWING_PRIORITY        (tskIDLE_PRIORITY+8)
#define SOCKET_PRIORITY         (tskIDLE_PRIORITY+7)
#define AUDIOSERIAL_PRIORITY    (tskIDLE_PRIORITY+6)      // If equal or lower than audio, will produce garbage on serial
#define NET_PRIORITY            (tskIDLE_PRIORITY+5)
#define AUDIO_PRIORITY          (tskIDLE_PRIORITY+4)
#define SCREEN_PRIORITY         (tskIDLE_PRIORITY+3)

#define REMOTE_PRIORITY         (tskIDLE_PRIORITY+3)
#define DEBUG_PRIORITY          (tskIDLE_PRIORITY+2)
#define JSONWRITER_PRIORITY     (tskIDLE_PRIORITY+2)
#define COLORDATA_PRIORITY      (tskIDLE_PRIORITY+2)

// If you experiment and mess these up, my go-to solution is to put Drawing on Core 0, and everything else on Core 1.
// My current core layout is as follows, and as of today it's solid as of (7/16/21).
//
// #define DRAWING_CORE            0
// #define NET_CORE                1
// #define AUDIO_CORE              0
// #define SCREEN_CORE             1
// #define DEBUG_CORE              1
// #define SOCKET_CORE             1
// #define REMOTE_CORE             1

// Some "Reliability Rules"
// Drawing must be on Core 1 if using SmartMatrix unless you specify SMARTMATRIX_OPTIONS_ESP32_CALC_TASK_CORE_1

#define DRAWING_CORE            1
#define NET_CORE                1
#define AUDIO_CORE              0
#define AUDIOSERIAL_CORE        1
#define SCREEN_CORE             1
#define DEBUG_CORE              1
#define SOCKET_CORE             1
#define REMOTE_CORE             1
#define JSONWRITER_CORE         0
#define COLORDATA_CORE          1

#define FASTLED_INTERNAL            1   // Suppresses the compilation banner from FastLED
#define __STDC_FORMAT_MACROS

extern RemoteDebug Debug;           // Let everyone in the project know about it.  If you don't have it, delete this

// Project Configuration
//
// One and only one of DEMO, SPECTRUM, ATOMLIGHT, etc. should be set to true by the build config for your project
//
// I've used this code to build a dozen different projects, most of which can be created by defining
// the right built environment (like INSULATORS=1).  The config here defines everything about the
// LEDs, how many, on how many channels, laid out into how many fans/rings, and so on.  You can also
// specify the audio system config like how many band channels.

#if __has_include ("custom_globals.h")

    // To reduce clutter, you may choose to create a new file called `custom_globals.h` in the `includes` directory.
    // You can place your project configurations and logic to select them in that file.
    // This can be done once you know how `platformio.ini` and `globals.h` interact with one another
    // to create different environments and projects.

    #include "custom_globals.h"

#elif DEMO

    // This is a simple demo configuration.  To build, simply connect the data lead from a WS2812B
    // strip to pin 5 or other pin marked PIN0 below.  This does not use the OLED, LCD, or anything fancy, it simply drives the
    // LEDs with a simple rainbow effect as specified in effects.cpp for DEMO.
    //
    // Please ensure you supply sufficent power to your strip, as even the DEMO of 144 LEDs, if set
    // to white, would overload a USB port.
    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Demo"
    #endif

    #define MATRIX_WIDTH            144
    #define MATRIX_HEIGHT           1
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            1
    #define ENABLE_AUDIO            0

    // Once you have a working project, selectively enable various additional features by setting
    // them to 1 in the list below.  This DEMO config assumes no audio (mic), or screen, etc.

    #ifndef ENABLE_WIFI
        #define ENABLE_WIFI             0   // Connect to WiFi
    #endif

    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define TIME_BEFORE_LOCAL       0   // How many seconds before the lamp times out and shows local content
    #define ENABLE_NTP              0   // Set the clock from the web
    #define ENABLE_OTA              0   // Accept over the air flash updates

    #ifndef LED_PIN0
        #if USE_M5
            #define LED_PIN0 32
        #elif LILYGOTDISPLAYS3
            #define LED_PIN0 21
        #else
            #define LED_PIN0 5
        #endif
    #endif

    // The webserver serves files that are baked into the device firmware. When running you should be able to
    // see/select the list of effects by visiting the chip's IP in a browser.  You can get the chip's IP by
    // watching the serial output or checking your router for the DHCP given to a new device; often they're
    // named "esp32-" followed by a seemingly random 6-digit hexadecimal number.

    #ifndef ENABLE_WEBSERVER
        #define ENABLE_WEBSERVER        0   // Turn on the internal webserver
    #endif

#elif M5DEMO

    // This is the DEMO project customized for the M5 that includes screen support and other
    // features that make it well suited to the demo strip that runs in Dave's Garage

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "M5Demo"
    #endif

    #define MATRIX_WIDTH            144*5+38
    #define MATRIX_HEIGHT           1
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            1
    #define COLOR_ORDER             EOrder::RGB

    #define ENABLE_AUDIOSERIAL          0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI                 1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED       1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI               0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL           2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER            0   // Turn on the internal webserver
    #define ENABLE_NTP                  1   // Set the clock from the web
    #define ENABLE_OTA                  0   // Accept over the air flash updates
    #define ENABLE_REMOTE               0   // IR Remote Control
    #define ENABLE_AUDIO                1   // Listen for audio from the microphone and process it
    #define COLORDATA_SERVER_ENABLED    0

    #if USE_PSRAM
        #define MAX_BUFFERS             500
    #else
        #define MAX_BUFFERS             24
    #endif

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24*5)

    #ifndef LED_PIN0
        #if USE_M5
            #define LED_PIN0 32
        #elif LILYGOTDISPLAYS3
            #define LED_PIN0 21
        #else
            #define LED_PIN0 5
        #endif
    #endif

    #define TOGGLE_BUTTON_1 37
    #define TOGGLE_BUTTON_2 39

#elif LANTERN

    // A railway-style lantern with concentric rings of light (16+12+8+1)

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Lantern"
    #endif

    #define NUM_FANS                1
    #define NUM_RINGS               4
    #define FAN_SIZE                (RING_SIZE_0 + RING_SIZE_1 + RING_SIZE_2 + RING_SIZE_3)
    #define RING_SIZE_0             16
    #define RING_SIZE_1             12
    #define RING_SIZE_2             8
    #define RING_SIZE_3             1
    #define MATRIX_WIDTH            6
    #define MATRIX_HEIGHT           2
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            1
    #define ENABLE_AUDIO            1

    // Once you have a working project, selectively enable various additional features by setting
    // them to 1 in the list below.  This config assumes no audio (mic), or screen, etc.

    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define TIME_BEFORE_LOCAL       0   // How many seconds before the lamp times out and shows local contexnt
    #define ENABLE_NTP              0   // Set the clock from the web
    #define ENABLE_OTA              0   // Accept over the air flash updates

    #ifndef LED_PIN0
        #if M5STICKC
            #define LED_PIN0 33
        #elif M5STICKCPLUS || M5STACKCORE2 || M5STICKCPLUS2
            #define LED_PIN0 32
        #else
            #define LED_PIN0 5
        #endif
    #endif

    // The webserver serves files that are baked into the device firmware. When running you should be able to
    // see/select the list of effects by visiting the chip's IP in a browser.  You can get the chip's IP by
    // watching the serial output or checking your router for the DHCP given to a new device; often they're
    // named "esp32-" followed by a seemingly random 6-digit hexadecimal number.


    #define ENABLE_WEBSERVER        0                                       // Turn on the internal webserver
    #define DEFAULT_EFFECT_INTERVAL 1000 * 60 * 60 * 24                     // One a day!

    #define TOGGLE_BUTTON_1 37
    #define TOGGLE_BUTTON_2 39

#elif PDPGRID

    // A matrix grid display for the front of the PDP-11

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "PDPGrid"
    #endif

    #define NUM_FANS                1
    #define NUM_RINGS               4
    #define FAN_SIZE                (RING_SIZE_0 + RING_SIZE_1 + RING_SIZE_2 + RING_SIZE_3)
    #define RING_SIZE_0             16
    #define RING_SIZE_1             12
    #define RING_SIZE_2             8
    #define RING_SIZE_3             1
    #define MATRIX_WIDTH            14
    #define MATRIX_HEIGHT           16
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            1
    #define ENABLE_AUDIO            1

    #define POWER_LIMIT_MW          1000

    // Once you have a working project, selectively enable various additional features by setting
    // them to 1 in the list below.  This config assumes no audio (mic), or screen, etc.

    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local contexnt
    #define ENABLE_NTP              1   // Set the clock from the web
    #define ENABLE_OTA              1   // Accept over the air flash updates
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver

    #ifndef LED_PIN0
        #define LED_PIN0            32
    #endif

    // The webserver serves files that are baked into the device firmware. When running you should be able to
    // see/select the list of effects by visiting the chip's IP in a browser.  You can get the chip's IP by
    // watching the serial output or checking your router for the DHCP given to a new device; often they're
    // named "esp32-" followed by a seemingly random 6-digit hexadecimal number.

    #define DEFAULT_EFFECT_INTERVAL 0

    #define TOGGLE_BUTTON_1 37
    #define TOGGLE_BUTTON_2 39

#elif TREESET

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Treeset"
    #endif

    #define ENABLE_WIFI             1  // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       0   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver
    #define ENABLE_NTP              0   // Set the clock from the web
    #define ENABLE_OTA              1   // Accept over the air flash updates
    #define ENABLE_REMOTE           1   // IR Remote Control
    #define ENABLE_AUDIO            1   // Listen for audio from the microphone and process it

    #ifndef LED_PIN0
        #define LED_PIN0      26
    #endif

    #define NUM_CHANNELS      1
    #define RING_SIZE_0       24
    #define BONUS_PIXELS      0
    #define MATRIX_WIDTH      5
    #define MATRIX_HEIGHT     RING_SIZE_0
    #define NUM_FANS          MATRIX_WIDTH
    #define FAN_SIZE          MATRIX_HEIGHT
    #define NUM_LEDS          (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define ENABLE_REMOTE     1                     // IR Remote Control
    #define ENABLE_AUDIO      1                     // Listen for audio from the microphone and process it
    #define IR_REMOTE_PIN     25
    #define LED_FAN_OFFSET_BU 12

    #define TOGGLE_BUTTON  37

#elif WROVERKIT

    #define MATRIX_WIDTH            144
    #define MATRIX_HEIGHT           1
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            1
    #define NUM_RINGS               5
    #define RING_SIZE_0             24

    #define USE_LCD                 1

    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Doesn't work smoothly with the screen on for some reason!
    #define TIME_BEFORE_LOCAL       2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_NTP              1   // Set the clock from the web
    #define ENABLE_OTA              1   // Accept over the air flash updates
    #define WAIT_FOR_WIFI           0   // Don't *need* it so don't wait for it

    #ifndef LED_PIN0
        #define LED_PIN0 5
    #endif

    // The webserver serves files that are baked into the device firmware. When running you should be able to
    // see/select the list of effects by visiting the chip's IP in a browser.  You can get the chip's IP by
    // watching the serial output or checking your router for the DHCP given to a new device; often they're
    // named "esp32-" followed by a seemingly random 6-digit hexadecimal number.

    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver


#elif LASERLINE

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Laser Line"
    #endif


    #define ENABLE_AUDIOSERIAL      0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       0   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER        0   // Turn on the internal webserver
    #define ENABLE_NTP              0   // Set the clock from the web
    #define ENABLE_OTA              0   // Accept over the air flash updates
    #define ENABLE_REMOTE           0   // IR Remote Control
    #define ENABLE_AUDIO            1   // Listen for audio from the microphone and process it

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24*5)

    #define NUM_CHANNELS    1
    #define RING_SIZE_0     24
    #define BONUS_PIXELS    0
    #define MATRIX_WIDTH    700
    #define MATRIX_HEIGHT   1
    #define NUM_FANS        MATRIX_WIDTH
    #define FAN_SIZE        MATRIX_HEIGHT
    #define NUM_LEDS        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define LED_FAN_OFFSET_BU 6

    #define TOGGLE_BUTTON_1 37
    #define TOGGLE_BUTTON_2 39

    #ifndef LED_PIN0
        #define LED_PIN0 32
    #endif

#elif MESMERIZER

    // This project uses a HUB75 matrix (standard 64 pixels wide and 32 high), to show a wide range of
    // effects. Its primary target device is the Mesmerizer board designed by Dave Plummer, but has been
    // known to work with at least one other type of device as well.

    #ifndef PROJECT_NAME
    #define PROJECT_NAME                "Mesmerizer"
    #endif

    #define SHOW_FPS_ON_MATRIX          0
    #define ENABLE_AUDIOSERIAL          0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI                 1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED       1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI               0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL           2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER            1   // Turn on the internal webserver
    #define ENABLE_NTP                  1   // Set the clock from the web
    #define ENABLE_OTA                  1   // Accept over the air flash updates
    #define ENABLE_REMOTE               1   // IR Remote Control
    #define ENABLE_AUDIO                1   // Listen for audio from the microphone and process it
    #define SCALE_AUDIO_EXPONENTIAL     0
    #define EFFECT_PERSISTENCE_CRITICAL 1   // Require effects serialization to succeed

    #define DEFAULT_EFFECT_INTERVAL     (MILLIS_PER_SECOND * 60 * 2)
    #define MILLIS_PER_FRAME            0

    #define NUM_CHANNELS                1
    #define RING_SIZE_0                 24
    #define BONUS_PIXELS                0
    #define MATRIX_WIDTH                64
    #define MATRIX_HEIGHT               32
    #define NUM_FANS                    128
    #define FAN_SIZE                    16
    #define NUM_BANDS                   16
    #define NUM_LEDS                    (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define IR_REMOTE_PIN               39
    #define INPUT_PIN                   36
    #define LED_FAN_OFFSET_BU           6

    #define TOGGLE_BUTTON_1             0

    // The mesmerizer mic isn't quite as sensitive as the M5 mic that the code was originally written for
    // so we adjust by a scalar to get the same effect.

    #define AUDIO_MIC_SCALAR            1.5

    #define COLOR_ORDER                 EOrder::RGB

#elif TTGO

    // Variant of Spectrum set up for a TTGO using a MAX4466 microphone on pin27

    // This project is set up as a 48x16 matrix of 16x16 WS2812B panels such as: https://amzn.to/3ABs5DK
    // It displays a spectrum analyzer and music visualizer

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "TTGO"
    #endif

    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver
    #define ENABLE_NTP              1   // Set the clock from the web
    #define ENABLE_OTA              0   // Accept over the air flash updates
    #define ENABLE_REMOTE           1   // IR Remote Control
    #define ENABLE_AUDIO            1   // Listen for audio from the microphone and process it

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24)

    #define MAX_BUFFERS     20

    #ifndef LED_PIN0
        #define LED_PIN0    21          // Note that TFT board on TFTGO uses pins 19, 18, 5, 16, 23, and 4
    #endif
    #define NUM_CHANNELS    1
    #define RING_SIZE_0     24
    #define BONUS_PIXELS    0
    #define MATRIX_WIDTH    48
    #define MATRIX_HEIGHT   16
    #define NUM_FANS        MATRIX_WIDTH
    #define FAN_SIZE        MATRIX_HEIGHT
    #define NUM_LEDS        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define IR_REMOTE_PIN   22
    #define LED_FAN_OFFSET_BU 6

    #define TOGGLE_BUTTON_1         35

#elif XMASTREES

    // This project is set up as a 48x16 matrix of 16x16 WS2812B panels such as: https://amzn.to/3ABs5DK
    // It uses an M5StickCPlus which has a microphone and LCD built in:  https://amzn.to/3CrvCFh
    // It displays a spectrum analyzer and music visualizer

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "X-mas Trees"
    #endif

    #define ENABLE_WIFI             1  // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver
    #define ENABLE_NTP              1   // Set the clock from the web
    #define ENABLE_OTA              0  // Accept over the air flash updates
    #define ENABLE_REMOTE           1   // IR Remote Control
    #define ENABLE_AUDIO            1   // Listen for audio from the microphone and process it

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24)

    #define MAX_BUFFERS     20

    #ifndef LED_PIN0
        #define LED_PIN0    26
    #endif
    #define NUM_CHANNELS    1
    #define RING_SIZE_0     24
    #define BONUS_PIXELS    0
    #define MATRIX_WIDTH    24
    #define MATRIX_HEIGHT   5
    #define FAN_SIZE        MATRIX_WIDTH
    #define NUM_FANS        MATRIX_HEIGHT
    #define NUM_LEDS        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define IR_REMOTE_PIN   25
    #define LED_FAN_OFFSET_BU 6

    #define TOGGLE_BUTTON_1         37
    #define TOGGLE_BUTTON_2         39

#elif ATOMLIGHT

    // This is the "Tiki Atomic Fire Lamp" project, which is an LED lamp with 4 arms of 53 LEDs each.
    // Each arm is wired as a separate channel.

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Atom Light"
    #endif

    #define ENABLE_WIFI             1               // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1               // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0               // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       3               // How many seconds before the lamp times out and shows local content

    #define MAX_BUFFERS     30                      // Times 4 channels, but they're only NUM_LEDS big
    #define NUM_CHANNELS    4                       // One per spoke
    #define MATRIX_WIDTH    53                      // Number of pixels wide (how many LEDs per channel)
    #define MATRIX_HEIGHT   1                       // Number of pixels tall
    #define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE   1                       // IR Remote Control
    #define IR_REMOTE_PIN   35                      // Eric's is PIN 35
    #define ENABLE_AUDIO    1                       // Listen for audio from the microphone and process it
    #define USE_SCREEN      0                       // Normally we use a tiny board inside the lamp with no screen
    #define FAN_SIZE        NUM_LEDS                // Allows us to use fan effects on the spokes
    #define NUM_FANS        1                       // Our fans are on channels, not in sequential order, so only one "fan"
    #define NUM_RINGS       1
    #define LED_FAN_OFFSET_BU 0
    #define BONUS_PIXELS      0

    // Original Wiring:
    //   Fine red     = 3.3v
    //        brown   = gnd
    //        orange  = IO15
    //        yellow  = IO14
    //        green   = IO13
    //        blue    = IO12
    //        purple  = IO4

    // Wiring is:

    #define LED_PIN0         5
    #define LED_PIN1        16
    #define LED_PIN2        17
    #define LED_PIN3        18

    #define DEFAULT_EFFECT_INTERVAL     (1000*60*5)

#elif SPIRALLAMP

    // This is the "Tiki Atomic Fire Lamp" project, which is an LED lamp with 4 arms of 53 LEDs each.
    // Each arm is wired as a separate channel.

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Spiral Light"
    #endif

    #define ENABLE_WIFI             1              // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1              // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0              // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       3              // How many seconds before the lamp times out and shows local content
    #define ENABLE_OTA              0

    #define MAX_BUFFERS             30             // Times 4 channels, but they're only NUM_LEDS big
    #define NUM_CHANNELS            2              // One per spoke
    #define MATRIX_WIDTH            172            // Number of pixels wide (how many LEDs per channel)
    #define MATRIX_HEIGHT           1              // Number of pixels tall
    #define NUM_LEDS                (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE           1              // IR Remote Control
    #define IR_REMOTE_PIN           26
    #define ENABLE_AUDIO            1              // Listen for audio from the microphone and process it
    #define USE_SCREEN              1              // Normally we use a tiny board inside the lamp with no screen
    #define FAN_SIZE                NUM_LEDS       // Allows us to use fan effects on the spokes
    #define NUM_FANS                1              // Our fans are on channels, not in sequential order, so only one "fan"
    #define NUM_RINGS               1
    #define FULL_COLOR_REMOTE_FILL  1              // Remote control color buttons fill the whole strip
    #define BRIGHTNESS_MIN          0              // Allow OFF button to turn lamp entirely off

    // Wiring is:

    #define TOGGLE_BUTTON_1         39
    #define TOGGLE_BUTTON_2         37

    #define LED_PIN0                32
    #define LED_PIN1                33

    #define DEFAULT_EFFECT_INTERVAL     (1000*60*5)

#elif PLATECOVER

    // This is the "Tiki Atomic Fire Lamp" project, which is an LED lamp with 4 arms of 53 LEDs each.
    // Each arm is wired as a separate channel.

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Plate Cover"
    #endif

    #define ENABLE_ESPNOW           1               // Connect to ESPNOW and listen for packets
    #define ENABLE_WIFI             0               // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0               // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0               // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       3               // How many seconds before the lamp times out and shows local content
    #define MAX_BUFFERS             60              // Times 4 channels, but they're only NUM_LEDS big
    #define NUM_CHANNELS            1               // One per spoke
    #define MATRIX_WIDTH            40              // Number of pixels wide (how many LEDs per channel)
    #define MATRIX_HEIGHT           1               // Number of pixels tall
    #define NUM_LEDS                40
    #define ENABLE_REMOTE           1               // IR Remote Control
    #define IR_REMOTE_PIN           26
    #define ENABLE_AUDIO            1               // Listen for audio from the microphone and process it
    #define USE_SCREEN              1               // Normally we use a tiny board inside the lamp with no screen
    #define FAN_SIZE                NUM_LEDS        // Allows us to use fan effects on the spokes
    // Wiring is:

    #define TOGGLE_BUTTON_1         39
    #define TOGGLE_BUTTON_2         37

    #ifndef LED_PIN0
        #define LED_PIN0            32
    #endif

    #define DEFAULT_EFFECT_INTERVAL 0               // No scheduled effect changes

#elif UMBRELLA

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Umbrella"
    #endif

    #define COLOR_ORDER     EOrder::RGB
    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_OTA              1   // Enable over the air updates to the flash
    #define ENABLE_REMOTE   1                     // IR Remote Control
    #define IR_REMOTE_PIN   39
    #define ENABLE_AUDIO    1                     // Listen for audio from the microphone and process it
    #define MAX_BUFFERS     40

    #define DEFAULT_EFFECT_INTERVAL     (1000*30 * 60)


    // The "Tiki Fire Umbrella" project, with 8 channels

    #ifndef LED_PIN0
        #define LED_PIN0    5   // Only one pin, it's routed to all 8 spokes.  Independent turned out not to be that useful.
    #endif

    #define NUM_CHANNELS    1
    #define MATRIX_WIDTH    228                   // Number of pixels wide (how many LEDs per channel)
    #define MATRIX_HEIGHT   1                     // Number of pixels tall

    #define ONBOARD_LED_R    16
    #define ONBOARD_LED_G    17
    #define ONBOARD_LED_B    18

    #define TOGGLE_BUTTON_2  0

#elif MAGICMIRROR

    // A magic infinity mirror such as: https://amzn.to/3lEZo2D
    // I then replaced the white LEDs with a WS2812B strip and a heltec32 module:

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Magic Mirror"
    #endif

    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local content

    #define DEFAULT_EFFECT_INTERVAL     (10*60*24)

    #ifndef LED_PIN0
        #define LED_PIN0    26
    #endif
    #define NUM_CHANNELS    1
    #define BONUS_PIXELS    0
    #define NUM_FANS        1
    #define FAN_SIZE        100
    #define MATRIX_WIDTH    (NUM_FANS * FAN_SIZE + BONUS_PIXELS)
    #define NUM_LEDS        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define MATRIX_HEIGHT   NUM_FANS
    #define ENABLE_REMOTE   1                     // IR Remote Control
    #define ENABLE_AUDIO    1                     // Listen for audio from the microphone and process it
    #define IR_REMOTE_PIN   15

    #define LED_FAN_OFFSET_BU 6

#elif HEXAGON

    // The LED strips I use for Christmas lights under my eaves

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Hexagon"
    #endif

    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver
    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local content

    #define NUM_CHANNELS    1
    #define MATRIX_WIDTH    (271)     // My maximum run, and about all you can do at 30fps
    #define MATRIX_HEIGHT   1
    #define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE   0                     // IR Remote Control
    #define ENABLE_AUDIO    0                     // Listen for audio from the microphone and process it

    #ifndef LED_PIN0
        #define LED_PIN0    5
    #endif

    #define DEFAULT_EFFECT_INTERVAL     (1000*20)

    #define HEX_MAX_DIMENSION 19                 // How big the hex is - it's biggest row and the number of rows
    #define HEX_HALF_DIMENSION 10                // How many rows from top to middle inclusive

#elif LEDSTRIP

    // The LED strips I use for Christmas lights under my eaves

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Ledstrip"
    #endif

    #ifndef ENABLE_WEBSERVER
    #define ENABLE_WEBSERVER            0   // Turn on the internal webserver
    #endif

    #define ENABLE_WIFI                 1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED       1   // Accepting incoming color data and commands

    #define WAIT_FOR_WIFI               1   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL           5   // How many seconds before the lamp times out and shows local content
    #define COLORDATA_SERVER_ENABLED    0   // Also provides a response packet
    #define NUM_CHANNELS    1
    #define MATRIX_WIDTH    (8*144)     // My maximum run, and about all you can do at 30fps
    #define MATRIX_HEIGHT   1
    #define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE   0                     // IR Remote Control
    #define ENABLE_AUDIO    0                     // Listen for audio from the microphone and process it

    #ifndef LED_PIN0
        #define LED_PIN0    5
    #endif

    #define DEFAULT_EFFECT_INTERVAL     (1000*20)

    #define RING_SIZE_0 1
    #define RING_SIZE_1 2
    #define RING_SIZE_2 4
    #define RING_SIZE_3 8
    #define RING_SIZE_4 16

#elif CHIEFTAIN

    // The LED strips I use for Christmas lights under my eaves

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Chieftain"
    #endif

    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       5   // How many seconds before the lamp times out and shows local content

    #define NUM_CHANNELS    1
    #define MATRIX_WIDTH    (12)
    #define MATRIX_HEIGHT   1
    #define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE   0                     // IR Remote Control
    #define ENABLE_AUDIO    1                     // Listen for audio from the microphone and process it
    #ifndef LED_PIN0
        #define LED_PIN0    5
    #endif

    #define DEFAULT_EFFECT_INTERVAL     (1000*20)

    #define RING_SIZE_0 1
    #define RING_SIZE_1 2
    #define RING_SIZE_2 4
    #define RING_SIZE_3 8
    #define RING_SIZE_4 16

#elif BELT

    // I was asked to wear something sparkly once, so I made an LED belt...

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Belt"
    #endif

    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local content

    #define NUM_CHANNELS    1
    #define MATRIX_WIDTH    (1*144)
    #define MATRIX_HEIGHT   1
    #define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
    #define ENABLE_REMOTE   0                     // IR Remote Control
    #define ENABLE_AUDIO    0                     // Listen for audio from the microphone and process it
    #ifndef LED_PIN0
        #define LED_PIN0    17
    #endif
    #define DEFAULT_EFFECT_INTERVAL     (1000*60*60*24)

#elif SPECTRUM

    // This project is set up as a 48x16 matrix of 16x16 WS2812B panels such as: https://amzn.to/3ABs5DK
    // It uses an M5StickCPlus which has a microphone and LCD built in:  https://amzn.to/3CrvCFh
    // It displays a spectrum analyzer and music visualizer

    #ifndef PROJECT_NAME
    #define PROJECT_NAME                    "Spectrum"
    #endif

    #define ENABLE_AUDIOSERIAL              0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI                     1   // Connect to WiFi
    #define WAIT_FOR_WIFI                   0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL               2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER                1   // Turn on the internal webserver
    #define ENABLE_NTP                      1   // Set the clock from the web
    #define ENABLE_OTA                      0   // Accept over the air flash updates
    #define ENABLE_REMOTE                   1   // IR Remote Control
    #define ENABLE_AUDIO                    1   // Listen for audio from the microphone and process it

    #if USE_PSRAM
        #define INCOMING_WIFI_ENABLED       1   // Accepting incoming color data and commands
        #define COLORDATA_SERVER_ENABLED    1
        #define MAX_BUFFERS                 500
    #else
        #define INCOMING_WIFI_ENABLED       0   // Do not accept incoming color data and commands
        #define COLORDATA_SERVER_ENABLED    0
        #define MIN_BUFFERS                 1
        #define MAX_BUFFERS                 1
    #endif

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24*5)

    #ifndef LED_PIN0
        #if SPECTRUM_WROVER_KIT
            #define LED_PIN0                5
        #elif ELECROW
            #define LED_PIN0                19
        #else
            #define LED_PIN0                26
        #endif
    #endif

    #if ELECROW
        #define IR_REMOTE_PIN           20
    #endif

    #define NUM_CHANNELS                1
    #define RING_SIZE_0                 24
    #define BONUS_PIXELS                0
    #define MATRIX_WIDTH                48
    #define MATRIX_HEIGHT               16
    #define NUM_FANS                    MATRIX_WIDTH
    #define FAN_SIZE                    MATRIX_HEIGHT
    #define NUM_BANDS                   16
    #define NUM_LEDS                    (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define LED_FAN_OFFSET_BU           6

    //#define MIN_VU                      400
    //#define NOISE_FLOOR                 30
    //#define NOISE_CUTOFF                5

    #if !(ELECROW)
        #define TOGGLE_BUTTON_1         37
        #define TOGGLE_BUTTON_2         39
    #endif

#elif HELMET

    #ifndef PROJECT_NAME
    #define PROJECT_NAME                    "Helmet"
    #endif

    #define POWER_LIMIT_MW                  1000

    #define ENABLE_AUDIOSERIAL              0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI                     1   // Connect to WiFi
    #define WAIT_FOR_WIFI                   0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL               2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER                1   // Turn on the internal webserver
    #define ENABLE_NTP                      1   // Set the clock from the web
    #define ENABLE_OTA                      0   // Accept over the air flash updates
    #define ENABLE_REMOTE                   1   // IR Remote Control
    #define ENABLE_AUDIO                    1   // Listen for audio from the microphone and process it

    #if USE_PSRAM
        #define INCOMING_WIFI_ENABLED       1   // Accepting incoming color data and commands
        #define COLORDATA_SERVER_ENABLED    1
        #define MAX_BUFFERS                 500
    #else
        #define INCOMING_WIFI_ENABLED       0   // Do not accept incoming color data and commands
        #define COLORDATA_SERVER_ENABLED    0
        #define MIN_BUFFERS                 1
        #define MAX_BUFFERS                 1
    #endif

    #define DEFAULT_EFFECT_INTERVAL         0   // Do not auto-advance unless the button is presssed

    #ifndef LED_PIN0
        #define LED_PIN0                    26
    #endif

    #define NUM_CHANNELS                    1
    #define RING_SIZE_0                     24
    #define BONUS_PIXELS                    0
    #define MATRIX_WIDTH                    32
    #define MATRIX_HEIGHT                   8
    #define NUM_FANS                        MATRIX_WIDTH
    #define FAN_SIZE                        MATRIX_HEIGHT
    #define NUM_BANDS                       16
    #define NUM_LEDS                        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define LED_FAN_OFFSET_BU               6

    #define TOGGLE_BUTTON_1         39
    #define TOGGLE_BUTTON_2         37

#elif FANSET

    // An M5 stick that controls the 10 RGB fans in my PC

    #ifndef PROJECT_NAME
    #define PROJECT_NAME                "Fan set"
    #endif

    #define ENABLE_AUDIOSERIAL          0   // Report peaks at 2400baud on serial port for PETRock consumption
    #define ENABLE_WIFI                 1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED       0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI               0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL           2   // How many seconds before the lamp times out and shows local content
    #define ENABLE_WEBSERVER            1   // Turn on the internal webserver
    #define ENABLE_NTP                  0   // Set the clock from the web
    #define ENABLE_OTA                  0   // Accept over the air flash updates
    #define ENABLE_REMOTE               1   // IR Remote Control
    #define ENABLE_AUDIO                1   // Listen for audio from the microphone and process it
    #define COLORDATA_SERVER_ENABLED    0

    #define MIN_BUFFERS                 1   // Keep buffers low because we have little memory to work with
    #define MAX_BUFFERS                 1

    #define DEFAULT_EFFECT_INTERVAL     (60*60*24*5)

    #ifndef LED_PIN0
        #define LED_PIN0                26
    #endif

    #define BONUS_PIXELS                32  // Extra pixels - in this case, my case strip
    #define NUM_CHANNELS                1   // Everything wired sequentially on a single channel
    #define NUM_FANS                    10  // My system has 10 fans.  Because RGB.
    #define NUM_BANDS                   8
    #define NUM_RINGS                   1   // Fans have a single outer ring of pixels
    #define FAN_SIZE                    16  // Each fan's pixel ring has 16 LEDs
    #define FAN_LEN                     (NUM_FANS * FAN_SIZE)
    #define MATRIX_WIDTH                (NUM_FANS * FAN_SIZE + BONUS_PIXELS)
    #define NUM_LEDS                    (MATRIX_WIDTH)
    #define LED_FAN_OFFSET_BU           3
    #define MATRIX_HEIGHT               1

    // Being case-mounted normally, the FANSET needs a more sensitive mic so the NOISE_CUTOFF value is are lower than spectrum

    #define NOISE_CUTOFF                0
    #define NOISE_FLOOR                 0.0f

    #define TOGGLE_BUTTON_1             37
    #define TOGGLE_BUTTON_2             39

#elif SINGLE_INSULATOR

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Single Insulator"
    #endif

    // A single glass insulator with a 12-pixel ring and then a 7=pixel "bonus" ring in the middle
    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local content

    #define DEFAULT_EFFECT_INTERVAL     (10*60*24)

    #define NUM_CHANNELS    1
    #define BONUS_PIXELS    7
    #define NUM_FANS        1
    #define FAN_SIZE        12
    #define MATRIX_WIDTH    (NUM_FANS * FAN_SIZE + BONUS_PIXELS)
    #define NUM_LEDS        (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define MATRIX_HEIGHT   NUM_FANS

    #define ENABLE_REMOTE   0                     // IR Remote Control
    #define ENABLE_AUDIO    1                     // Listen for audio from the microphone and process it

    #define LED_FAN_OFFSET_BU 6

    #ifndef LED_PIN0
        #if M5STICKC
            #define LED_PIN0 26
        #else
            #define LED_PIN0 5
        #endif
    #endif

#elif INSULATORS

    // A set of 5 Hemmingray glass insulators that each have a ring of 12 LEDs.  Music reactive to the beat.

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Insulators"
    #endif

    #define ENABLE_WIFI             0   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   0   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       0   // How many seconds before the lamp times out and shows local content

    #define DEFAULT_EFFECT_INTERVAL     (0)

    #ifndef LED_PIN0
        #define LED_PIN0      26
    #endif

    #define NUM_CHANNELS      1
    #define RING_SIZE_0       12
    #define BONUS_PIXELS      0
    #define MATRIX_WIDTH      5
    #define MATRIX_HEIGHT     RING_SIZE_0
    #define NUM_FANS          MATRIX_WIDTH
    #define FAN_SIZE          MATRIX_HEIGHT
    #define NUM_LEDS          (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define ENABLE_REMOTE     0                     // IR Remote Control
    #define ENABLE_AUDIO      1                     // Listen for audio from the microphone and process it
    #define IR_REMOTE_PIN     26
    #define LED_FAN_OFFSET_BU 6

    #define TOGGLE_BUTTON_1 37
    #define TOGGLE_BUTTON_2 39

#elif CUBE

    // A cube of 5 x 5 x 5 LEDs

    #ifndef PROJECT_NAME
    #define PROJECT_NAME            "Cube"
    #endif

    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define WAIT_FOR_WIFI           0   // Hold in setup until we have WiFi - for strips without effects
    #define TIME_BEFORE_LOCAL       5   // How many seconds before the cube times out and shows local content
    #define ENABLE_WEBSERVER        1   // Enable the webserver to control the effects

    #define DEFAULT_EFFECT_INTERVAL     (1000 * 60 * 10)    // 10 min

    #ifndef LED_PIN0
        #define LED_PIN0      26
    #endif

    #define NUM_CHANNELS      1
    #define RING_SIZE_0       25                    // Treat each layer as one ring
    #define BONUS_PIXELS      0
    #define MATRIX_WIDTH      5                     // 5 layers
    #define MATRIX_HEIGHT     RING_SIZE_0
    #define NUM_FANS          MATRIX_WIDTH
    #define FAN_SIZE          MATRIX_HEIGHT
    #define NUM_LEDS          (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define ENABLE_REMOTE     0                     // IR Remote Control
    #define ENABLE_AUDIO      1                     // Listen for audio from the microphone and process it
    #define IR_REMOTE_PIN     26
    #define LED_FAN_OFFSET_BU 6
    #define ENABLE_OTA        0

    #define TOGGLE_BUTTON  37
    #define COLOR_ORDER EOrder::RGB

#else

    // This is a simple demo configuration used when no other project is defined; it's only purpose is
    // to serve as a build to be run for [all-deps]

    #define MATRIX_WIDTH            144
    #define MATRIX_HEIGHT           8
    #define NUM_LEDS                (MATRIX_WIDTH*MATRIX_HEIGHT)
    #define NUM_CHANNELS            8
    #define NUM_RINGS               5
    #define RING_SIZE_0             24

    // Once you have a working project, selectively enable various additional features by setting
    // them to 1 in the list below.  This DEMO config assumes no audio (mic), or screen, etc.

    #define ENABLE_AUDIO            1
    #define ENABLE_WIFI             1   // Connect to WiFi
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #define TIME_BEFORE_LOCAL       1   // How many seconds before the lamp times out and shows local content
    #define ENABLE_NTP              1   // Set the clock from the web
    #define ENABLE_OTA              1
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver

    #define LED_PIN0         5
    #define LED_PIN1        16
    #define LED_PIN2        17
    #define LED_PIN3        18
    #define LED_PIN4        32
    #define LED_PIN5        33
    #define LED_PIN6        23
    #define LED_PIN7        22
#endif

#ifndef PROJECT_NAME
#define PROJECT_NAME        "Mesmerizer"
#endif

#if USE_HUB75
#include "MatrixHardware_ESP32_Custom.h"
#define SM_INTERNAL     // Silence build messages from their header
#include <SmartMatrix.h>
#endif

#if ENABLE_AUDIOSERIAL
#ifndef SERIAL_PINRX
    #define SERIAL_PINRX    33
    #define SERIAL_PINTX    32
#endif
#endif

#define STACK_SIZE (ESP_TASK_MAIN_STACK) // Stack size for each new thread
#define TIME_CHECK_INTERVAL_MS (1000 * 60 * 5)   // How often in ms we resync the clock from NTP
#define MAX_RINGS       5


// Default Settings
//
// Set rest of things to reasonable defaults that were not specified by the project config above.

#ifndef MIN_BUFFERS
#define MIN_BUFFERS 3               // How many copies of LED buffers this board will keep at a minimum per strand
#endif

#ifndef MAX_BUFFERS
#define MAX_BUFFERS (180)           // Six seconds at 30fps
#endif

#ifndef ENABLE_WEBSERVER
#define ENABLE_WEBSERVER        0   // Chip provides a web server with controls to adjust effects
#endif

#if ENABLE_WEBSERVER
    #ifndef ENABLE_WEB_UI
    #define ENABLE_WEB_UI           1   // Enable HTTP pathnames for the web UI
    #endif
#endif

#ifndef ENABLE_OTA
#define ENABLE_OTA              1   // Listen for over the air update to the flash
#endif

#ifndef ENABLE_ESPNOW
#define ENABLE_ESPNOW           0   // Listen for ESPNOW packets
#endif

#ifndef ENABLE_NTP
#define ENABLE_NTP              1   // Update the clock from NTP
#endif

#ifndef NUM_LEDS
#define NUM_LEDS (MATRIX_HEIGHT * MATRIX_WIDTH)
#endif

#ifndef FAN_SIZE                // How man LEDs around the circumference
#define FAN_SIZE 1
#define NUM_FANS NUM_LEDS
#endif

#if ENABLE_AUDIO
    #ifndef NUM_BANDS              // How many bands in the spectrum analyzer
        #define NUM_BANDS 16
    #endif
    #ifndef NOISE_FLOOR
        #define NOISE_FLOOR 30
    #endif
    #ifndef NOISE_CUTOFF
        #define NOISE_CUTOFF 10
    #endif
    #ifndef AUDIO_MIC_SCALAR
        #define AUDIO_MIC_SCALAR 1.0
    #endif
    #ifndef AUDIO_PEAK_REMOTE_TIMEOUT
        #define AUDIO_PEAK_REMOTE_TIMEOUT 1000.0f       // How long after remote PeakData before local microphone is used again
    #endif
    #ifndef ENABLE_AUDIO_SMOOTHING
        #define ENABLE_AUDIO_SMOOTHING 1
    #endif
    #ifndef BARBEAT_ENHANCE
        #define BARBEAT_ENHANCE 0.3                     // How much the SpectrumAnalyzer "pulses" with the music
    #endif
    #ifndef SPECTRUMBARBEAT_ENHANCE
        #define SPECTRUMBARBEAT_ENHANCE 0.75            // How much the SpectrumBar effect "pulses" with the music
    #endif
    #ifndef VU_REACTIVITY_RATIO
        #define VU_REACTIVITY_RATIO 10.0                // How much the VU meter reacts to the music going up vs down
    #endif
#endif


#ifndef LED_PIN0                // Which pin the LEDs are connected to
#define LED_PIN0 5
#endif

#ifndef NUM_RINGS               // How many rings in each tree/insulator/etc
#define NUM_RINGS 1
#endif

#ifndef COLOR_ORDER
#define COLOR_ORDER EOrder::GRB
#endif

// Define fan ordering for drawing into the fan directionally

#ifndef LED_FAN_OFFSET_LR
#define LED_FAN_OFFSET_LR  (LED_FAN_OFFSET_BU + (FAN_SIZE * 1 / 4))         // High level stuff right here!
#endif

#ifndef LED_FAN_OFFSET_TD
#define LED_FAN_OFFSET_TD  (LED_FAN_OFFSET_BU + (FAN_SIZE * 2 / 4))
#endif

#ifndef LED_FAN_OFFSET_RL
#define LED_FAN_OFFSET_RL  (LED_FAN_OFFSET_BU + (FAN_SIZE * 3 / 4))
#endif

#ifndef NUM_FANS
#define NUM_FANS 1
#endif

#ifndef RING_SIZE_0
#define RING_SIZE_0 FAN_SIZE
#endif

#ifndef RING_SIZE_1
#define RING_SIZE_1 FAN_SIZE
#endif

#ifndef RING_SIZE_2
#define RING_SIZE_2 FAN_SIZE
#endif

#ifndef RING_SIZE_3
#define RING_SIZE_3 FAN_SIZE
#endif

#ifndef RING_SIZE_4
#define RING_SIZE_4 FAN_SIZE
#endif

#ifndef DEFAULT_EFFECT_INTERVAL
#define DEFAULT_EFFECT_INTERVAL 1000*30
#endif

#ifndef MILLIS_PER_FRAME
#define MILLIS_PER_FRAME 0
#endif

#ifndef LED_FAN_OFFSET_BU
#define LED_FAN_OFFSET_BU 0
#endif

#ifndef RESERVE_MEMORY
  #ifdef USE_PSRAM
    #define RESERVE_MEMORY 1000000
  #else
    #define RESERVE_MEMORY 150000
  #endif
#endif

#ifndef TIME_BEFORE_LOCAL
#define TIME_BEFORE_LOCAL 5
#endif

#ifndef ENABLE_REMOTE
#define ENABLE_REMOTE 0
#endif

#ifndef EFFECT_PERSISTENCE_CRITICAL
#define EFFECT_PERSISTENCE_CRITICAL 0
#endif

#ifndef MATRIX_REFRESH_RATE
#define MATRIX_REFRESH_RATE 180
#endif

#ifndef MATRIX_CALC_DIVIDER
#define MATRIX_CALC_DIVIDER 3
#endif

// Power Limit
//
// The maximum amount of power, in milliwatts, that you want your project to use, if you want to limit that.
// If your demands exceed this in practice, the code will try to scale back brightness to hit this.
// Don't rely on this for safety! Instead, design your hardware to protect against it with a fuse, etc.
// Another way to limit power usage is through the Brightness setting that is contained by the DeviceConfig
// class. Again, this should not be your primary/only means to protect against the overdraft of power.
// If POWER_LIMIT_MW is unset and DeviceConfig's Brightness is set to maximum, the amount of power drawn will
// not be limited at the software level.

// #define POWER_LIMIT_MW 500*5                 // Define for your power draw limit. Example is a low 2500mA
                                                // which may dim your LEDs quite a lot.

// Display
//
// Enable USE_OLED or USE_TFT based on selected board definition
// These board definations are added by platformio

#if USE_SCREEN

    #if ARDUINO_HELTEC_WIFI_KIT_32
                        // screen definations for heltec_wifi_kit_32 or heltec_wifi_kit_32_v2

        #define USE_OLED 1                                    // Enable the Heltec's monochrome OLED
        #if !(USE_SSD1306)
            #define NUM_INFO_PAGES 1        // Only display "BasicInfoSummary" if not SSD1306
        #endif

    #elif USE_M5                                        // screen definitions for m5stick-c-plus

        #define USE_M5DISPLAY 1                               // enable the M5's LCD screen

    #elif USE_TFTSPI || ESP32FEATHERTFT || PANLEE || LILYGOTDISPLAYS3

        #define USE_TFTSPI 1                                  // Use TFT_eSPI

    #elif WROVERKIT || SPECTRUM_WROVER_KIT

        #define USE_LCD 1                                      // Use the ILI9341 onboard

    #elif TTGO

        #define USE_TFTSPI 1                                  // Use TFT_eSPI

    #elif ELECROW

        // Implies ElecrowScreen

    #elif AMOLED_S3

        // Implicitly uses LilyGoScreen3

    #else                                                     // unsupported board defined in platformio
        #error Unknown Display! Check platformio.ini board definition.
    #endif

#endif // end USE_SCREEN

#if USE_LCD
    // These pins are based on the Espressif WROVER-KIT, which uses an ILI9314 chipset for its display
    // connected as follows:
    #define TFT_CS      22
    #define TFT_DC      21
    #define TFT_MOSI    23
    #define TFT_SCK     19
    #define TFT_RST     18
    #define TFT_MISO    25
    #define TFT_WIDTH   240
    #define TFT_HEIGHT  320
#endif

#if AMOLED_S3
    #define TFT_WIDTH   240
    #define TFT_HEIGHT  536
#endif


#ifdef ESP32FEATHERTFT
// Commented out because FastLED crashes if the onboard pixel is used
//    #define ONBOARD_PIXEL_ORDER     EOrder::RGB
//    #define ONBOARD_PIXEL_POWER     34
//    #define ONBOARD_PIXEL_DATA      33
#endif

#ifndef USE_OLED
#define USE_OLED 0
#endif

#ifndef USE_M5DISPLAY
#define USE_M5DISPLAY 0
#endif

#ifndef USE_LCD
#define USE_LCD 0
#endif

#ifndef USE_TFTSPI
#define USE_TFTSPI 0
#endif

#ifndef CAPTION_TIME
#define CAPTION_TIME 3000
#endif

#ifndef MATRIX_CENTER_X
#define MATRIX_CENTER_X ((MATRIX_WIDTH + 1) / 2)
#endif

#ifndef MATRIX_CENTER_Y
#define MATRIX_CENTER_Y ((MATRIX_HEIGHT + 1) / 2)
#endif

#ifndef NUM_INFO_PAGES
#define NUM_INFO_PAGES 2
#endif

// When you press a color button on the remote, the color is used to create a temporary fill effect, but
// only when this is set to 1.  Otherwise, just the global colors are set, and it's up to the active effect
// to actually use them.

#ifndef FULL_COLOR_REMOTE_FILL
#define FULL_COLOR_REMOTE_FILL 0
#endif

// Convenience shortcut to avoid cluttering all the above with #if pretzels.
// This allows single, easy setting from either a platform.ini (or override)
// or allowing this to be set from the command line build. e.g.:
// $ PLATFORMIO_BUILD_FLAGS="-DLED_PIN0=14 -DUSE_ALL_NETWORKING" \
//   pio run --target upload -e lilygo-tdisplay-s3-demo

#if USE_ALL_NETWORKING
    #undef ENABLE_WIFI
    #define ENABLE_WIFI             1   // Connect to WiFi

    #undef INCOMING_WIFI_ENABLED
    #define INCOMING_WIFI_ENABLED   1   // Accepting incoming color data and commands
    #undef ENABLE_NTP
    #define ENABLE_NTP              1   // Set the clock from the web

    #undef ENABLE_OTA
    #define ENABLE_OTA              1

    #undef ENABLE_WEBSERVER
    #define ENABLE_WEBSERVER        1   // Turn on the internal webserver

    #undef ENABLE_WEB_UI
    #define ENABLE_WEB_UI           1   // Enable HTTP pathnames for the web UI
#endif

// Common globals

// g_aRingSizeTable
//
// Items with rings must provide a table indicating how big each ring is.  If an insulator had 60 LEDs grouped
// into rings of 30, 20, and 10, you'd have (NUM_RINGS = 3) and this table would contain (30, 20, 10).

extern DRAM_ATTR const int g_aRingSizeTable[];

#ifndef MICROS_PER_SECOND
    #define MICROS_PER_SECOND 1000000
#endif

#define MILLIS_PER_SECOND   1000
#define MICROS_PER_MILLI    1000

#ifndef M5STICKC
#define M5STICKC 0
#endif

#ifndef M5STICKCPLUS
#define M5STICKCPLUS 0
#endif

#ifndef M5STICKCPLUS2
#define M5STICKCPLUS2 0
#endif

#ifndef M5STACKCORE2
#define M5STACKCORE2 0
#endif

#ifndef COLORDATA_SERVER_ENABLED
  #if ENABLE_WIFI
    #define COLORDATA_SERVER_ENABLED 1
  #else
    #define COLORDATA_SERVER_ENABLED 0
  #endif
#endif

#ifndef COLORDATA_WEB_SOCKET_ENABLED
  #if ENABLE_WIFI && ENABLE_WEBSERVER && COLORDATA_SERVER_ENABLED
    #define COLORDATA_WEB_SOCKET_ENABLED 1
  #else
    #define COLORDATA_WEB_SOCKET_ENABLED 0
  #endif
#endif

#ifndef EFFECTS_WEB_SOCKET_ENABLED
  #if ENABLE_WIFI && ENABLE_WEBSERVER
    #define EFFECTS_WEB_SOCKET_ENABLED 1
  #else
    #define EFFECTS_WEB_SOCKET_ENABLED 0
  #endif
#endif

// Microphone
//
// The M5 mic is on Pin34, but when I wire up my own microphone module I usually put it on pin 36.

#if ENABLE_AUDIO
    #ifndef INPUT_PIN
        #if TTGO
            #define INPUT_PIN (36)
        #elif ELECROW
            #define INPUT_PIN (41)
        #elif USE_M5
            #define INPUT_PIN (34)
            #define IO_PIN (0)
        #else
            #define INPUT_PIN (36)    // Audio line input, ADC #1, input line 0 (GPIO pin 36)
        #endif
    #endif
#else
    #define INPUT_PIN 0
#endif

#ifndef IR_REMOTE_PIN
#define IR_REMOTE_PIN   25
#endif

// Set and use for I2S input.
#if USE_I2S_AUDIO_PINS || ELECROW
  // Bit clock
  #ifndef I2S_BCLK_PIN
    #define I2S_BCLK_PIN   39
  #endif

  // Word select clock.
  #ifndef I2S_WS_PIN
    #define I2S_WS_PIN     38
  #endif

  #ifndef I2S_DATA_PIN
    #define I2S_DATA_PIN     INPUT_PIN
  #endif
#endif



// Custom WiFi Commands
//
// A Wifi packet can contain color data or potentially other info, like a clock.  Or it could be
// a stats request.  Beyond color data these are poorly tested and likely can be removed, though
// stats and clock are handy for debugging!

#define WIFI_COMMAND_PIXELDATA64 3             // Wifi command with color data and 64-bit clock vals
#define WIFI_COMMAND_PEAKDATA    4             // Wifi command that delivers audio peaks

// Final headers
//
// Headers that are only included when certain features are enabled


// FPS
//
// Given a time value for when the last frame took place and the current timestamp returns the number of
// frames per second, as low as 0.  Never exceeds 999 so you can make some width assumptions.

inline int FPS(unsigned long duration, uint32_t perSecond = MILLIS_PER_SECOND)
{
    if (duration == 0)
        return 999;

    float fpsf = 1.0f / (duration / (float)perSecond);
    int FPS = (int)fpsf;
    if (FPS > 999)
        FPS = 999;
    return FPS;
}

// str_snprintf
//
// va-args style printf that returns the formatted string as a result

// Let compiler warn if our arguments don't match.
inline String str_sprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

inline String str_sprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);

    // BUGBUG: Investigate a vasprintf here and String::copy() to get move semantics
    // on the return.
    // Could Save one complete format, a copy, and an alloc and we're called a
    // few times a second.
    int requiredLen = vsnprintf(nullptr, 0, fmt, args) + 1;
    va_end(args);

    if (requiredLen <= 0) {
        va_end(args_copy);
        return {};
    }

    std::unique_ptr<char []> str = std::make_unique<char []>(requiredLen);
    vsnprintf(str.get(), requiredLen, fmt, args_copy);
    va_end(args_copy);

    String retval;
    retval.reserve(requiredLen); // At least saves one scan of the buffer.

    retval = str.get();
    return retval;
}

#include "types.h"

// C Helpers
//
// Simple inline utility functions like random numbers, mapping, conversion, etc

#include <random>
#include <type_traits>

template <typename T>
inline static T random_range(T lower, T upper)
{
#if USE_STRONG_RAND
    static_assert(std::is_arithmetic<T>::value, "Template argument must be numeric type");

    static std::random_device rd;
    static std::mt19937 gen(rd());

    if constexpr (std::is_integral<T>::value) {
        std::uniform_int_distribution<T> distrib(lower, upper);
        return distrib(gen);
    } else if constexpr (std::is_floating_point<T>::value) {
        std::uniform_real_distribution<T> distrib(lower, upper);
        return distrib(gen);
    }
#else
    static bool seeded = [&] { srand(time(nullptr)); return true; } ();

    if constexpr (std::is_integral<T>::value) {
        return std::rand() % (upper - lower + 1) + lower;
    } else if constexpr (std::is_floating_point<T>::value) {
        return std::rand() / (RAND_MAX / (upper - lower)) + lower;
    } else {
        static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value, "Template argument must be numeric type");
    }
#endif
}

inline uint64_t ULONGFromMemory(const uint8_t * payloadData)
{
    return  (uint64_t)payloadData[7] << 56  |
            (uint64_t)payloadData[6] << 48  |
            (uint64_t)payloadData[5] << 40  |
            (uint64_t)payloadData[4] << 32  |
            (uint64_t)payloadData[3] << 24  |
            (uint64_t)payloadData[2] << 16  |
            (uint64_t)payloadData[1] << 8   |
            (uint64_t)payloadData[0];
}

inline uint32_t DWORDFromMemory(const uint8_t * payloadData)
{
    return  (uint32_t)payloadData[3] << 24  |
            (uint32_t)payloadData[2] << 16  |
            (uint32_t)payloadData[1] << 8   |
            (uint32_t)payloadData[0];
}

inline uint16_t WORDFromMemory(const uint8_t * payloadData)
{
    return  (uint16_t)payloadData[1] << 8   |
            (uint16_t)payloadData[0];
}

// SetSocketBlockingEnabled
//
// In blocking mode, socket API calls wait until the operation is complete before returning control to the application.
// For example, a call to the send() function won't return until all data has been sent. This can lead to the application
// hanging if the operation takes a long time.

// In contrast, in non-blocking mode, socket API calls return immediately. If an operation cannot be completed immediately, the function returns an error (usually EWOULDBLOCK or EAGAIN). The application can then decide how to handle the situation, such as by retrying the operation later. This provides more control and can make the application more responsive. However, it also requires more sophisticated programming, as the application must be prepared to handle these error conditions.

inline bool SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return false;
   flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}

// formatSize
//
// Returns a string with the size formatted in a human-readable format.
// For example, 1024 becomes "1K", 1000*1000 becomes "1M", etc.
// It pains me not to use 1024, but such are the times we live in.

inline String formatSize(size_t size, size_t threshold = 1000)
{
    // If the size is above the threshold, we want a precision of 2 to show more accurate value
    const int precision = size < threshold ? 0 : 2;

    const char* suffixes[] = {"", "K", "M", "G", "T", "P", "E", "Z"};
    size_t suffixIndex = 0;
    double sizeDouble = static_cast<double>(size);

    while (sizeDouble >= threshold && suffixIndex < (sizeof(suffixes) / sizeof(suffixes[0])) - 1)
    {
        sizeDouble /= 1000;
        ++suffixIndex;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << sizeDouble << suffixes[suffixIndex];
    std::string result = oss.str();  // Store the result to avoid dangling pointer
    return String(result.c_str());
}


// to_array
//
// Because the ESP32 compiler, as of this writing, doesn't have std::to_array, we provide our own (davepl).
// BUGBUG: Once we have compiler support we should use the C++20 versions

template <typename T, std::size_t N>
constexpr std::array<T, N> to_array(const T (&arr)[N]) {
    std::array<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = arr[i];
    }
    return result;
}

// 16-bit (5:6:5) color definitions for common colors

#define BLACK16     0x0000
#define BLUE16      0x001F
#define RED16       0xF800
#define GREEN16     0x07E0
#define CYAN16      0x07FF
#define MAGENTA16   0xF81F
#define YELLOW16    0xFFE0
#define WHITE16     0xFFFF

// Main includes

#include "gfxbase.h"                            // GFXBase drawing interface
#include "socketserver.h"                       // Incoming WiFi data connections
#include "ledstripgfx.h"                        // Essential drawing code for strips
#include "ledstripeffect.h"                     // Defines base led effect classes
#include "ntptimeclient.h"                      // setting the system clock from ntp
#include "effectmanager.h"                      // For g_EffectManager
#include "ledbuffer.h"                          // Buffer manager for strip
#include "colordata.h"                          // color palettes

#if USE_TFTSPI
    #define DISABLE_ALL_LIBRARY_WARNINGS 1
    #include <TFT_eSPI.h>
    #include <SPI.h>
#endif
