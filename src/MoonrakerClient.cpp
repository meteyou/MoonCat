#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <WebSocketsClient.h>

#include "Display.h"
#include "MoonrakerClient.h"
#include "PrinterState.h"

static WebSocketsClient webSocket;

static void subscribe() {
    webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"printer.objects.subscribe","params":{"objects":{"print_stats":null,"display_status":null,"virtual_sdcard":null}},"id":1})");
}

static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            Serial.println(F("WS connected"));
            webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"server.info","id":2})");
            webSocket.sendTXT(R"({"jsonrpc":"2.0","method":"server.database.get_item","params":{"namespace":"mainsail","key":"general.printername"},"id":3})");
            subscribe();
            display_setView(DisplayView::Status);
            break;
        }

        case WStype_DISCONNECTED: {
            Serial.println(F("WS disconnected"));
            printerState.klippyReady = false;
            printerState.status = PrinterStatus::Offline;
            display_setView(DisplayView::Offline);
            break;
        }

        case WStype_TEXT: {
            char* msg = (char*)payload;

            // ignore proc stat
            if (strstr(msg, "notify_proc_stat_update")) return;

            if (strstr(msg, "notify_klippy_shutdown") || strstr(msg, "notify_klippy_disconnected")) {
                printerState.status = PrinterStatus::Error;
                display_makeDirty();
                printerState.klippyReady = false;
                return;
            }

            if (strstr(msg, "notify_klippy_ready")) {
                subscribe();
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
                
                display_makeDirty();
            }

            // update progress if it exists
            if (!status["display_status"]["progress"].isNull()) {
                printerState.progress = status["display_status"]["progress"];
                display_makeDirty();
            }

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
