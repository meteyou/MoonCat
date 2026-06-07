# MoonCat

> A physical print-status indicator for **Klipper / Moonraker**, built into the
> *Illumination Cat*, a 3D-printed cat with a glowing head.

MoonCat runs on a **Wemos D1 mini (ESP8266)**, lights up a **WS2812B NeoPixel**
inside the cat's head and shows the current print status on a small **OLED**.
It talks to your printer through **Moonraker** over a persistent WebSocket and
reflects the live print state with colors, animations and on-screen text.

---

## Features

- **Live print status** straight from Moonraker over a WebSocket (push, not polling)
- **Status-driven NeoPixel** colors & animations in the cat's head
- **Print progress** shown on the OLED
- **Web config** — change the Moonraker host/port later from any browser
- **Robust reconnect** handling when Wi-Fi, Moonraker or Klipper drop
- **Klipper-aware** — detects shutdown / not-ready states, not just the print job

## Status → light mapping

| Printer state | NeoPixel |
|---------------|----------|
| `printing`    | white (solid) |
| `paused`      | yellow (pulsing) |
| `complete`    | green (solid) |
| `error` / `cancelled` | red (pulsing) |
| `standby`     | purple (pulsing) |
| offline / Klipper not ready | off |

> Colors and animations live in a single `renderLed()` function. So easy to tweak
> to your own taste.

## Hardware

| Part    | Notes |
|---------|-------|
| MCU     | Wemos D1 mini (ESP8266) |
| LED     | WS2812B NeoPixel (single pixel in the cat's head) |
| Display | 0.96″ OLED, **SH1106** controller, I²C |
| Model   | [Illumination Cat](https://www.printables.com/model/96457-illumination-cat) |

## Wiring / Pinout

| Component | Signal | Wemos pin | GPIO | Note |
|-----------|--------|-----------|------|------|
| WS2812B   | DIN    | `RX`      | GPIO3 | **DMA pin — fixed**, can't be moved |
| WS2812B   | VCC    | `5V`      | — | — |
| WS2812B   | GND    | `G`       | — | — |
| OLED      | SDA    | `D2`      | GPIO4 | I²C data |
| OLED      | SCL    | `D1`      | GPIO5 | I²C clock |
| OLED      | VCC    | `3V3`     | — | see note |
| OLED      | GND    | `G`       | — | — |

Notes:
- The NeoPixel data line **must** sit on `RX` (GPIO3): NeoPixelBus uses the
  ESP8266 DMA engine there, which keeps the LED flicker-free while Wi-Fi runs.
- The OLED uses the default ESP8266 I²C pins (`D2`/`D1`).
- Power the **OLED from `3V3`, not `5V`**. The module's I²C pull-ups tie SDA/SCL
  to VCC, and the ESP8266 GPIOs are not 5 V tolerant — running the OLED at 3.3 V
  keeps the I²C levels safe for the MCU.
- If your OLED reports a different I²C address or controller, adjust the U8g2
  constructor in `src/main.cpp`.

## Build & flash

MoonCat is built with [PlatformIO](https://platformio.org/).

```bash
# clone, then from the project folder:
pio run                       # build
pio run -t upload             # flash over USB
pio device monitor            # serial console @ 115200
```

**Wireless updates:** once MoonCat is on your network, open its IP in a browser,
hit **Update**, and upload the freshly built binary from
`.pio/build/d1_mini/firmware.bin`. No USB cable required.

## First-time setup

1. On first boot (or when no known Wi-Fi is found) MoonCat opens an access point
   called **`MoonCat-Setup`**.
2. Connect to it with your phone — a captive portal pops up.
3. Pick your Wi-Fi network, enter the password, and fill in the
   **Moonraker host/IP** and **port** (default `7125`).
4. Save. MoonCat reboots, connects, and starts showing your printer status.

### Changing settings later

Open MoonCat's IP address in a browser. From the WiFiManager portal you can
update the Moonraker host/port (the device reboots to apply changes).

### Re-running setup / wiping config

Press the **`RST`** button **twice quickly** (double-reset). MoonCat erases its
stored Wi-Fi + Moonraker config and reopens the `MoonCat-Setup` portal.

## How it works

- Connects to `ws://<host>:<port>/websocket` (Moonraker JSON-RPC 2.0).
- Subscribes to `print_stats`, `display_status` and `virtual_sdcard`.
- Processes the initial snapshot and incremental `notify_status_update` deltas,
  keeping the last known value for fields that aren't part of an update.
- Queries `server.info` on connect and watches `notify_klippy_*` events to
  reflect Klipper's health (shutdown / ready), re-subscribing after a restart.
- Sends **no `Origin` header**, so Moonraker accepts the connection as a trusted
  local client without any `cors_domains` configuration on the printer side.

## Tech stack

- **C++ / Arduino** framework via **[PlatformIO](https://platformio.org/)**
- [WiFiManager](https://github.com/tzapu/WiFiManager) — captive-portal Wi-Fi & config + web update
- [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) — WS2812B driver (DMA)
- [U8g2](https://github.com/olikraus/u8g2) — SH1106 OLED driver
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) — Websocket Lib
- [ArduinoJson](https://arduinojson.org/) — Parse the Moonraker responses
- [ESP_DoubleResetDetector](https://github.com/khoih-prog/ESP_DoubleResetDetector) — double-press reset trigger
- Config stored in **LittleFS**

## 🙏 Credits & inspiration

- [Cali Cat](https://www.thingiverse.com/thing:1545913) by Dezign
- [Illumination Cat](https://www.printables.com/model/96457-illumination-cat) by Agent Cain
- [Printer Monitor](https://github.com/Qrome/printer-monitor) by Qrome
- [simple-octo-ws2812](https://github.com/FrYakaTKoP/simple-octo-ws2812) by Fry

## License

[MIT](LICENSE)
