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

static DisplayView currentView = DisplayView::Status;
static char wifiApSsid[40];

static void drawStatusView() {
    char line[24];
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 12, printerState.printerName);
    snprintf(line, sizeof(line), "Progress: %d%%", (int)(printerState.progress * 100));
    u8g2.drawStr(2, 40, line);
    u8g2.sendBuffer();
}

// draw a string horizontally centered at baseline y
static void drawCentered(const char* s, int y) {
    int w = u8g2.getStrWidth(s);
    u8g2.drawStr((128 - w) / 2, y, s);
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

void display_setWifiAP(const char* ssid) {
    strlcpy(wifiApSsid, ssid, sizeof(wifiApSsid));
}

void display_begin() {
    u8g2.begin();
    u8g2.setBusClock(100000);
}

void display_setView(DisplayView view) {
    currentView = view;
}

void display_render() {
    switch(currentView) {
        case DisplayView::Status:
            drawStatusView();
            break;
        case DisplayView::WifiGuide:
            drawWifiGuideView();
            break;
    }
}
