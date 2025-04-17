#include "xiaozhi_ai.h"
#include <utils/helper.h>
#include "cubicat.h"
#include "system_info.h"
#include <esp_random.h>
#include "esp_chip_info.h"
#include <esp_ota_ops.h>
#include "esp_app_desc.h"
#include <mbedtls/base64.h>

#define FG_TASK_EVENT (1 << 0)
#define AUDIO_TASK_EVENT (1 << 1)
#define STOP_SPEAK_EVENT (1 << 2)
#define LANG_CN "zh-CN"
#define BOARD_TYPE "bread-compact-wifi"
#define BOARD_NAME BOARD_TYPE
#define LOCK_OPUS_BUFFER std::lock_guard<std::recursive_mutex> lock(m_opusMutex);
#define OTA_VERSION_URL "https://api.tenclass.net/xiaozhi/ota/"
std::string session_id = "";

XiaoZhiAI::XiaoZhiAI()
{
    m_eventGroup = xEventGroupCreate();
    m_wakeWordDetect.Initialize(1, false);
#ifdef CONFIG_AUDIO_PROCESSING
    m_audioProcessor.Initialize(2, true);
    m_loopbackBuffer.allocate(1024 * 2);
#endif
}

XiaoZhiAI::~XiaoZhiAI()
{
    m_opusFrameBuffer.release();
#ifdef CONFIG_AUDIO_PROCESSING
    m_loopbackBuffer.release();
#endif
    vEventGroupDelete(m_eventGroup);
}
void AudioTask(void* param) {
    auto ai = (XiaoZhiAI*)param;
    while (true)
    {
        ai->audioLoop();
    }
}
void XiaoZhiAI::onBeginConnect() {
    // Check for new firmware version or get the MQTT broker address

    // m_ota.SetCheckVersionUrl(OTA_VERSION_URL);
    // m_ota.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    // m_ota.SetHeader("Client-Id", getUUID());
    // m_ota.SetHeader("Accept-Language", LANG_CN);
    // auto app_desc = esp_app_get_description();
    // m_ota.SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);

    // xTaskCreate([](void* arg) {
    //     XiaoZhiAI* app = (XiaoZhiAI*)arg;
    //     app->checkNewVersion();
    //     vTaskDelete(NULL);
    // }, "check_new_version", 4096, this, 2, nullptr);
}

void XiaoZhiAI::onConnected(Socket* socket) {
    m_socket = socket;
    if (!m_audioTaskHandle)
        xTaskCreatePinnedToCoreWithCaps(AudioTask, "Audio Task", 1024*32, this, 1, &m_audioTaskHandle, getSubCoreId(), MALLOC_CAP_SPIRAM);
    // Send hello message to describe the client
    // keys: message type, version, audio_params (format, sample_rate, channels)
    std::string message = "{";
    message += "\"type\":\"hello\",";
    message += "\"version\": 1,";
    message += "\"transport\":\"websocket\",";
    message += "\"audio_params\":{";
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    socket->send(message.c_str(), message.length(), false);
}

void XiaoZhiAI::onDisconnected() {
    setState(Idle);
    foregroundTask([this]() {
        if (m_connectionCallback)
            m_connectionCallback(false);
    });
}

void XiaoZhiAI::onBinaryData(const char* data, unsigned int len) {
    // printf("onBinaryData: %d\n", len);
    if (m_eDeviceState == Speaking) {
        LOCK_OPUS_BUFFER
        m_opusBufferQueue.emplace_back(std::move(std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len)));
    }
}

