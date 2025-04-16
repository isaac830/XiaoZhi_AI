#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_
#include "esp_http_client.h"
#include <string>
#include <unordered_map>

enum HttpMethod {
    GET,
    POST,
};

struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string url;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    uint32_t timeout{5000};
};

struct HttpResponse {
    int status_code = 0;
    std::string body = "";
    std::string error = "";
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse sendRequest(const HttpRequest& request);
    esp_err_t openRequest(const HttpRequest& request);
    void closeRequest();
    esp_err_t read(char* data, int len);
    uint32_t getOpenContentLen() { return m_contentLen; }
private:
    void createClient(const HttpRequest& request);
    esp_err_t onHttpEvent(esp_http_client_event_t* evt);
    esp_http_client_handle_t    m_client = nullptr;
    std::string                 m_dataReceived;
    uint32_t                    m_contentLen = 0;
};

#endif