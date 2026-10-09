# PresenceOS

PresenceOS is an ESP-IDF prototype for an office presence display built for the **Waveshare ESP32-S3-Touch-LCD-5B**. The board's 5-inch, 1024x600 RGB display is driven with LVGL 9, and its GT911 touch controller provides the status controls.

## Current prototype

The firmware currently contains a display interface with four example employees, five presence statuses, and an in-office count. Status selections update the interface in RAM while the firmware is running.

This is an early hardware prototype, not the complete product described in the original project goal. It does not yet include Wi-Fi setup, a web dashboard, REST endpoints, configurable employee records, or NVS persistence. The RGB display output is also still under investigation: firmware builds and flashes, but the physical screen has shown intermittent output and may remain black.

## Hardware

- Waveshare ESP32-S3-Touch-LCD-5B
- 1024x600 RGB LCD
- GT911 capacitive touch controller
- CH422G I/O expander for board controls and backlight
- USB-C cable for power, flashing, and serial monitoring

The board port and RGB pin mapping are in `components/waveshare_board/`.

## Software requirements

- ESP-IDF 6.1.0 (the version currently used to build this project)
- ESP-IDF Component Manager, included with the ESP-IDF installation
- USB serial drivers and a data-capable USB cable

The component manifest allows ESP-IDF 5.5 or later, but only ESP-IDF 6.1.0 has been used to verify the current build. Component versions are recorded in `dependencies.lock`.

## Build

Open an ESP-IDF PowerShell terminal, change to the project directory, then run:

```powershell
cd C:\Users\<user>\Desktop\PresenceOS
idf.py build
```

Alternatively, open this folder in VS Code with the Espressif ESP-IDF extension and run its **Build** command. The project targets `esp32s3`.

The application image is created at `build/PresenceOS.bin`.

## Flash and monitor

Find the board's COM port in Device Manager, then replace `COM4` below if needed:

```powershell
idf.py -p COM4 flash monitor
```

In VS Code, the equivalent actions are **Flash** and **Monitor** in the ESP-IDF extension. The monitor runs at 115200 baud. Close it before disconnecting the USB cable or starting another command that needs the same serial port.

If the board is not detected, enter download mode: hold **BOOT**, connect USB, release **BOOT**, then retry flashing. Press **RESET** after a successful flash if the board does not restart automatically.

## Troubleshooting the black screen

The firmware has compiled and has been written to the ESP32-S3, but that alone does not verify that the LCD is displaying correctly. Keep the serial monitor open at 115200 baud and inspect the boot output for errors mentioning `presence_board`, `GT911`, `RGB panel`, or `Presence display started`.

The current board port uses 21 MHz RGB pixel clock and the 1024x600 timing values from Waveshare's newer LVGL 9 demo. If the display is still black or unstable, capture the complete boot log before changing panel timings or touch configuration.

## Project layout

- `main/`: LVGL presence screen and status controls
- `components/waveshare_board/`: RGB LCD, CH422G, backlight, and GT911 initialization
- `sdkconfig.defaults`: ESP32-S3, PSRAM, and LVGL defaults
- `dependencies.lock`: resolved ESP-IDF component versions

## Roadmap

The intended product includes a configurable employee list, local persistence, Wi-Fi provisioning, and an authenticated web administration dashboard hosted on the ESP32. Those features are not implemented yet.
