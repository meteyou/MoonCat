#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP_DoubleResetDetector.h>
#include <NeoPixelBus.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <WebSocketsClient.h>
#include <WiFiManager.h>

DoubleResetDetector* drd;
WebSocketsClient webSocket;

// define the pixel count and strip type for the NeoPixel
const uint16_t PixelCount = 1;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount);

// define the I2C address and create the U8G2 object for the OLED display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

char moonrakerHost[40] = "192.168.0.50";
char moonrakerPort[6]  = "7125";

WiFiManager wm;

WiFiManagerParameter custom_host("host", "Moonraker Host/IP", moonrakerHost, 40);
WiFiManagerParameter custom_port("port", "Moonraker Port",    moonrakerPort, 6);

// planned reboot variables
bool rebootPending = false;
unsigned long rebootAt = 0;

// debug output for mem leak
unsigned long lastHeapLog = 0;

// led wave variables
unsigned long lastFrame = 0;
const unsigned long interval = 20;

bool displayDirty = false;

// PrinterStatus enum
enum class PrinterStatus { Standby, Printing, Paused, Complete, Error, Offline };

PrinterStatus stateToStatus(const char* state) {
    if (strcmp(state, "printing") == 0)  return PrinterStatus::Printing;
    if (strcmp(state, "paused") == 0)    return PrinterStatus::Paused;
    if (strcmp(state, "complete") == 0)  return PrinterStatus::Complete;
    if (strcmp(state, "cancelled") == 0) return PrinterStatus::Error;
    if (strcmp(state, "error") == 0)     return PrinterStatus::Error;
    if (strcmp(state, "standby") == 0)   return PrinterStatus::Standby;

    return PrinterStatus::Offline;
}

// current printer stats
bool klippyReady = false;
PrinterStatus currentStatus = PrinterStatus::Offline;
float currentProgress = 0.0;

// load params from LittleFS (flash)
bool loadConfig() {
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
void saveConfig() {
    JsonDocument doc;
    doc["host"] = moonrakerHost;
    doc["port"] = moonrakerPort;

    File f = LittleFS.open("/config.json", "w");
    if (!f) return;

    serializeJson(doc, f);
    f.close();
}

// callback function from wm to save params and restart
void saveParamsCallback() {
    strlcpy(moonrakerHost, custom_host.getValue(), sizeof(moonrakerHost));
    strlcpy(moonrakerPort, custom_port.getValue(), sizeof(moonrakerPort));
    saveConfig();
    Serial.printf("Saved Moonraker: %s:%s\n", moonrakerHost, moonrakerPort);

    // set a reboot after 2s
    rebootPending = true;
    rebootAt = millis() + 2000;
}

RgbColor pulse(RgbColor base, uint16_t periodMs) {
    float phase = (millis() % periodMs) / (float)periodMs;
    float b = (sin(phase * 2 * PI) + 1) / 2;   // 0..1
    return base.Dim(b * 255);
}

void renderLed() {
    switch (currentStatus) {
        case PrinterStatus::Printing:
            strip.SetPixelColor(0, RgbColor(255, 255, 255));  // white
            break;
        case PrinterStatus::Paused:
            strip.SetPixelColor(0, pulse(RgbColor(255, 255, 0), 2000));  // yellow
            break;
        case PrinterStatus::Complete:
            strip.SetPixelColor(0, RgbColor(0, 255, 0));  // green
            break;
        case PrinterStatus::Error:
            strip.SetPixelColor(0, pulse(RgbColor(255, 0, 0), 2000));  // red
            break;
        case PrinterStatus::Standby:
            strip.SetPixelColor(0, pulse(RgbColor(128, 0, 128), 2000));  // purple
            break;
        case PrinterStatus::Offline:
            strip.SetPixelColor(0, RgbColor(0, 0, 0));  // black/off
            break;
    }
    strip.Show();
}

void updateDisplay() {
    char line[24];
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 12, "MoonCat");
    snprintf(line, sizeof(line), "Progress: %d%%", (int)(currentProgress * 100));
    u8g2.drawStr(2, 40, line);
    u8g2.sendBuffer();
}

