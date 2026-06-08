#include <U8g2lib.h>
#include "Display.h"
#include "PrinterState.h"

// define the I2C address and create the U8G2 object for the OLED display
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
static DisplayView currentView = DisplayView::Status;

static void drawStatusView() {
    char line[24];
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(2, 12, "MoonCat");
    snprintf(line, sizeof(line), "Progress: %d%%", (int)(printerState.progress * 100));
    u8g2.drawStr(2, 40, line);
    u8g2.sendBuffer();
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
    }
}
