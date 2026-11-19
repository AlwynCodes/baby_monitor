
Resolve RainMaker component layout (find rmaker_common, check esp_rainmaker requires)

Add missing main/AppDriver.hpp
Rewrite root CMakeLists.txt as a real ESP-IDF project with EXTRA_COMPONENT_DIRS

Fix main/CMakeLists.txt REQUIRES

Retarget sdkconfig.defaults from esp32s3 to esp32; strip S3/ESP-SR leftovers

Fix partitions.csv

Fix AppAudio.hpp defects (ADC config, error checks, allocation)

Fix .vscode/settings.json IDF path; add .gitignore

Build with ESP-IDF 5.3.2 to verify, then commit locally