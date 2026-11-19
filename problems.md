RUN COMMANDS:
Step 1:
[System.IO.Ports.SerialPort]::GetPortNames()

Step 2:
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:IDF_COMPONENT_MANAGER = "0"
. C:\Espressif\frameworks\esp-idf-v5.3.5\export.ps1
cd C:\baby_monitor

Step 3:

idf.py -p COM3 flash monitor