void XiaoZhiAI::onJsonData(cJSON* root) {
    auto type = cJSON_GetObjectItem(root, "type");
    if (!type) {
        LOGE("Missing message type, data: %s", root->valuestring);
        return;
    }
    if (strcmp(type->valuestring, "hello") == 0) {
        onServerHello(root);
    } else {
        // other command
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                setState(Speaking);
            } else if (strcmp(state->valuestring, "stop") == 0) {
                // 等待所有缓存语音数据播放完毕才能切换状态
                xEventGroupSetBits(m_eventGroup, STOP_SPEAK_EVENT);
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto textItem = cJSON_GetObjectItem(root, "text");
                if (textItem != NULL) {
                    std::string text = textItem->valuestring;
                    LOGI("<< %s", text.c_str());
                    foregroundTask([text, this]() {
                        if (m_ttsCallback) {
                            m_ttsCallback(text);
                        } 
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            // printf("stt result: %s\n", cJSON_GetObjectItem(root, "text")->valuestring);
        } else if (strcmp(type->valuestring, "llm") == 0) {
            std::string emo = cJSON_GetObjectItem(root, "emotion")->valuestring;
            printf("llm result: %s\n", emo.c_str());
            foregroundTask([emo, this]() {
                if (m_llmCallback) {
                    Emotion e;
                    if (emo == "neutral") {
                        e = Neutral;
                    } else if (emo == "happy") {
                        e = Happy;
                    } else if (emo == "sad") {
                        e = Sad;
                    } else if (emo == "angry") {
                        e = Angry;
                    } else if (emo == "surprise") {
                        e = Surprise;
                    } else if (emo == "disgust") {
                        e = Disgust;
                    } else if (emo == "fear") {
                        e = Fear;
                    } else {
                        e = Unknown;
                    }
                    m_llmCallback(e);
                }
            });
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                printf("iot commands:%s\n", commands->valuestring);
            }
        }
    }
}

void XiaoZhiAI::onServerHello(const cJSON* root) {
    printf("On server hello!!\n");
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        LOGE("Unsupported transport: %s", transport->valuestring);
        return;
    }
    uint16_t sampleRate = CUBICAT.speaker.getSampleRate();
    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (audio_params != NULL) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (sample_rate != NULL) {
            sampleRate = sample_rate->valueint;
            CUBICAT.speaker.setSampleRate(sampleRate);
#ifdef CONFIG_AUDIO_PROCESSING
            if (sampleRate != 16000) {
                m_speakerResampler.Configure(sampleRate, 16000);
            }
#endif
        }
    }
    if (!m_pOpusDecoder)
        m_pOpusDecoder = std::make_unique<OpusDecoderWrapper>(sampleRate, 1, OPUS_FRAME_DURATION_MS);
    if (!m_pOpusEncoder) {
        m_pOpusEncoder = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
        m_pOpusEncoder->SetComplexity(3);
        CUBICAT.mic.setSampleRate(16000);
        m_opusFrameBuffer.allocate(MAX_OPUS_PACKET_SIZE);
    }
    m_opusFrameSize = 16000 / 1000 * 1 * OPUS_FRAME_DURATION_MS;
    m_wakeWordDetect.OnWakeWordDetected([this](const std::string& wake_word) {
        m_sLastWakeWord = wake_word;
        foregroundTask([this, &wake_word]() {
            if (m_eDeviceState == Speaking) {
                abortSpeaking();
            }
            if (m_eDeviceState == Idle) {
                setState(Connecting);
                m_wakeWordDetect.EncodeWakeWordData();
                // Reopen audio channel if audio channel is closed
                if (!m_socket->isConnected()) {
                    m_bAutoWakeOnReconnect = true;
                    m_socket->reconnect();
                    return;
                }
                onWakeWord();
            }
        });
    });
    m_wakeWordDetect.StartDetection();
    CUBICAT.mic.start();
    CUBICAT.speaker.setEnable(true);
    if (m_bAutoWakeOnReconnect) {
        onWakeWord();
        m_bAutoWakeOnReconnect = false;   
    }
    // Audio processing 
#ifdef CONFIG_AUDIO_PROCESSING
    m_audioProcessor.OnOutput([this](std::vector<int16_t>&& data) {
        audioTask([this, data = std::move(data)]() mutable {
            m_pOpusEncoder->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                sendAudio(opus.data(), opus.size());
            });  
        });
    });
    m_audioProcessor.Stop();
#endif
    foregroundTask([this]() {
        if (m_connectionCallback) {
            m_connectionCallback(true);
        }
    });
}

void XiaoZhiAI::onWakeWord() {
    std::vector<uint8_t> opus;
    // Encode and send the wake word data to the server
    while (m_wakeWordDetect.GetWakeWordOpus(opus)) {
        sendAudio(opus.data(), opus.size());
    }
    // Set the chat state to wake word detected
    sendWakeWord(m_sLastWakeWord);

    setState(Idle);
}

void XiaoZhiAI::loop() {
    auto bits = xEventGroupWaitBits(m_eventGroup, FG_TASK_EVENT | STOP_SPEAK_EVENT, pdFALSE, pdFALSE, 0);
    if (bits & FG_TASK_EVENT) {
        std::lock_guard<std::recursive_mutex> lock(m_taskMutex);
        while (!m_bgTasks.empty())
        {
            auto& func = m_bgTasks.front();
            func();
            m_bgTasks.pop_front();
        }
        xEventGroupClearBits(m_eventGroup, FG_TASK_EVENT);
    }
    if (bits & STOP_SPEAK_EVENT) {
        if (m_eDeviceState == Speaking) {
            if (m_opusBufferQueue.empty()) {
                setState(Listening);
                xEventGroupClearBits(m_eventGroup, STOP_SPEAK_EVENT);
            }
        } else {
            xEventGroupClearBits(m_eventGroup, STOP_SPEAK_EVENT);
        }
    }
}

