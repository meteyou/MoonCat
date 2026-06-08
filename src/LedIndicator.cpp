#include <NeoPixelBus.h>
#include "LedIndicator.h"
#include "PrinterState.h"

// define the pixel count and strip type for the NeoPixel
static const uint16_t PixelCount = 1;
static NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount);

static RgbColor pulse(RgbColor base, uint16_t periodMs) {
    float phase = (millis() % periodMs) / (float)periodMs;
    float b = (sin(phase * 2 * PI) + 1) / 2;   // 0..1
    return base.Dim(b * 255);
}

// clear neopixel
void led_begin() {
    strip.Begin();
    strip.Show();
}

void led_render() {
    switch (printerState.status) {
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