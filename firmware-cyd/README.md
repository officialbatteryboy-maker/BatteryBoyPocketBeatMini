# PocketBeat Mini — CYD 2.8" firmware port

This folder is a **native ESP32 Arduino/PlatformIO port** of the browser UI prototype. It targets the common ESP32-2432S028 CYD with a 2.8-inch ILI9341 display.

## Current firmware scope
- Native 320×240 landscape UI with the purple/pink retro console styling inspired by the web prototype.
- Touch input via XPT2046, with touch handling kept separate from SD scanning.
- One bounded SD scan at boot; missing/unreadable SD cards do not block the UI or trigger a restart loop.
- MP3 file list from the SD card root or `/MUSIC`; tap a track to select it.
- Reads ID3v1 title/artist/album tags where present and uses the filename when tags are absent.
- Home, Library, Collection/level concept screen, and Settings/touch diagnostics.
- Surprise Spin chooses a random indexed track.

## Audio / Bluetooth setup\n\nThis build includes MP3 decoding and an A2DP Bluetooth audio path. Before building, edit `src/main.cpp` and set `BT_SPEAKER_NAME` to the exact advertised Bluetooth name of the speaker. The Settings screen has a connect button and stop button. This implementation attempts connection by speaker name; it does not provide a general nearby-device picker, and it requires a classic Bluetooth-capable ESP32 (not ESP32-S3/C3). Test on hardware: A2DP library APIs and CYD board variants can differ.\n\n## Important limitation
The uploaded React app is a **browser prototype** and cannot be flashed directly to an ESP32. This port implements an initial native display/touch/SD firmware, MP3 decoding through ESP8266Audio, and a Bluetooth A2DP source path. Embedded album-art decoding and full persistent leveling are not implemented yet. Bluetooth connection is configured by the speaker name in source code rather than a full visual device scanner/picker.

## Board/pin assumptions
For the common ESP32-2432S028R-style CYD:
- TFT: ILI9341, MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, reset tied to board reset.
- Touch: XPT2046 CS 33, IRQ 36; shared display SPI bus.
- microSD: separate VSPI bus, SCK 18, MISO 19, MOSI 23, CS 5.

CYD variants exist. If your board has different wiring/controller, adjust `platformio.ini` and the SD/touch pin constants in `src/main.cpp`.

## SD card
Use FAT32. Put MP3 files in the card root or in `/MUSIC`. Scan is intentionally limited to those locations (no recursive full-card scan), and it runs only once at boot. The index is capped at 100 tracks to keep RAM use predictable.

## Build
Install PlatformIO Core, then from this folder:
```sh
pio run
```

GitHub Actions builds the firmware and publishes an artifact containing a merged factory image plus the component binaries. On GitHub, open **Actions → Build CYD 2.8 Firmware → latest successful run → Artifacts**.

## Flash
The Actions artifact includes `PocketBeatMini-CYD-2.8.factory.bin`, a merged image intended for a 4 MB classic ESP32. In ESP flashing tools select ESP32 and write the factory image at **0x0**. Alternatively, flash component files at:
- bootloader.bin → `0x1000`
- partitions.bin → `0x8000`
- firmware.bin → `0x10000`

Use a data/USB cable, choose the correct COM port, and do not flash while the board is powered from another source. If using esptool manually, erase flash first only if you are comfortable losing existing firmware/settings.
