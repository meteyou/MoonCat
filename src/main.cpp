#include <Arduino.h>
#include <ESP_DoubleResetDetector.h>
#include <LittleFS.h>

#include "Display.h"
#include "LedIndicator.h"
#include "MoonrakerClient.h"
#include "PrinterState.h"
#include "WifiSetup.h"

DoubleResetDetector* drd;

// debug output for mem leak
unsigned long lastHeapLog = 0;

// led wave variables
unsigned long lastFrame = 0;
const unsigned long interval = 20;

void setup() {
    Serial.begin(115200);
    LittleFS.begin();

    // reset wifi manager and LittleFS after double reset button
    drd = new DoubleResetDetector(3, 0);
    if (drd->detectDoubleReset()) {
        Serial.println(F("Double-Reset -> Config-Portal"));
        wifi_factoryReset();
    }

    wifi_begin();
    led_begin();
    display_begin();
    moonraker_begin(wifi_moonraker_host(), wifi_moonraker_port());

    Serial.printf("Setup complete, heap=%u\n", ESP.getFreeHeap());
}

void loop() {
    // for double reset button
    drd->loop();

    // execute wifi-setup loop
    wifi_loop();

    // execute websocket
    moonraker_loop();

    if (printerState.dirty) {
        printerState.dirty = false;
        display_render();
    }

    if (millis() - lastFrame >= interval) {
        lastFrame = millis();
        led_render();
    }

    if (millis() - lastHeapLog >= 30000) {
        lastHeapLog = millis();
        Serial.printf("heap=%u  frag=%u%%  maxblock=%u\n",
            ESP.getFreeHeap(),
            ESP.getHeapFragmentation(),
            ESP.getMaxFreeBlockSize());
   }
}
