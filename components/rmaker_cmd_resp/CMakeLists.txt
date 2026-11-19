set(include_dirs "include")
set(srcs "src/cmd_resp.c")
set(requires esp_event)

idf_component_register(
    # --- requires injected from idf_component.yml ---
SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires})
