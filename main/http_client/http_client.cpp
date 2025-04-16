#include "http_client.h"
#include "esp_crt_bundle.h"
#include "utils/logger.h"

HttpClient::HttpClient() {

}

HttpClient::~HttpClient() {
    closeRequest();
}


esp_err_t HttpClient::onHttpEvent(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            LOGI( "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            LOGI( "Received data: %.*s", evt->data_len, (char*)evt->data);
            m_dataReceived.append((char*)evt->data, evt->data_len);
            break;
        default:
            break;
    }
    return ESP_OK;
}
void HttpClient::createClient(const HttpRequest& req) {
    esp_http_client_config_t config = {
        .url = req.url.c_str(),
        .cert_pem = NULL,
        .cert_len = 0,
        .method = HTTP_METHOD_POST,
        .event_handler = [](esp_http_client_event_t *evt) -> int {
            return static_cast<HttpClient*>(evt->user_data)->onHttpEvent(evt);
        },
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = this,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    m_client = esp_http_client_init(&config);

    // 设置请求头
    for (auto header : req.headers) {
        esp_http_client_set_header(m_client, header.first.c_str(), header.second.c_str());
    }
    if (req.method == HttpMethod::POST) {
        esp_http_client_set_method(m_client, HTTP_METHOD_POST);
    } else if (req.method == HttpMethod::GET) {
        esp_http_client_set_method(m_client, HTTP_METHOD_GET);
    }
}
HttpResponse HttpClient::sendRequest(const HttpRequest& req) {
    createClient(req);
    // set post field
    esp_http_client_set_post_field(m_client, req.body.c_str(), req.body.length());
    m_dataReceived.clear();
    esp_err_t err = esp_http_client_perform(m_client);
    HttpResponse response;
    if (err == ESP_OK) {
        response.status_code = esp_http_client_get_status_code(m_client);
        response.body = std::move(m_dataReceived);
    } else {
        response.error = esp_err_to_name(err);
    }
    esp_http_client_cleanup(m_client);
    m_client = nullptr;
    return response;
}

esp_err_t HttpClient::openRequest(const HttpRequest& req) {
    createClient(req);
    auto writeLen = req.method == HttpMethod::POST ? req.body.length(): 0;
    if (esp_http_client_open(m_client, writeLen) != ESP_OK) {
        return ESP_FAIL;
    }
    if (req.method == HttpMethod::POST) {
        int len = esp_http_client_write(m_client, req.body.c_str(), writeLen);
        if (len != writeLen) {
            return ESP_FAIL;
        }
    }
    m_contentLen = esp_http_client_fetch_headers(m_client);
    return ESP_OK;
}

void HttpClient::closeRequest() {
    if (m_client) {
        esp_http_client_close(m_client);
        esp_http_client_cleanup(m_client);
        m_client = nullptr;
    }
}
esp_err_t HttpClient::read(char* data, int len) {
    return esp_http_client_read(m_client, data, len);
}