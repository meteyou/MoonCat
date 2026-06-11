#include <U8g2lib.h>
#include "Display.h"
#include "PrinterState.h"

// Select the display controller via build flag:
//   -D DISPLAY_SSD1306  -> SSD1306 controller mostly for 0.96" OLED
//   -D DISPLAY_SH1106   -> SH1106 controller mostly for 1.3" OLED
#if defined(DISPLAY_SSD1306)
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
#else
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
#endif

static DisplayView  currentView         = DisplayView::Offline;
static bool         needsRender         = true;
static char         wifiApSsid[40];
static char         moonrakerHost[40]   = "";
static uint16_t     moonrakerPort       = 0;

// format a temperature as "E 23°" or "E 23/210°" while heating
static void formatTemp(char* buf, size_t len, const char* label, float actual, float target) {
    if (target > 0.0f) {
        snprintf(buf, len, "%s %d/%d\u00b0", label, (int)(actual + 0.5f), (int)(target + 0.5f));
    } else {
        snprintf(buf, len, "%s %d\u00b0", label, (int)(actual + 0.5f));
    }
}

// draw a string horizontally centered at baseline y
static void drawCentered(const char* s, int y) {
    int w = u8g2.getStrWidth(s);
    u8g2.drawStr((128 - w) / 2, y, s);
}

// header: printer name + separator line
static void drawHeader() {
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 12, printerState.printerName);
    u8g2.drawHLine(0, 16, 128);
}

// temperatures at the bottom
static void drawTempFooter() {
    char line[20];
    u8g2.setFont(u8g2_font_6x10_tf);
    formatTemp(line, sizeof(line), "E", printerState.extruderTemp, printerState.extruderTarget);

    if (printerState.hasHeaterBed) {
        // extruder left, bed right
        u8g2.drawUTF8(2, 62, line);
        formatTemp(line, sizeof(line), "B", printerState.bedTemp, printerState.bedTarget);
        u8g2.drawUTF8(126 - u8g2.getUTF8Width(line), 62, line);
    } else {
        // extruder only -> centered
        u8g2.drawUTF8((128 - u8g2.getUTF8Width(line)) / 2, 62, line);
    }
}

static void drawStatusView() {
    char line[24];
    u8g2.clearBuffer();

    drawHeader();

    // state label left, percentage right
    const char* label = "Printing";
    if (printerState.status == PrinterStatus::Paused)     label = "Paused";
    else if (printerState.status == PrinterStatus::Error) label = "Error";
    else if (printerState.printDuration <= 0.0f)          label = "Preparing";

    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 32, label);

    snprintf(line, sizeof(line), "%d%%", (int)(printerState.progress * 100.0f + 0.5f));
    u8g2.drawStr(126 - u8g2.getStrWidth(line), 32, line);

    // progress bar: outer frame + inner fill with 2px padding
    const int barX = 2, barY = 37, barW = 124, barH = 9;
    u8g2.drawFrame(barX, barY, barW, barH);

    int fill = (int)((barW - 4) * printerState.progress + 0.5f);
    if (fill > 0) {
        u8g2.drawBox(barX + 2, barY + 2, fill, barH - 4);
    }

    drawTempFooter();

    u8g2.sendBuffer();
}

static void drawWifiGuideView() {
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_6x10_tr);
    drawCentered("WiFi Manager", 10);
    drawCentered("Connect to AP:", 24);

    // SSID prominent in the middle; fall back to a smaller font if too wide
    u8g2.setFont(u8g2_font_profont15_tr);
    if (u8g2.getStrWidth(wifiApSsid) > 126) {
        u8g2.setFont(u8g2_font_6x10_tr);
    }
    drawCentered(wifiApSsid, 44);

    u8g2.setFont(u8g2_font_6x10_tr);
    drawCentered("to setup WiFi", 62);

    u8g2.sendBuffer();
}

static void drawOfflineView() {
    u8g2.clearBuffer();

    // center points
    const int cx = 64;
    const int cy = 24;

    u8g2.drawDisc(cx, cy, 2);
    u8g2.drawCircle(cx, cy, 7,  U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(cx, cy, 12, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawCircle(cx, cy, 17, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);

    u8g2.drawLine(cx - 19, cy + 5, cx + 17, cy - 19);
    u8g2.drawLine(cx - 18, cy + 5, cx + 18, cy - 19);

    u8g2.setFont(u8g2_font_profont15_tr);
    drawCentered("Printer offline", 46);

    char target[48];
    snprintf(target, sizeof(target), "%s:%u", moonrakerHost, moonrakerPort);

    u8g2.setFont(u8g2_font_6x10_tr);
    if (u8g2.getStrWidth(target) > 126) {
        u8g2.setFont(u8g2_font_4x6_tr);
    }
    drawCentered(target, 60);

    u8g2.sendBuffer();
}

static void drawStandbyView() {
    u8g2.clearBuffer();

    drawHeader();

    // big state label in the middle
    const char* label = (printerState.status == PrinterStatus::Complete) ? "Done" : "Standby";
    u8g2.setFont(u8g2_font_profont22_tr);
    drawCentered(label, 42);

    drawTempFooter();

    u8g2.sendBuffer();
}

void display_setWifiAP(const char* ssid) {
    strlcpy(wifiApSsid, ssid, sizeof(wifiApSsid));
}

void display_setMoonrakerTarget(const char* host, uint16_t port) {
    strlcpy(moonrakerHost, host, sizeof(moonrakerHost));
    moonrakerPort = port;
}

void display_begin() {
    u8g2.begin();
    u8g2.setBusClock(100000);
}

void display_makeDirty() {
    needsRender = true;
}

void display_setView(DisplayView view) {
    if (currentView == view) {
        return;
    }

    currentView = view;
    needsRender = true;
}

void display_computeView() {
    switch(printerState.status) {
        case PrinterStatus::Paused:
        case PrinterStatus::Printing:
            display_setView(DisplayView::Status);
            break;

        case PrinterStatus::Complete:
        case PrinterStatus::Standby:
            display_setView(DisplayView::Standby);
            break;
        
        case PrinterStatus::Offline:
            display_setView(DisplayView::Offline);
            break;
        
        case PrinterStatus::Error:
            display_setView(DisplayView::Status);
            break;
    }
}

void display_render() {
    if (!needsRender) return;
    needsRender = false;

    switch(currentView) {
        case DisplayView::Offline:
            drawOfflineView();
            break;
        case DisplayView::Standby:
            drawStandbyView();
            break;
        case DisplayView::Status:
            drawStatusView();
            break;
        case DisplayView::WifiGuide:
            drawWifiGuideView();
            break;
    }
}
