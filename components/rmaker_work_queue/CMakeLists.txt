set(include_dirs "include")
set(srcs "src/work_queue.c")

idf_component_register(
    # --- requires injected from idf_component.yml ---
SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs})
