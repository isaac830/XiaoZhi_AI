#ifndef _SOCKET_H_
#define _SOCKET_H_
#include "cJSON.h"
#include <functional>
#include <map>
#include <string>

using CustomHeaders = std::map<std::string, std::string>;

class Socket {
public:
    virtual void connect(const char* uri, int port, CustomHeaders customHeaders) = 0;
    virtual void reconnect() = 0;
    virtual void send(const char* data, unsigned int len, bool binary) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
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