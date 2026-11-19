# RUN COMMANDS

# Step 1: Check the available serial/COM ports connected to the computer.
[System.IO.Ports.SerialPort]::GetPortNames()


# Step 2: Configure the ESP-IDF environment and navigate to the project folder.

# Set the ESP-IDF tools installation directory.
$env:IDF_TOOLS_PATH = "C:\Espressif"

# Disable the ESP-IDF Component Manager.
$env:IDF_COMPONENT_MANAGER = "0"

# Load the ESP-IDF v5.3.5 environment and required tools.
. C:\Espressif\frameworks\esp-idf-v5.3.5\export.ps1

# Navigate to the Baby Monitor project directory.
cd C:\baby_monitor


# Step 3: Flash the firmware and open the serial monitor.

# Upload the compiled firmware to the ESP32 using COM3, then display serial output.
idf.py -p COM3 flash monitor