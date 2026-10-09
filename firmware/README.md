# BatteryBoyPocketBeatMini CYD Firmware

This folder contains a starter ESP32 firmware scaffold for the 2.8" CYD display. It mirrors the current app layout and behavior as a native UI, rather than trying to run the existing React/Vite app directly on the ESP32.

## Why this is separate from the web app

The repository currently contains a browser-based React/Vite app. That code cannot run on a CYD directly because the CYD is a microcontroller with a TFT display and touch panel, not a browser environment. To keep the same design and interaction model, the UI must be rebuilt in C++ for the ESP32.

## Project structure

- `src/main.cpp` — main app loop and rendering logic
- `platformio.ini` — PlatformIO setup for ESP32 + TFT_eSPI + LVGL

## Build

```bash
cd firmware
pio run
pio run -t upload
```

## Hardware notes

This is configured for a typical 2.8" CYD board with:

- ESP32 dev board
- ILI9341 display
- resistive or capacitive touch panel
- SPI TFT bus

You may need to adjust the pinout and display rotation for your exact board revision.

## Recommend next step

Use the existing app's `src/App.tsx` and `src/index.css` as the visual blueprint. Then port each section into `drawHomeScreen()`, `drawLibraryScreen()`, and `drawBattleScreen()` in the firmware code so the interface remains visually consistent.

This starter preserves the same screen structure, tabbed navigation, and game-like music UI but is intentionally a native firmware base rather than a literal browser port.
