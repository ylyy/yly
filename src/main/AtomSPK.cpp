#include "AtomSPK.h"

static QueueHandle_t i2sstateQueue    = nullptr;
static QueueHandle_t i2sAudioEndQueue = nullptr;

void speakerPlayTask(void *arg) {
    i2sQueueMsg_t QueueMsg;
    while (1) {
        if (xQueueReceive(i2sstateQueue, &QueueMsg, portMAX_DELAY) == pdTRUE) {
            if (QueueMsg.type == kTypeAudio) {
                audioParameters_t *pam = (audioParameters_t *)QueueMsg.dataptr;
                size_t bytes_written   = 0;
                i2s_write(SPAKER_I2S_NUMBER, pam->pAudioData, pam->length,
                          &bytes_written, portMAX_DELAY);
                // Serial.printf("point :%p\r\n",pam->pAudioData);
                if (pam->freeFlag == true)
                    xQueueSend(i2sAudioEndQueue, &pam->pAudioData,
                               (TickType_t)0);
                // delay(1);
                // delete (uint16_t*)pam->pAudioData;
                delete pam;
            } else if (QueueMsg.type == kTypeBeep) {
                beepParameters_t *pam = (beepParameters_t *)QueueMsg.dataptr;
                size_t bytes_written  = 0;
                size_t count = 0, length = 0;

                double t = (1 / (double)pam->freq) * (double)pam->rate;

                if (pam->time > 1000) {
                    length = pam->rate * (pam->time % 1000) / 1000;
                    count  = pam->time / 1000;
                } else {
                    length = pam->rate * pam->time / 1000;
                    count  = 0;
                }
                int rawLength = (count == 0) ? length : pam->rate;
                rawLength -= (int)((int)(rawLength % (int)t));

                int16_t *raw = (int16_t *)calloc(rawLength, sizeof(int16_t));
                for (int i = 0; i < rawLength; i++) {
                    double val = 0;
                    if (i < 1000) {
                        val = pam->maxval * i / 1000;
                    } else if (i > (rawLength - 1000)) {
                        val = pam->maxval -
                              pam->maxval * (1000 - (rawLength - i)) / 1000;
                    } else {
                        val = pam->maxval;
                    }
                    if (pam->freq == 0) {
                        raw[i] = 0;
                    } else {
                    }
                }
                if (rawLength != 0) {
                    i2s_write(SPAKER_I2S_NUMBER, raw, (rawLength)*2,
                              &bytes_written, portMAX_DELAY);
                    Serial.printf("I2S Write\r\n");
                }
                delete pam;
                delete raw;
            }
        }
        delay(1);
    }
}

bool ATOMSPK::begin(int __rate) {
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPAKER_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode        = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = __rate,
        .bits_per_sample =
            I2S_BITS_PER_SAMPLE_16BIT,  // is fixed at 12bit, stereo, MSB
        .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 2,
        .dma_buf_len          = 128,
    };
    i2s_config.mode               = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.use_apll           = false;
    i2s_config.tx_desc_auto_clear = true;

    err += i2s_driver_install(SPAKER_I2S_NUMBER, &i2s_config, 0, NULL);
    i2s_pin_config_t tx_pin_config;

    tx_pin_config.bck_io_num   = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num    = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num  = CONFIG_I2S_DATA_IN_PIN;

    err += i2s_set_pin(SPAKER_I2S_NUMBER, &tx_pin_config);

    err += i2s_set_clk(SPAKER_I2S_NUMBER, __rate, I2S_BITS_PER_SAMPLE_16BIT,
                       I2S_CHANNEL_MONO);

    _rate            = __rate;
    i2sAudioEndQueue = xQueueCreate(1024, sizeof(uint16_t *));
    i2sstateQueue    = xQueueCreate(1024, sizeof(i2sQueueMsg_t));
    if (i2sstateQueue == 0) {
        return false;
    }
    xTaskCreatePinnedToCore(speakerPlayTask, "speakerPlayTask", 4096, nullptr,
                            10, nullptr, 0);
    return (err == ESP_OK) ? true : false;
}

size_t ATOMSPK::playRAW(const uint8_t *__audioPtr, size_t __size, bool modal,
                        bool freeFlag, TickType_t __ticksToWait) {
    size_t writeSize = 0;
    if (modal == false) {
        audioParameters_t *pam =
            (audioParameters_t *)malloc(sizeof(audioParameters_t));
        pam->pAudioData   = (void *)__audioPtr;
        pam->length       = __size;
        pam->freeFlag     = freeFlag;
        i2sQueueMsg_t msg = {.type = kTypeAudio, .dataptr = pam};
        xQueueSend(i2sstateQueue, &msg, (TickType_t)0);
        // xTaskCreatePinnedToCore(speakerAudioTask, "speakerAudioTask", 4096,
        // pam, 3, NULL, 0);
    } else {
        i2s_write(SPAKER_I2S_NUMBER, __audioPtr, __size, &writeSize,
                  __ticksToWait);
    }
    return writeSize;
}
