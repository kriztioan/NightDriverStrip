#define PROJECT_NAME "Bookcase"

#define MATRIX_WIDTH 107
#define MATRIX_HEIGHT 1
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define NUM_CHANNELS 1
#define ENABLE_AUDIO 0

#define POWER_LIMIT_MW 25 * 5 * 1000 // 25 amp supply at 5 volts assumed

#define ENABLE_WIFI 1 // Connect to WiFi

#define INCOMING_WIFI_ENABLED 0     // Accepting incoming color data and commands
#define TIME_BEFORE_LOCAL 0         // How many seconds before the lamp times out and shows local content
#define ENABLE_NTP 1                // Set the clock from the web
#define ENABLE_OTA 0                // Accept over the air flash updates

#define LED_PIN0 5

// The webserver serves files that are baked into the device firmware. When running you should be able to
// see/select the list of effects by visiting the chip's IP in a browser.  You can get the chip's IP by
// watching the serial output or checking your router for the DHCP given to a new device; often they're
// named "esp32-" followed by a seemingly random 6-digit hexadecimal number.

#define ENABLE_WEBSERVER 1 // Turn on the internal webserver