void XiaoZhiAI::audioLoop() {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    auto bits = xEventGroupWaitBits(m_eventGroup, AUDIO_TASK_EVENT, pdTRUE, pdFALSE, 0);
    if (bits & AUDIO_TASK_EVENT) {
        std::lock_guard<std::recursive_mutex> lock(m_audioTaskMutex);
        while (!m_audioTasks.empty())
        {
            auto& func = m_audioTasks.front();
            func();
            m_audioTasks.pop_front();
        }
    }
    auto buf = CUBICAT.mic.popAudioBuffer(0);
    std::vector<int16_t> micPCM;
    if (buf.size()) {
        int16_t* data = (int16_t*)buf.data();
        micPCM = std::move(std::vector<int16_t>(data, data + buf.size() / 2));
    }
    // Wake word detection data feed
    if (micPCM.size()) {
        if (m_wakeWordDetect.IsDetectionRunning()) {
            m_wakeWordDetect.Feed(micPCM);
        }
    }
    if (getState() == Speaking) {
        if (m_opusBufferQueue.empty()) {
            return;
        }
        std::unique_lock<std::recursive_mutex> lock(m_opusMutex);
        auto opus = std::move(m_opusBufferQueue.front());
        m_opusBufferQueue.pop_front();
        lock.unlock();
        // decode opus
        std::vector<int16_t> playPCM;
        if (!m_pOpusDecoder->Decode(std::move(opus), playPCM)) {
            return;
        }
#ifdef CONFIG_AUDIO_PROCESSING
        // Input audio aec process
        std::vector<int16_t>* resampledRef = &playPCM;
        std::vector<int16_t> resampleBuff;
        if (m_audioProcessor.IsRunning()) {
            // combine pcm with output pcm which is work as reference
            std::vector<int16_t> pcmWithRef;
            // resample speaker pcm data if speaker sample rate is not 16k
            if (CUBICAT.speaker.getSampleRate() != 16000) {
                resampleBuff.resize(m_speakerResampler.GetOutputSamples(playPCM.size()));
                m_speakerResampler.Process(playPCM.data(), playPCM.size(), resampleBuff.data());
                resampledRef = &resampleBuff;
            }
            if (m_loopbackBuffer.len >= micPCM.size() * 2) {
                pcmWithRef.resize(micPCM.size() * 2);
                int16_t* loopback = (int16_t*)m_loopbackBuffer.data + (m_loopbackBuffer.len/2 - micPCM.size());
                for (int i = 0; i < micPCM.size(); i++) {
                    pcmWithRef[i*2] = micPCM[i];
                    pcmWithRef[i*2 + 1] = *(loopback++);
                }
                m_audioProcessor.Feed(pcmWithRef);   
            }
        }
#endif
        CUBICAT.speaker.playRaw(playPCM.data(), playPCM.size(), 1);
#ifdef CONFIG_AUDIO_PROCESSING
        m_loopbackBuffer.append((uint8_t*)resampledRef->data(), resampledRef->size() * sizeof(int16_t));
#endif
    } else if (getState() == Listening) {
        if (micPCM.size()) {
            m_pOpusEncoder->Encode(std::move(micPCM),
             [this](std::vector<uint8_t>&& opus) {
                sendAudio(opus.data(), opus.size());
            });
        }
    }
}


void XiaoZhiAI::foregroundTask(std::function<void()> callback) {
    std::lock_guard<std::recursive_mutex> lock(m_taskMutex);
    m_bgTasks.push_back(callback);
    xEventGroupSetBits(m_eventGroup, FG_TASK_EVENT);
}

void XiaoZhiAI::audioTask(std::function<void()> callback) {
    std::lock_guard<std::recursive_mutex> lock(m_audioTaskMutex);
    m_audioTasks.push_back(callback);
    xEventGroupSetBits(m_eventGroup, AUDIO_TASK_EVENT);
}

