#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP_DoubleResetDetector.h>
#include <LittleFS.h>
#include <WiFiManager.h>

#include "WifiSetup.h"

static WiFiManager wm;

static char moonrakerHost[40] = "192.168.0.50";
static char moonrakerPort[6]  = "7125";

static WiFiManagerParameter custom_host("host", "Moonraker Host/IP", moonrakerHost, 40);
static WiFiManagerParameter custom_port("port", "Moonraker Port",    moonrakerPort, 6);

static bool rebootPending = false;
static unsigned long rebootAt = 0;

// load params from LittleFS (flash)
static bool loadConfig() {
    if (!LittleFS.begin()) return false;
    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    strlcpy(moonrakerHost, doc["host"] | "", sizeof(moonrakerHost));
    strlcpy(moonrakerPort, doc["port"] | "7125", sizeof(moonrakerPort));
    f.close();
    return true;
}

// save params to LittleFS (flash)
static void saveConfig() {
    JsonDocument doc;
    doc["host"] = moonrakerHost;
    doc["port"] = moonrakerPort;
    File f = LittleFS.open("/config.json", "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

// callback from wm to save params and restart
static void saveParamsCallback() {
    strlcpy(moonrakerHost, custom_host.getValue(), sizeof(moonrakerHost));
    strlcpy(moonrakerPort, custom_port.getValue(), sizeof(moonrakerPort));
    saveConfig();
    Serial.printf("Saved Moonraker: %s:%s\n", moonrakerHost, moonrakerPort);
    rebootPending = true;
    rebootAt = millis() + 2000;
}

void wifi_factoryReset() {
    wm.resetSettings();
    LittleFS.remove("/config.json");
}

void wifi_begin() {
    LittleFS.begin();

    loadConfig();
    custom_host.setValue(moonrakerHost, sizeof(moonrakerHost));
    custom_port.setValue(moonrakerPort, sizeof(moonrakerPort));

    wm.addParameter(&custom_host);
    wm.addParameter(&custom_port);
    wm.setSaveParamsCallback(saveParamsCallback);

    bool ok = wm.autoConnect("MoonCat-Setup");
    if (!ok) {
        Serial.println(F("Config/Connect failed -> reboot!"));
        ESP.restart();
    }
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());

    wm.startWebPortal();
}

void wifi_loop() {
    wm.process();
    
    if (rebootPending && millis() >= rebootAt) {
        ESP.restart();
    }
}

const char* wifi_moonraker_host() { return moonrakerHost; }
uint16_t    wifi_moonraker_port() { return atoi(moonrakerPort); }
