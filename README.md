# ESP32-S3 Baby Monitor (Edge Impulse & ESP RainMaker)

An ESP32-S3 Box Lite edge-AI audio monitoring application built on ESP-IDF and FreeRTOS. The system captures dual-microphone audio via ESP-SkaiNet's Audio Front-End (AFE), classifies baby-crying events on-device using a quantized Edge Impulse C++ inference library, reports state changes and push alerts to the ESP RainMaker cloud platform, and monitors internal and external (SPIRAM) heap usage at runtime.

## Features
- **On-Device Audio Classification:** Detects baby cries (`crying` vs. `noise`) locally without continuous cloud audio streaming.
- **ESP RainMaker Integration:** Registers a `Baby node` (`esp.node.sensor`) containing a `Cry sensor` (`esp.device.sensor`) with a read-only `Baby crying` toggle parameter (`esp.param.toggle`).
- **Push Notifications & Voice Integration:** Triggers mobile alerts (`esp.alert.str: "crying"`) on cry detection and supports Amazon Alexa routines through ESP RainMaker.
- **SPIRAM Memory Optimization:** Offloads large audio/feature buffers and non-AFE FreeRTOS task stacks to external Octal PSRAM to prevent internal RAM exhaustion.
- **Runtime Heap Telemetry:** Periodically logs free and total internal DRAM and SPIRAM every 5 seconds.

## Hardware & Software Requirements
- **Hardware:** ESP32-S3 Box Lite (utilizing the two integrated onboard microphones; no external sensors required).
- **Frameworks & SDKs:**
  - ESP-IDF (v4.4.6+)
  - ESP-SkaiNet (`esp_codec_dev`, `esp-dsp`, `esp-sr`, `hardware_driver`, `sr_ringbuf`)
  - ESP RainMaker (`esp-rainmaker`, `esp-insights`)
  - Edge Impulse C++ SDK (Cloned from `mandymadongyi/Baby-cry-detector`, Project ID `279239`, deployed with EON™ Compiler and `int8` quantization)
- **Companion Apps:** ESP RainMaker mobile app (Android/iOS) and Amazon Alexa app (optional).

## Project Structure
```text
baby_monitor/
├── CMakeLists.txt                  # Root CMake referencing ESP-SkaiNet and ESP-RainMaker paths
├── partitions.csv                  # Custom 2 MB factory app partition table
├── sdkconfig                       # Preconfigured SPIRAM, task stack, and RainMaker settings
├── edge-impulse-sdk/               # Edge Impulse inference engine (with ESP32-S3 Box Lite port)
├── model-parameters/               # Model labels (crying, noise) and metadata
├── tflite-model/                   # Compiled quantized TFLite model
└── main/
    ├── CMakeLists.txt              # Component registration and Edge Impulse build rules
    ├── main.cpp                    # Application entry point (app_main)
    ├── AppAudio.hpp                # AFE audio capture, circular buffering, and ML inference tasks
    ├── AppRmaker.hpp               # ESP RainMaker node, device, parameter, and alert management
    └── AppMem.hpp                  # Periodic internal/SPIRAM heap monitoring utility
```

## Setup and Build Instructions

### 1. Generate and Integrate the Edge Impulse Model
1. Clone `mandymadongyi/Baby-cry-detector` on [Edge Impulse](https://edgeimpulse.com/projects/).
2. In **Project info**, set the target device to **Espressif ESP-EYE**.
3. Under **Deployment**, select **C++ library**, enable **EON™ Compiler** and **Quantized (int8)**, then click **Build** and download the archive.
4. Extract the archive into the project root without overwriting the root `CMakeLists.txt`.
5. Replace `edge-impulse-sdk/porting/espressif` with the ESP32-S3 Box Lite porting directory from the companion repository (`ch12/baby_monitor/edge-impulse-sdk/porting/espressif`).

### 2. Configure Framework Paths
Update `BOOK_REPO`, `RMAKER_PATH`, and `SKAINET_PATH` in the root `CMakeLists.txt` to match your local ESP-SkaiNet and ESP-RainMaker component directories.

### 3. Build, Flash, and Provision
```bash
source ~/esp/esp-idf/export.sh
idf.py erase-flash flash monitor
```
1. Open the **ESP RainMaker** mobile app and ensure no previous instance of the devkit is bound.
2. Tap **Add Device** and scan the QR code printed on the serial console (or use the fallback provisioning URL with the displayed `PROV_xxxxxx` name and `POP` code).
3. Once provisioned as **Cry sensor**, play a baby-crying audio sample to verify state updates and mobile notifications.

## Verification & Diagnostics
- **Serial Telemetry:** Verify `esp_rmaker_param` logs (`{"Cry sensor": {"Baby crying":true}}` and `{"esp.alert.str":"crying"}`) alongside 5-second `app-mem` heap reports.
- **Static Footprint:** Run `idf.py size` to inspect static D/IRAM (~40.4% used, 139,839 bytes) and total flash image size (1,277,240 bytes).
- **Troubleshooting:** If provisioning or integration issues occur, run `idf.py erase-flash fullclean flash monitor` and inspect `sdkconfig` via `idf.py menuconfig`.