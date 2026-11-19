#pragma once

#include <cstring>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/i2s.h"
#include "driver/adc.h"
#pragma GCC diagnostic pop

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

namespace app
{
    class AppAudio
    {
    private:
        static constexpr int CRYING_IDX{0};
        static constexpr int NOISE_IDX{1};
        static constexpr int AUDIO_BUFFER_SIZE{16000};
        static constexpr i2s_port_t I2S_PORT{I2S_NUM_0};
        static constexpr adc1_channel_t MIC_ADC_CHANNEL{ADC1_CHANNEL_6}; // GPIO34
        static constexpr float MIC_GAIN{16.0f}; // 12-bit ADC -> 16-bit range; tune if too quiet/loud

        int16_t *m_ring{nullptr};
        int16_t *m_snapshot{nullptr};
        volatile int m_wptr{0};

        std::function<void(bool)> m_crying_fn;
        bool m_crying{false};

        static void captureTask(void *arg)
        {
            AppAudio *o{static_cast<AppAudio *>(arg)};
            uint16_t raw[256];
            float dc{2048.0f};

            while (true)
            {
                size_t bytes{0};
                i2s_read(I2S_PORT, raw, sizeof(raw), &bytes, portMAX_DELAY);
                int count = bytes / sizeof(uint16_t);
                for (int i = 0; i < count; ++i)
                {
                    float s = static_cast<float>(raw[i] & 0x0FFF);
                    dc += (s - dc) * 0.001f; // remove the MAX4466 DC bias
                    float v = (s - dc) * MIC_GAIN;
                    if (v > 32767.0f) v = 32767.0f;
                    if (v < -32768.0f) v = -32768.0f;
                    o->m_ring[o->m_wptr] = static_cast<int16_t>(v);
                    o->m_wptr = (o->m_wptr + 1) % AUDIO_BUFFER_SIZE;
                }
            }
        }

        static void actionTask(void *arg)
        {
            AppAudio *o{static_cast<AppAudio *>(arg)};
            ei_impulse_result_t result = {nullptr};

            auto get_data_fn = [o](size_t offset, size_t length, float *out_ptr) -> int
            {
                for (size_t i = 0; i < length; ++i)
                {
                    out_ptr[i] = static_cast<float>(o->m_snapshot[offset + i]);
                }
                return 0;
            };

            vTaskDelay(pdMS_TO_TICKS(1500)); // let the ring buffer fill once

            while (true)
            {
                int start = o->m_wptr;
                for (int i = 0; i < AUDIO_BUFFER_SIZE; ++i)
                {
                    o->m_snapshot[i] = o->m_ring[(start + i) % AUDIO_BUFFER_SIZE];
                }

                signal_t features_signal{get_data_fn, AUDIO_BUFFER_SIZE};
                int result_idx{NOISE_IDX};

                if (run_classifier(&features_signal, &result) == EI_IMPULSE_OK)
                {
                    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i)
                    {
                        if (result.classification[i].value > result.classification[result_idx].value)
                        {
                            result_idx = i;
                        }
                    }
                    ESP_LOGI("audio", "crying=%.2f noise=%.2f",
                             result.classification[CRYING_IDX].value,
                             result.classification[NOISE_IDX].value);
                }

                if (result_idx == CRYING_IDX && !o->m_crying)
                {
                    o->m_crying_fn(true);
                    o->m_crying = true;
                }
                else if (result_idx == NOISE_IDX && o->m_crying)
                {
                    o->m_crying_fn(false);
                    o->m_crying = false;
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

    public:
        void init(std::function<void(bool)> f)
        {
            m_crying_fn = f;
            m_crying = false;
            m_ring = new int16_t[AUDIO_BUFFER_SIZE]();
            m_snapshot = new int16_t[AUDIO_BUFFER_SIZE]();

            i2s_config_t cfg = {};
            cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
            cfg.sample_rate = 16000;
            cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
            cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
            cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
            cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
            cfg.dma_buf_count = 4;
            cfg.dma_buf_len = 256;
            cfg.use_apll = false;

            i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
            i2s_set_adc_mode(ADC_UNIT_1, MIC_ADC_CHANNEL);
            i2s_adc_enable(I2S_PORT);
        }

        void start(void)
        {
            xTaskCreatePinnedToCore(captureTask, "capture", 4 * 1024, this, 6, nullptr, 0);
            xTaskCreatePinnedToCore(actionTask, "action", 8 * 1024, this, 5, nullptr, 1);
        }
    };
}
