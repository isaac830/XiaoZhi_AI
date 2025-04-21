#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_
#include <stdio.h>
#include "esp_websocket_client.h"
#include "cJSON.h"
#include "socket.h"
#include <functional>
#include <string>

using IncomingAudioCallback = std::function<void (const char* data, size_t len)> ;
using IncomingJsonCallback = std::function<void (cJSON* json)> ;
using CustomHeaders = std::map<std::string, std::string>;

class WebSocket : public Socket {

public:
    WebSocket();
    ~WebSocket();

    void connect(const char* uri, int port, void* customHeaders) override;
    void reconnect() override;
    int send(const char* data, size_t len, uint8_t type) override;
    void disconnect() override;
    bool isConnected() override;

    void safeDisconnect();

    void setSocketListener(SocketListener* listener) { m_pListener = listener; }

    void onConnected();
    void onDisconnected();
    void onDataReceived(const char* data, size_t len, uint8_t opCode);
private:
    esp_websocket_client_handle_t m_client = nullptr;
    ConnectState            m_eConnectState = DISCONNECTED;
    SocketListener*         m_pListener = nullptr;
    TaskHandle_t            m_watchTaskHandle = nullptr;
    std::string             m_sUri; 
    int                     m_nPort = -1;
    CustomHeaders           m_userHeaders;
};

#endif