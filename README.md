# MR19-Code

Embedded software, tooling, and hardware design for **MR19**, the Formula SAE car
built by **Minutemen Racing** (UMass Amherst). This repository holds the on-car
data-acquisition and driver-display electronics, plus the supporting ground-station
tools used to visualize and log vehicle data.

## System Overview

```
        ┌──────────────┐   WiFi AP (HTTP /data)   ┌─────────────────┐
        │  Ground       │  <────────────────────  │                 │
        │  station /    │                          │   Telemetry     │
        │  web dashboard│                          │   unit          │
        └──────────────┘                          │  (UNO R4 WiFi)  │
                                                   │                 │
   Engine / powertrain sensors  ───────────────>  │  RPM, gear,     │
   (CPS, paddles, AFR, TPS,                        │  speed, TPS,    │
    MAP, battery voltage)                          │  battery, laps  │
                                                   └────────┬────────┘
                                                            │ Serial1
                                                            │ (single-char protocol)
                                                            v
                                                   ┌─────────────────┐
                                                   │  Driver display │
                                                   │  (ESP32-S3 +    │
                                                   │   LVGL, 7" LCD) │
                                                   └─────────────────┘
```

The **telemetry** unit is the central data-acquisition computer. It reads the
engine and powertrain sensors, computes derived values (speed, gear, lap time),
and drives the driver-facing **dashboard** display over a serial link.

## Repository Layout

| Directory | Description |
|-----------|-------------|
| `telemetry/` | Main data-acquisition firmware. PlatformIO project targeting the **Arduino UNO R4 WiFi**. Reads sensors, hosts the WiFi telemetry server, and drives the dashboard. |
| `dashboard/` | Driver-display firmware. Arduino sketch for a **Waveshare ESP32-S3-Touch-LCD-7** (800×480 RGB LCD) using **LVGL v8**. Renders gear, RPM, speed, throttle, voltage, and warning lights. UI generated with SquareLine Studio. |
| `dashboard_pcb/` | KiCad PCB design (`mmr_pcb`) for the dashboard hardware, including Teensy footprint/symbol libraries and design backups. |
| `web_site/` | Browser-based ground-station dashboard. Polls the telemetry unit's `/data` endpoint for live gauges, a serial console, CSV data export, and a Leaflet GPS map. |
| `libraries/` | Vendored Arduino libraries used by the dashboard build (`ESP32_Display_Panel`, `esp-lib-utils`, `ESP32_IO_Expander`, `lvgl`). |

## Components

### Telemetry unit (`telemetry/`)

Runs on an Arduino UNO R4 WiFi and:

- Measures **engine RPM** via an interrupt-driven CPS pulse counter.
- Tracks **gear position** from up/down shift paddles and a neutral sensor, with
  a shift-timeout state machine.
- Computes **road speed** from RPM and the selected gear ratio.
- Reads **AFR**, **TPS**, **MAP**, and **battery voltage** (with a low-voltage warning).
- Runs a 1 Hz **lap timer**.
- Hosts a WiFi access point (`minutemen-racing`) with an HTTP `GET /data` endpoint
  that returns comma-separated sensor values for the ground station.
- Pushes display updates to the dashboard over `Serial1` using a single-character
  command protocol (e.g. `G` gear, `R` RPM, `S` speed, `T` throttle, `V` volts,
  and upper/lowercase letters to toggle warning lights).

Build & upload with [PlatformIO](https://platformio.org/):

```bash
cd telemetry
pio run -t upload      # build and flash the UNO R4 WiFi
pio device monitor     # optional serial monitor
```

### Driver dashboard (`dashboard/`)

An LVGL-based UI for the 7" ESP32-S3 touch display. It listens on serial for the
telemetry unit's command protocol and updates on-screen gauges, bars, and warning
indicators (kill switch, over-temp, stall, low battery). Open `dashboard.ino` in
the Arduino IDE with the vendored `libraries/` on the library path, select the
ESP32-S3 board, and flash.

## License

Released under the [MIT License](LICENSE).

Vendored libraries under `libraries/` and `dashboard_pcb/` retain their own
respective licenses.