void subscribe() {
    webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"printer.objects.subscribe","params":{"objects":{"print_stats":null,"display_status":null,"virtual_sdcard":null}},"id":1})");
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            Serial.println(F("WS connected"));
            webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"server.info","id":2})");
            subscribe();
            break;
        }

        case WStype_DISCONNECTED: {
            Serial.println(F("WS disconnected"));
            klippyReady = false;
            currentStatus = PrinterStatus::Offline;
            displayDirty = true;
            break;
        }

        case WStype_TEXT: {
            char* msg = (char*)payload;

            // ignore proc stat
            if (strstr(msg, "notify_proc_stat_update")) return;

            if (strstr(msg, "notify_klippy_shutdown") || strstr(msg, "notify_klippy_disconnected")) {
                currentStatus = PrinterStatus::Error;
                displayDirty = true;
                klippyReady = false;
                return;
            }

            if (strstr(msg, "notify_klippy_ready")) {
                subscribe();
                klippyReady = true;
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, msg)) return;

            JsonObject status;
            // init data after subscribe
            if (doc["id"] == 1) {
                status = doc["result"]["status"];

            // server.info response
            } else if (doc["id"] == 2) {
                const char* klippy_state = doc["result"]["klippy_state"];
                klippyReady = (klippy_state && strcmp(klippy_state, "ready") == 0);
            
            // delta status update
            } else if (strstr(msg, "notify_status_update")) {
                status = doc["params"][0];

            // ignore all other notify messages
            } else { return; }

            // update currentStatus if it exists
            if (!status["print_stats"]["state"].isNull()) {
                currentStatus = stateToStatus(status["print_stats"]["state"]);
                displayDirty = true;

                if (!klippyReady) {
                    currentStatus = PrinterStatus::Error;
                }
            }

            // update progress if it exists
            if (!status["display_status"]["progress"].isNull()) {
                currentProgress = status["display_status"]["progress"];
                displayDirty = true;
            }

            break;
        }
        
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    LittleFS.begin();

    // reset wifi manager and LittleFS after double reset button
    drd = new DoubleResetDetector(3, 0);
    if (drd->detectDoubleReset()) {
        Serial.println(F("Double-Reset -> Config-Portal"));
        wm.resetSettings();
        LittleFS.remove("/config.json");
    }

    // clear neopixel
    strip.Begin();
    strip.Show();

    // init lcd
    u8g2.begin();
    u8g2.setBusClock(100000);

    // load config form flash and set globale vars
    loadConfig();
    custom_host.setValue(moonrakerHost, sizeof(moonrakerHost));
    custom_port.setValue(moonrakerPort, sizeof(moonrakerPort));

    // add parameter to wifi manager
    wm.addParameter(&custom_host);
    wm.addParameter(&custom_port);
    // set callback to save params to flash 
    wm.setSaveParamsCallback(saveParamsCallback);

    // init wifi manager and config auto connect (hotspot) function
    bool ok = wm.autoConnect("MoonCat-Setup");
    if (!ok) {
        Serial.println(F("Config/Connect failed -> reboot!"));
        ESP.restart();
    }
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());

    // start non blocking webserver for host & port settings
    wm.startWebPortal();

    Serial.printf("Connect to Moonraker: ws://%s:%s/websocket\n", moonrakerHost, moonrakerPort);
    webSocket.begin(moonrakerHost, atoi(moonrakerPort), "/websocket");
    webSocket.setExtraHeaders("");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);

    Serial.printf("Setup complete, heap=%u\n", ESP.getFreeHeap());
}

void loop() {
    // for double reset button
    drd->loop();

    // execute webserver
    wm.process();

    // restart ESP, when a pending reboot exists and time over
    if (rebootPending && millis() >= rebootAt) {
        ESP.restart();
    }

    // execute websocket
    webSocket.loop();

    if (displayDirty) {
        displayDirty = false;
        updateDisplay();
    }

    if (millis() - lastFrame >= interval) {
        lastFrame = millis();
        renderLed();
    }

    if (millis() - lastHeapLog >= 30000) {     // alle 30s
        lastHeapLog = millis();
        Serial.printf("heap=%u  frag=%u%%  maxblock=%u\n",
            ESP.getFreeHeap(),
            ESP.getHeapFragmentation(),
            ESP.getMaxFreeBlockSize());
   }
}
