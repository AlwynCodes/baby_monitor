idf_component_register(
    # --- requires injected from idf_component.yml ---
    PRIV_REQUIRES jsmn
SRCS "src/json_parser.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "jsmn"
                    )
