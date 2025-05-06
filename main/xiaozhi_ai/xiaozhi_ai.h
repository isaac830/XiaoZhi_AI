#ifndef _XIAOZHI_AI_H_
#define _XIAOZHI_AI_H_
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../socket/socket.h"
#include <vector>
#include <list>
#include <stdint.h>
#include <freertos/semphr.h>
#include "wake_detect/wake_word_detect.h"
#include "opus_decoder.h"
#include "opus_encoder.h"
#include "opus_resampler.h"
#include "devices/audio_buffer.h"
#include <functional>
#include "audio_processing/audio_processor.h"

// AEC not working right now
// #define CONFIG_AUDIO_PROCESSING

enum Emotion : uint8_t {
    Neutral = 0,
    Happy = 1,
    Sad = 2,
    Angry = 3,
    Surprise = 4,
    Disgust = 5,
    Fear = 6,
    Unknown
};

enum DeviceState : uint8_t {
    Idle = 0,
    Connecting,
    Speaking,
    Listening,
    Upgrading
};

using TTSCallback = std::function<void (const std::string& text)>;
using LLMCallback = std::function<void (Emotion emo)>;
using StateCallback = std::function<void (DeviceState state)>;
using ConnectionCallback = std::function<void (bool connected)>;

class XiaoZhiAI : public SocketListener
{
    enum ListeningMode {
        kListeningModeAutoStop,
        kListeningModeManualStop,
        kListeningModeRealtime // 需要 AEC 支持
    };
public:
    XiaoZhiAI();
    ~XiaoZhiAI();

    void onBinaryData(const char* data, unsigned int len) override;
    void onJsonData(cJSON* root) override;
    void onBeginConnect() override;
    void onConnected(Socket* socket) override;
    void onDisconnected() override;

    void loop();
    std::string getUUID();
    DeviceState getState() { return m_eDeviceState; }
    void setTTSCallback(TTSCallback ttsCallback) {m_ttsCallback = ttsCallback;}
    void setLLMCallback(LLMCallback llmCallback) {m_llmCallback = llmCallback;}
    void setStateCallback(StateCallback stateCallback) {m_stateCallback = stateCallback;}
    void setConnectionCallback(ConnectionCallback connectionCallback) {m_connectionCallback = connectionCallback;}

    // Internal use only
    void audioLoop();
private:
    void onServerHello(const cJSON* root);
    void setState(DeviceState state);
    void onStateChange();
    void onWakeWord();
    // Protocal begin
    void abortSpeaking();
    void sendWakeWord(const std::string& wakeWord);
    void sendAudio(const uint8_t* data, size_t len);
    void sendStartListening(ListeningMode mode);
    // Protocal end
    void foregroundTask(std::function<void()> callback);
    void audioTask(std::function<void()> callback);
    void sendBoardInfo(std::map<std::string, std::string> headers);
    void reboot();

    std::string getCurrentStateName();
    std::string getJson();
    std::string getBoardJson();
    Socket* m_socket = nullptr;
    std::list<std::vector<uint8_t>>     m_opusBufferQueue;
    std::recursive_mutex                m_opusMutex;
    std::recursive_mutex                m_taskMutex;
    std::recursive_mutex                m_audioTaskMutex;
    TaskHandle_t                        m_audioTaskHandle = nullptr;
    WakeWordDetect                      m_wakeWordDetect;
#ifdef CONFIG_AUDIO_PROCESSING
    AudioProcessor                      m_audioProcessor;
    AudioBuffer                         m_loopbackBuffer;
    OpusResampler                       m_speakerResampler;
#endif
    EventGroupHandle_t                  m_eventGroup = nullptr;
    std::atomic<DeviceState>            m_eDeviceState = Idle;
    std::list<std::function<void()>>    m_bgTasks;
    std::list<std::function<void()>>    m_audioTasks;
    uint16_t                            m_pcmFrameSize = 512;
    std::unique_ptr<OpusDecoderWrapper> m_pOpusDecoder;
    std::unique_ptr<OpusEncoderWrapper> m_pOpusEncoder;
    uint16_t                            m_opusFrameSize = 960;
    AudioBuffer                         m_opusFrameBuffer;
    bool                                m_bAutoWakeOnReconnect = false;
    std::string                         m_sLastWakeWord;
    TTSCallback                         m_ttsCallback = nullptr;
    LLMCallback                         m_llmCallback = nullptr;
    StateCallback                       m_stateCallback = nullptr;
    ConnectionCallback                  m_connectionCallback = nullptr;
};


#endif