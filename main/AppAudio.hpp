#pragma once

#include <cstring>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "sdkconfig.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "driver/i2s.h"
#include "driver/adc.h"
#pragma GCC diagnostic pop

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

namespace app
{
    class AppAudio
    {
    private:
        constexpr static const char *TAG{"audio"};

        static constexpr int CRYING_IDX{0};
        static constexpr int NOISE_IDX{1};

        // The Edge Impulse model expects 16 kHz.
        static constexpr int MODEL_SAMPLE_RATE{16000};

        // Classic ESP32 I2S-ADC mode cannot produce 16 kHz directly. In
        // i2s_calculate_adc_dac_clock() (i2s_legacy.c):
        //     bclk = sample_rate * I2S_LL_AD_BCK_FACTOR   (factor = 2)
        //     mclk = bclk * slot_bits                     (16-bit slot)
        //     mclk_div = sclk / mclk,  sclk = 160 MHz
        // and mclk_div must be < 256, so
        //     160e6 / (sr * 2 * 16) < 256  =>  sr > 19,531 Hz
        // 16 kHz therefore fails with "sample rate is too small, the mclk
        // division exceed the maximum value 255". We capture at 2x and
        // decimate by averaging pairs, which also gives a mild anti-alias
        // filter.
        static constexpr int OVERSAMPLE{2};
        static constexpr int I2S_SAMPLE_RATE{MODEL_SAMPLE_RATE * OVERSAMPLE};

        // 1 second of 16 kHz audio.
        static constexpr int AUDIO_BUFFER_SIZE{MODEL_SAMPLE_RATE};

        static constexpr i2s_port_t I2S_PORT{I2S_NUM_0};
        // Classic ESP32 only: ADC1 channel 6 is GPIO34.
        static constexpr adc1_channel_t MIC_ADC_CHANNEL{ADC1_CHANNEL_6};
        static constexpr float MIC_GAIN{16.0f}; // 12-bit ADC -> 16-bit range
        static constexpr int CAPTURE_STACK_BYTES{4 * 1024};
        static constexpr int ACTION_STACK_BYTES{8 * 1024};

        int16_t *m_ring{nullptr};
        int16_t *m_snapshot{nullptr};
        volatile int m_wptr{0};
        bool m_ready{false};

        std::function<void(bool)> m_crying_fn;
        bool m_crying{false};

        // 32 KB per buffer. Prefer PSRAM when the board has it, otherwise fall
        // back to internal RAM.
        static int16_t *alloc_samples()
        {
            const size_t bytes = AUDIO_BUFFER_SIZE * sizeof(int16_t);
            int16_t *p{nullptr};

            if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0)
            {
                p = static_cast<int16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            }
            if (p == nullptr)
            {
                p = static_cast<int16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            }
            if (p != nullptr)
            {
                memset(p, 0, bytes);
            }
            return p;
        }

        static void captureTask(void *arg)
        {
            AppAudio *o{static_cast<AppAudio *>(arg)};
            uint16_t raw[256];
            float dc{2048.0f};
            float acc{0.0f};
            int phase{0};

            while (true)
            {
                size_t bytes{0};
                esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes, portMAX_DELAY);
                if (err != ESP_OK || bytes == 0)
                {
                    ESP_LOGW(TAG, "i2s_read failed: %s", esp_err_to_name(err));
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }

                const int count = static_cast<int>(bytes / sizeof(uint16_t));
                for (int i = 0; i < count; ++i)
                {
                    acc += static_cast<float>(raw[i] & 0x0FFF);
                    if (++phase < OVERSAMPLE)
                    {
                        continue;
                    }
                    // Decimate: average OVERSAMPLE raw samples into one 16 kHz
                    // sample.
                    float s = acc / static_cast<float>(OVERSAMPLE);
                    acc = 0.0f;
                    phase = 0;

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
                // NOTE: this copy is lock-free and deliberately unsynchronised.
                // The capture task (core 0) may wrap during the copy, which can
                // only tear the newest few milliseconds of the snapshot. That is
                // harmless for a 1 s audio classification window, and avoids
                // holding a critical section for a 32 KB memcpy.
                const int start = o->m_wptr;
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
                    if (EI_CLASSIFIER_LABEL_COUNT > NOISE_IDX)
                    {
                        ESP_LOGI(TAG, "crying=%.2f noise=%.2f",
                                 result.classification[CRYING_IDX].value,
                                 result.classification[NOISE_IDX].value);
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "classifier run failed");
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

            m_ring = alloc_samples();
            m_snapshot = alloc_samples();
            if (m_ring == nullptr || m_snapshot == nullptr)
            {
                // Without buffers the capture task would fault.
                ESP_LOGE(TAG, "not enough memory for %d-sample audio buffers", AUDIO_BUFFER_SIZE);
                return;
            }

            i2s_config_t cfg = {};
            cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
            cfg.sample_rate = I2S_SAMPLE_RATE;
            cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
            cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
            cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
            cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
            cfg.dma_buf_count = 4;
            cfg.dma_buf_len = 256;
            cfg.use_apll = false;

            ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &cfg, 0, nullptr));
            ESP_ERROR_CHECK(i2s_set_adc_mode(ADC_UNIT_1, MIC_ADC_CHANNEL));
            // 12-bit samples at 11 dB attenuation: the 0 dB default clips the
            // MAX4466 output almost immediately.
            ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
            ESP_ERROR_CHECK(adc1_config_channel_atten(MIC_ADC_CHANNEL, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(i2s_adc_enable(I2S_PORT));

            ESP_LOGI(TAG, "ADC mic ready: I2S %d Hz -> %d Hz model rate (decimate x%d), GPIO34",
                     I2S_SAMPLE_RATE, MODEL_SAMPLE_RATE, OVERSAMPLE);
            m_ready = true;
        }

        void start(void)
        {
            if (!m_ready)
            {
                ESP_LOGE(TAG, "start() called before a successful init()");
                return;
            }
            xTaskCreatePinnedToCore(captureTask, "capture", CAPTURE_STACK_BYTES, this, 6, nullptr, 0);
            xTaskCreatePinnedToCore(actionTask, "action", ACTION_STACK_BYTES, this, 5, nullptr, 1);
        }
    };
}