void XiaoZhiAI::abortSpeaking() {
    setState(Idle);
    std::string message = "{\"session_id\":\"" + session_id + "\",\"type\":\"abort\"";
    if (true) {
        message += ",\"reason\":\"wake_word_detected\"";
    }
    message += "}";
    m_socket->send(message.c_str(), message.length(), false);
}
void XiaoZhiAI::sendWakeWord(const std::string& wakeWord) {
    std::string json = "{\"session_id\":\"" + session_id + 
    "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + wakeWord + "\"}";
    m_socket->send(json.c_str(), json.length(), false);
}

void XiaoZhiAI::sendAudio(const uint8_t* data, size_t len) {
    assert(data != nullptr && len > 0);
    m_socket->send((const char*)data, len, true);
}

void XiaoZhiAI::sendStartListening(ListeningMode mode) {
    std::string message = "{\"session_id\":\"" + session_id + "\"";
    message += ",\"type\":\"listen\",\"state\":\"start\"";
    if (mode == kListeningModeRealtime) {
        message += ",\"mode\":\"realtime\"";
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";
    } else {
        message += ",\"mode\":\"manual\"";
    }
    message += "}";
    m_socket->send(message.c_str(), message.length(), false);
}

std::string XiaoZhiAI::getJson() {
    /* 
        {
            "version": 2,
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "uuid": "00000000-0000-0000-0000-000000000000",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            },
            "board": {
                ...
            }
        }
    */
    std::string json = "{";
    json += "\"version\":2,";
    json += "\"language\":\"" + std::string(LANG_CN) + "\",";
    json += "\"flash_size\":" + std::to_string(SystemInfo::GetFlashSize()) + ",";
    json += "\"minimum_free_heap_size\":" + std::to_string(SystemInfo::GetMinimumFreeHeapSize()) + ",";
    json += "\"mac_address\":\"" + SystemInfo::GetMacAddress() + "\",";
    json += "\"uuid\":\"" + getUUID() + "\",";
    json += "\"chip_model_name\":\"" + SystemInfo::GetChipModelName() + "\",";
    json += "\"chip_info\":{";

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    json += "\"model\":" + std::to_string(chip_info.model) + ",";
    json += "\"cores\":" + std::to_string(chip_info.cores) + ",";
    json += "\"revision\":" + std::to_string(chip_info.revision) + ",";
    json += "\"features\":" + std::to_string(chip_info.features);
    json += "},";

    json += "\"application\":{";
    auto app_desc = esp_app_get_description();
    json += "\"name\":\"" + std::string(app_desc->project_name) + "\",";
    json += "\"version\":\"" + std::string(app_desc->version) + "\",";
    json += "\"compile_time\":\"" + std::string(app_desc->date) + "T" + std::string(app_desc->time) + "Z\",";
    json += "\"idf_version\":\"" + std::string(app_desc->idf_ver) + "\",";

    char sha256_str[65];
    for (int i = 0; i < 32; i++) {
        snprintf(sha256_str + i * 2, sizeof(sha256_str) - i * 2, "%02x", app_desc->app_elf_sha256[i]);
    }
    json += "\"elf_sha256\":\"" + std::string(sha256_str) + "\"";
    json += "},";

    json += "\"partition_table\": [";
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it) {
        const esp_partition_t *partition = esp_partition_get(it);
        json += "{";
        json += "\"label\":\"" + std::string(partition->label) + "\",";
        json += "\"type\":" + std::to_string(partition->type) + ",";
        json += "\"subtype\":" + std::to_string(partition->subtype) + ",";
        json += "\"address\":" + std::to_string(partition->address) + ",";
        json += "\"size\":" + std::to_string(partition->size);
        json += "},";
        it = esp_partition_next(it);
    }
    json.pop_back(); // Remove the last comma
    json += "],";

    json += "\"ota\":{";
    auto ota_partition = esp_ota_get_running_partition();
    json += "\"label\":\"" + std::string(ota_partition->label) + "\"";
    json += "},";

    json += "\"board\":" + getBoardJson();

    // Close the JSON object
    json += "}";
    return json;
}


