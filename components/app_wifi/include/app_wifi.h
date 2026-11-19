#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POP_TYPE_RANDOM,
    POP_TYPE_MAC
} app_wifi_pop_type_t;

void app_wifi_init(void);
void app_wifi_start(app_wifi_pop_type_t pop_type);

#ifdef __cplusplus
}
#endif
