#ifndef _SOCKET_H_
#define _SOCKET_H_
#include "cJSON.h"
#include <functional>
#include <map>
#include <string>

#define TEXT_PROTOC0L   0
#define BINARY_PROTOCOL 1
#define AUDIO_PROTOCOL  2

class Socket {
public:
    virtual void connect(const char* host, int port, void* arg) = 0;
    virtual void reconnect() = 0;
    virtual int send(const char* data, unsigned int len, uint8_t type) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
protected:
    enum ConnectState {
        CONNECTING,
        CONNECTED,
        DISCONNECTED
    };
};

class SocketListener {
public:
    virtual void onBinaryData(const char* data, unsigned int len) = 0;
    virtual void onJsonData(cJSON* root) = 0;
    virtual void onBeginConnect() = 0;
    virtual void onConnected(Socket* socket) = 0;
    virtual void onDisconnected() = 0;
};
    

#endif