std::string XiaoZhiAI::getBoardJson() {
    // Set the board type for OTA
    auto& wifi_station = CUBICAT.wifi;
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    // if (false) {
    //     board_json += "\"ssid\":\"" + wifi_station.getSSID() + "\",";
    //     board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ",";
    //     board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ",";
    //     board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";
    // }
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";
    return board_json;
}
void XiaoZhiAI::checkNewVersion() {
    // Check if there is a new firmware version available
    m_ota.SetPostData(getJson());
    auto display = &CUBICAT.lcd;
    auto spker = &CUBICAT.speaker;
    const int MAX_RETRY = 10;
    int retry_count = 0;
    const int retryInterval = 10;
    while (true) {
        if (!m_ota.CheckVersion()) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                LOGE("Too many retries, exit version check");
                return;
            }
            LOGW("Check new version failed, retry in %d seconds (%d/%d)", retryInterval, retry_count, MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(retryInterval * 1000));
            continue;
        }
        retry_count = 0;

        if (m_ota.HasNewVersion()) {
            // Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);
            // Wait for the chat state to be idle
            do {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } while (getState() != Idle);

            // Use main task to do the upgrade, not cancelable
            foregroundTask([this, display, spker]() {
                setState(Upgrading);
                
                // display->SetIcon(FONT_AWESOME_DOWNLOAD);
                std::string message = std::string("新版本 ") + m_ota.GetFirmwareVersion();
                // display->drawText(0,0, message.c_str(), BLACK, CUBICAT.lcd.width(), 2);

                m_wakeWordDetect.StopDetection();
                spker->setEnable(false);
                {
                    LOCK_OPUS_BUFFER
                    m_opusBufferQueue.clear();
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                m_ota.StartUpgrade([display](int progress, size_t speed) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->drawText(0,0, buffer, BLACK, CUBICAT.lcd.width(), 2);
                });
                // LOGI("Firmware upgrade failed...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                reboot();
            });

            return;
        }

        // No new version, mark the current version as valid
        m_ota.MarkCurrentVersionValid();
        // std::string message = std::string("版本: ") + m_ota.GetCurrentVersion();
        // display->ShowNotification(message.c_str());
    
        if (m_ota.HasActivationCode()) {
            // Activation code is valid
            setState(Idle);
            // ShowActivationCode();

            // Check again in 60 seconds or until the device is idle
            for (int i = 0; i < 60; ++i) {
                if (getState() == Idle) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }

        setState(Idle);
        // display->fillScreen(GRAY);
        // Exit the loop if upgrade or idle
        break;
    }
}

std::string GenerateUuid() {
    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];
    
    // 使用 ESP32 的硬件随机数生成器
    esp_fill_random(uuid, sizeof(uuid));
    
    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;    // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;    // 变体 1
    
    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
    
    return std::string(uuid_str);
}
std::string XiaoZhiAI::getUUID() {
    auto uuid = CUBICAT.storage.getString("uuid");
    if (uuid.empty()) {
        uuid = GenerateUuid();
        CUBICAT.storage.setString("uuid", uuid.c_str());
    }
    return uuid;
}
void XiaoZhiAI::reboot() {
    LOGI("Rebooting...");
    esp_restart();
}

void XiaoZhiAI::setState(DeviceState state) {
    if (m_eDeviceState != state) {
        m_eDeviceState = state;
        onStateChange();
    }
}

void XiaoZhiAI::onStateChange() {
    printf("state change: %s\n", getCurrentStateName().c_str());
    if (getState() == Idle) {
        CUBICAT.speaker.setEnable(false);
        m_wakeWordDetect.StartDetection();
#ifdef CONFIG_AUDIO_PROCESSING
        m_audioProcessor.Stop();
#endif
    } else if (getState() == Connecting) {

    } else if (getState() == Speaking) {
        LOCK_OPUS_BUFFER
        m_opusBufferQueue.clear();
        CUBICAT.speaker.setEnable(true);
        m_wakeWordDetect.StopDetection();
        m_pOpusDecoder->ResetState();
    } else if (getState() == Listening) {
        CUBICAT.speaker.setEnable(false);
        CUBICAT.mic.start();
#ifdef CONFIG_AUDIO_PROCESSING
        if (!m_audioProcessor.IsRunning()) {
            sendStartListening(kListeningModeRealtime);
            m_audioProcessor.Start();
            m_pOpusEncoder->ResetState();
            m_wakeWordDetect.StopDetection();
        }
#else
        sendStartListening(kListeningModeAutoStop);
        audioTask([this](){
            m_pOpusEncoder->ResetState();
        });
        m_wakeWordDetect.StopDetection();
#endif
    }
    auto state = getState();
    foregroundTask([this, state]() {
        if (m_stateCallback) {
            m_stateCallback(state);
        }
    });
}

std::string XiaoZhiAI::getCurrentStateName() {
    switch (m_eDeviceState) {
        case DeviceState::Idle:
            return "idle";
        case DeviceState::Connecting:
            return "connecting";
        case DeviceState::Speaking:
            return "speaking";
        case DeviceState::Listening:
            return "listening";
        case DeviceState::Upgrading:
            return "upgrading";
        default:
            return "unknown";
    }
}
