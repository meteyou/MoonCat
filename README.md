# 🐱 MoonCat

> A physical print-status indicator for **Klipper / Moonraker**, built into the
> *Illumination Cat*, a 3D-printed cat with a glowing head.

MoonCat runs on a **Wemos D1 mini (ESP8266)**, lights up a **WS2812B NeoPixel**
inside the cat's head and shows the current print status on a **128×64 LCD**.
It connects to your printer through **Moonraker** and reflects the live print
state with colors, animations and on-screen text.

> ⚠️ **Status: early development.** This project is just getting started — there
> is no working firmware yet. Follow along as it grows.

---

## ✨ Planned features

- Live print status from Moonraker (`standby`, `printing`, `paused`, `complete`,
  `cancelled`, `error`)
- Status-driven NeoPixel colors & animations in the cat's head
- Print progress + status text on the 128×64 LCD
- Easy first-time setup via a Wi-Fi captive portal (hotspot)
- Robust reconnect handling when the printer or Wi-Fi drops

## 🧰 Hardware

| Part | Notes |
|------|-------|
| MCU | Wemos D1 mini (ESP8266) |
| LED | WS2812B NeoPixel |
| Display | 128×64 LCD |
| Model | [Illumination Cat](https://www.printables.com/model/96457-illumination-cat) |

## 🛠️ Tech stack

- **C++ / Arduino** framework via **[PlatformIO](https://platformio.org/)**
- [WiFiManager](https://github.com/tzapu/WiFiManager) — captive-portal Wi-Fi & config
- [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) — WS2812B driver
- [U8g2](https://github.com/olikraus/u8g2) — LCD driver
- [WebSockets](https://github.com/Links2004/arduinoWebSockets) + [ArduinoJson](https://arduinojson.org/) — Moonraker integration

## 🗺️ Roadmap

- [ ] Toolchain & blink test
- [ ] NeoPixel status colors
- [ ] LCD output
- [ ] Wi-Fi captive portal + config storage
- [ ] Moonraker status (HTTP, then WebSocket)
- [ ] Integration & polish (OTA, stability)
- [ ] Wiring guide & docs

## 🙏 Credits & inspiration

- [Cali Cat](https://www.thingiverse.com/thing:1545913) by Dezign
- [Illumination Cat](https://www.printables.com/model/96457-illumination-cat) by Agent Cain
- [simple-octo-ws2812](https://github.com/FrYakaTKoP/simple-octo-ws2812) by Fry

## 📄 License

[MIT](LICENSE.md)
