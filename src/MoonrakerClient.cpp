#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <WebSocketsClient.h>

#include "Display.h"
#include "MoonrakerClient.h"
#include "PrinterState.h"

static WebSocketsClient webSocket;

static void sendObjectsList() {
    webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"printer.objects.list","id":4})");
}

static void sendMainsailDbRequest() {
    webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"server.database.get_item","params":{"namespace":"mainsail","key":"general.printername"},"id":3})");
}

static void subscribe() {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\","
        "\"method\":\"printer.objects.subscribe\","
        "\"params\":{\"objects\":{"
            "\"print_stats\":null,"
            "\"display_status\":null,"
            "\"virtual_sdcard\":null,"
            "\"extruder\":[\"temperature\",\"target\"]"
            "%s"
        "}},\"id\":1}",
        printerState.hasHeaterBed ? ",\"heater_bed\":[\"temperature\",\"target\"]" : ""
    );
    webSocket.sendTXT(buf);
}

static void updateTemp(JsonVariant src, float& dst) {
    if (src.isNull()) return;

    float newVal = src.as<float>();
    if ((int)(newVal + 0.5f) != (int)(dst + 0.5f)) {
        display_makeDirty();
    }
    dst = newVal;
}

static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            Serial.println(F("WS connected"));
            webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"server.info","id":2})");
            sendMainsailDbRequest();
            sendObjectsList();
            break;
        }

        case WStype_DISCONNECTED: {
            Serial.println(F("WS disconnected"));
            printerState.klippyReady = false;
            printerState.status = PrinterStatus::Offline;
            display_computeView();
            break;
        }

        case WStype_TEXT: {
            char* msg = (char*)payload;

            // ignore proc stat
            if (strstr(msg, "notify_proc_stat_update")) return;

            if (strstr(msg, "notify_klippy_shutdown") || strstr(msg, "notify_klippy_disconnected")) {
                printerState.status = PrinterStatus::Error;
                display_computeView();
                display_makeDirty();
                printerState.klippyReady = false;
                return;
            }

            if (strstr(msg, "notify_klippy_ready")) {
                sendObjectsList();
                printerState.klippyReady = true;
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, msg)) return;

            // server.info response for init klippy state
            if (doc["id"] == 2) {
                const char* klippy_state = doc["result"]["klippy_state"];
                printerState.klippyReady = (klippy_state && strcmp(klippy_state, "ready") == 0);

                return;
            }

            // get mainsail printer name from moonraker db
            if (doc["id"] == 3) {
                const char* name = doc["result"]["value"];
                if (name && name[0] != '\0') {
                    strlcpy(printerState.printerName, name, sizeof(printerState.printerName));
                    display_makeDirty();
                }
                
                return;
            }

            // get printer objects list
            if (doc["id"] == 4) {
                printerState.hasHeaterBed = (strstr(msg, "\"heater_bed\"") != nullptr);
                subscribe();
                return;
            }

            JsonObject status;
            // init data after subscribe
            if (doc["id"] == 1) {
                status = doc["result"]["status"];
            
            // delta status update
            } else if (strstr(msg, "notify_status_update")) {
                status = doc["params"][0];

            // ignore all other notify messages
            } else { return; }

            // update currentStatus if it exists
            if (!status["print_stats"]["state"].isNull()) {
                printerState.status = stateToStatus(status["print_stats"]["state"]);

                if (!printerState.klippyReady) {
                    printerState.status = PrinterStatus::Error;
                }

                display_computeView();
                display_makeDirty();
            }

            // update progress if it exists
            if (!status["display_status"]["progress"].isNull()) {
                printerState.progress = status["display_status"]["progress"];
                display_makeDirty();
            }

            updateTemp(status["extruder"]["temperature"],   printerState.extruderTemp);
            updateTemp(status["extruder"]["target"],        printerState.extruderTarget);
            updateTemp(status["heater_bed"]["temperature"], printerState.bedTemp);
            updateTemp(status["heater_bed"]["target"],      printerState.bedTarget);

            break;
        }
        
        default:
            break;
    }
}

void moonraker_begin(const char* host, uint16_t port) {
    Serial.printf("Connect to Moonraker: ws://%s:%u/websocket\n", host, port);
    webSocket.begin(host, port, "/websocket");
    webSocket.setExtraHeaders("");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    webSocket.enableHeartbeat(15000, 3000, 2);
}

void moonraker_loop() {
    webSocket.loop();
}
