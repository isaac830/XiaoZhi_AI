#include "websocket.h"
#include "esp_crt_bundle.h"
#include "utils/logger.h"
#include <string>
#include "cubicat.h"

#define RECONNECT_INTERVAL_MS 5 * 1000
static SemaphoreHandle_t ws_mutex = xSemaphoreCreateMutex();

#define OP_CONTINUATION     0x0
#define OP_TEXT             0x1
#define OP_BINARY           0x2
#define OP_CLOSE            0x8
#define OP_PING             0x9
#define OP_PONG             0xA


#define log_error_if_nonzero(message, error_code) \
{ \
    if (error_code != 0) { \
        LOGE("Last error %s: 0x%x", message, error_code); \
    } \
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    WebSocket *ws = (WebSocket *)handler_args;
    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ws->onConnected();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        LOGI("WEBSOCKET_EVENT_DISCONNECTED");
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        // close frame
        if (data->op_code == OP_CLOSE) {
            uint16_t status_code = 0;
            if (data->data_len >= 2) {
                status_code = (data->data_ptr[0] << 8) | data->data_ptr[1];
            } else {
                status_code = 1005; // CLOSE_NO_STATUS
            }
            LOGI("Connection closed by peer, status=%d", status_code);
            ws->safeDisconnect();
        } else {
            ws->onDataReceived((char *)data->data_ptr, data->data_len, data->op_code);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        LOGI("WEBSOCKET_EVENT_ERROR");
        // log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_FINISH:
        ws->safeDisconnect();
        break;
    }
}

void SocketWatchTask(void *arg) {
    uint32_t notification_value;
    while (1) {
        if (xTaskNotifyWait(0, ULONG_MAX, &notification_value, portMAX_DELAY) == pdTRUE) {
            break;
        }
    }
    WebSocket* ws = (WebSocket*)notification_value;
    ws->disconnect();
}

WebSocket::WebSocket() {
}

WebSocket::~WebSocket() {
    disconnect();
}

void WebSocket::connect(const char* uri, int port, void* arg) {
    if (m_eConnectState == CONNECTING) {
        return;
    }
    if (m_eConnectState == CONNECTED) {
        disconnect();
    }
    esp_websocket_client_config_t websocket_cfg;
    memset(&websocket_cfg, 0, sizeof(esp_websocket_client_config_t));
    if (port < 1) {
        if (startsWith(uri, "wss://")) {
            port = 443;
        } else {
            port = 80;
        }
    }
    m_sUri = uri;
    m_nPort = port;
    websocket_cfg.uri = uri;
    websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
    websocket_cfg.skip_cert_common_name_check = true;
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    websocket_cfg.keep_alive_enable = true;
    websocket_cfg.keep_alive_interval = 5;
    websocket_cfg.keep_alive_idle = 5;
    websocket_cfg.port = port;
    websocket_cfg.buffer_size = 1024 * 4;
    websocket_cfg.reconnect_timeout_ms = RECONNECT_INTERVAL_MS;

    m_userHeaders = *static_cast<CustomHeaders*>(arg);
    m_client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(m_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)this);
    for (const auto& header : m_userHeaders) {
        esp_websocket_client_append_header(m_client, header.first.c_str(), header.second.c_str());
    }
    if (m_pListener) {
        m_pListener->onBeginConnect();
    }
    esp_websocket_client_start(m_client);
    m_eConnectState = CONNECTING;
    xTaskCreatePinnedToCoreWithCaps(SocketWatchTask, "SocketWatchTask", 1024 * 4, (void *)this,
     5, &m_watchTaskHandle, 0, MALLOC_CAP_SPIRAM);
}

void WebSocket::reconnect() {
    connect(m_sUri.c_str(), m_nPort, &m_userHeaders);
}

void WebSocket::onConnected() {
    LOGW("WebSocket connected");
    m_eConnectState = CONNECTED;
    if (m_pListener) {
        m_pListener->onConnected(this);
    }
}

void WebSocket::onDisconnected() {
    LOGW("WebSocket disconnected");
    m_eConnectState = DISCONNECTED;
    if (m_pListener) {
        m_pListener->onDisconnected();
    }
}
void WebSocket::onDataReceived(const char* data, size_t len, uint8_t opCode) {
    if (opCode == OP_BINARY) {
        if (data && m_pListener) {
            m_pListener->onBinaryData(data, len);
        }
    } else if (opCode == OP_TEXT) {
        // Parse JSON data
        auto root = cJSON_Parse(data);
        if (root) {
            if (m_pListener) {
                m_pListener->onJsonData(root);
            }
        } else {
            LOGE("Parse JSON data failed %s", data);
        }
        cJSON_Delete(root);
    }
}

int WebSocket::send(const char* data, size_t len, uint8_t type) {
    if (!isConnected()) {
        return 0;
    }
    int sizeSend;
    xSemaphoreTake(ws_mutex, portMAX_DELAY);
    if (type == BINARY_PROTOCOL) {
        sizeSend = esp_websocket_client_send_bin(m_client, data, len, portMAX_DELAY);
    } else {
        sizeSend = esp_websocket_client_send_text(m_client, data, len, portMAX_DELAY);
    }
    xSemaphoreGive(ws_mutex);
    return sizeSend;
}
void WebSocket::disconnect() {
    onDisconnected();
    xSemaphoreTake(ws_mutex, portMAX_DELAY);
    if (esp_websocket_client_is_connected(m_client)) {
        esp_websocket_client_close(m_client, 0);
    }
    esp_websocket_client_destroy(m_client);
    xSemaphoreGive(ws_mutex);
    m_client = nullptr;
    m_watchTaskHandle = nullptr;
    vTaskDelay(1);
    vTaskDelete(m_watchTaskHandle);
}

void WebSocket::safeDisconnect() {
    xTaskNotify(m_watchTaskHandle, (uint32_t)this, eSetValueWithOverwrite);
}

bool WebSocket::isConnected() {
    return m_eConnectState == CONNECTED;
}