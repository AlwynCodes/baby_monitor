# Step 1: Check available COM ports
[System.IO.Ports.SerialPort]::GetPortNames()

# Step 2: Configure ESP-IDF and navigate to the project directory
$env:IDF_TOOLS_PATH = "C:\Espressif"; $env:IDF_COMPONENT_MANAGER = "0"; . C:\Espressif\frameworks\esp-idf-v5.3.5\export.ps1; cd C:\baby_monitor

# Step 3: Flash the firmware and open the serial monitor
idf.py -p COM3 flash monitor