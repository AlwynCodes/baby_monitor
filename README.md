# ESP32 Baby Monitor (Edge Impulse & ESP RainMaker)

An edge-AI audio monitoring application built on ESP-IDF and FreeRTOS for the **ESP32 Dev Module** paired with a **MAX4466** electret microphone amplifier. The system samples single-channel audio at 16 kHz via ADC, classifies baby-crying events on-device using a quantized Edge Impulse C++ inference library, reports state changes and push alerts to the ESP RainMaker cloud platform, and monitors runtime heap usage.

## Features
- **On-Device Audio Classification:** Detects baby cries (`crying` vs. `noise`) locally from analog microphone samples without continuous cloud audio streaming.
- **ESP RainMaker Integration:** Registers a `Baby node` (`esp.node.sensor`) containing a `Cry sensor` (`esp.device.sensor`) with a read-only `Baby crying` toggle parameter (`esp.param.toggle`).
- **Push Notifications & Voice Integration:** Triggers mobile alerts (`esp.alert.str: "crying"`) on cry detection and supports Amazon Alexa routines through ESP RainMaker.
- **Runtime Heap Telemetry:** Periodically logs free and total internal DRAM heap every 5 seconds via `AppMem`.

## Hardware & Wiring
- **Microcontroller:** ESP32 Dev Module (WROOM-32, 4 MB Flash, internal SRAM)
- **Audio Sensor:** MAX4466 Electret Microphone Amplifier (adjustable gain)

| MAX4466 Pin | ESP32 Dev Module Pin | Description |
| :--- | :--- | :--- |
| **VCC** | `3V3` | 3.3 V power supply (cleaner analog reference than 5 V) |
| **GND** | `GND` | Common ground |
| **OUT** | `GPIO36` (`VP` / `ADC1_CH0`) | Analog audio signal input (ADC1 required when Wi-Fi is active) |

> **Hardware Note:** `ADC1` (`GPIO32`–`GPIO39`) must be used for the MAX4466 output because `ADC2` pins cannot be read concurrently while the ESP32 Wi-Fi driver is active for ESP RainMaker.

## Software Requirements
- **Framework:** ESP-IDF (v4.4.6+)
- **Cloud SDK:** ESP RainMaker (`esp-rainmaker`, `esp-insights`)
- **ML Inference Engine:** Edge Impulse C++ SDK (Cloned from `mandymadongyi/Baby-cry-detector`, Project ID `279239`, deployed with EON™ Compiler and `int8` quantization)
- **Companion Apps:** ESP RainMaker mobile app (Android/iOS) and Amazon Alexa app (optional).

## Project Structure
```text
baby_monitor/
├── CMakeLists.txt                  # Root CMake referencing ESP-RainMaker paths
├── partitions.csv                  # Custom 2 MB factory app partition table
├── sdkconfig                       # Target configuration for ESP32 (xtensa-esp32)
├── edge-impulse-sdk/               # Edge Impulse inference engine (Espressif ESP32 port)
├── model-parameters/               # Model labels (crying, noise) and metadata
├── tflite-model/                   # Compiled quantized TFLite model
└── main/
    ├── CMakeLists.txt              # Component registration and Edge Impulse build rules
    ├── main.cpp                    # Application entry point (app_main)
    ├── AppAudio.hpp                # 16 kHz ADC audio capture, circular buffer, and ML inference
    ├── AppRmaker.hpp               # ESP RainMaker node, device, parameter, and alert management
    └── AppMem.hpp                  # Periodic internal DRAM heap monitoring utility
```

## Setup and Build Instructions

### 1. Generate and Integrate the Edge Impulse Model
1. Clone `mandymadongyi/Baby-cry-detector` on [Edge Impulse](https://edgeimpulse.com/projects/).
2. In **Project info**, set the target device to **Espressif ESP-EYE (ESP32 240MHz)**.
3. Under **Deployment**, select **C++ library**, enable **EON™ Compiler** and **Quantized (int8)**, then click **Build** and download the archive.
4. Extract the archive into the project root without overwriting the root `CMakeLists.txt`.

### 2. Configure Target & Paths
1. Update `RMAKER_PATH` in the root `CMakeLists.txt` to match your local ESP-RainMaker directory.
2. Set the IDF target to the standard ESP32 and verify the 2 MB factory partition table in `partitions.csv`:
   ```bash
   source ~/esp/esp-idf/export.sh
   idf.py set-target esp32
   ```

### 3. Build, Flash, and Provision
```bash
idf.py erase-flash flash monitor
```
1. Open the **ESP RainMaker** mobile app and ensure no previous instance of the devkit is bound.
2. Tap **Add Device** and scan the QR code printed on the serial console (or use the fallback provisioning URL with the displayed `PROV_xxxxxx` name and `POP` code).
3. Once provisioned as **Cry sensor**, play a baby-crying audio sample near the MAX4466 microphone to verify state updates and mobile notifications.

## Verification & Diagnostics
- **Serial Telemetry:** Verify `esp_rmaker_param` logs (`{"Cry sensor": {"Baby crying":true}}` and `{"esp.alert.str":"crying"}`) alongside 5-second `app-mem` internal heap reports.
- **Static Footprint:** Run `idf.py size` to inspect static D/IRAM and total flash image usage within the 2 MB `factory` partition.
- **Troubleshooting:** If Wi-Fi provisioning fails or ADC readings saturate, check that `OUT` is wired to an `ADC1` pin (`GPIO36`), adjust the potentiometer on the back of the MAX4466 breakout board, and perform a clean rebuild with `idf.py erase-flash fullclean flash monitor`.