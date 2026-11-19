set(component_srcs "src/esp_schedule.c"
                   "src/esp_schedule_nvs.c")

idf_component_register(
    # --- requires injected from idf_component.yml ---
    PRIV_REQUIRES esp_daylight
SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       PRIV_REQUIRES nvs_flash esp_netif)
