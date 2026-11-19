idf_component_register(
    # --- requires injected from idf_component.yml ---
SRCS "src/esp_daylight.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_common")
