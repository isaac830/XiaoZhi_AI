#include <stdio.h>
#include "cubicat.h"
#include "utils/helper.h"
#include "cubicat_spine.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "js_binding/js_binding.h"
#include <vector>
#include <mbedtls/base64.h>
#include "xiaozhi_ai/xiaozhi_ai.h"
#include "socket/websocket.h"
#include <esp_mac.h>
#include "sounds.h"
#include "cubicat_spine.h"

using namespace cubicat;


#define LVGL_TICK_PERIOD_MS    5
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
static lv_disp_drv_t disp_drv;      // contains callback functions
static lv_indev_drv_t touch_drv;
static lv_img_dsc_t buffer_desc;
uint16_t* screenBuffer = nullptr;
LV_FONT_DECLARE(yuanti_18);

extern void Register_SPINE_API();

static void tick_task(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void initLvglEnv() {
    lv_init();
    uint32_t bufPixels = CUBICAT.lcd.width() * CUBICAT.lcd.height() / 8;
    void* buf = heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf, nullptr, bufPixels);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = CUBICAT.lcd.width();
    disp_drv.ver_res = CUBICAT.lcd.height();
    disp_drv.flush_cb = [](lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
        CUBICAT.lcd.pushPixelsToScreen(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
        lv_disp_flush_ready(drv);
    };
    disp_drv.draw_buf = &disp_buf;
    // disp_drv.screen_transp = 1;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    // register touch driver
    lv_indev_drv_init(&touch_drv); // 初始化驱动结构
    touch_drv.type = LV_INDEV_TYPE_POINTER; // 设置为指针类型
    touch_drv.read_cb = [](lv_indev_drv_t* drv, lv_indev_data_t* data) {
        if (CUBICAT.lcd.isTouched()) {
            const TOUCHINFO& info = CUBICAT.lcd.getTouchInfo();
            data->state = LV_INDEV_STATE_PR;
            data->point.x = info.x[0];
            data->point.y = info.y[0];
        }else {
            data->state = LV_INDEV_STATE_REL;  // release touch state
        }
    };
    lv_indev_drv_register(&touch_drv);
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = tick_task,
        .arg = NULL,
        .name = "lvgl_tick",
        .skip_unhandled_events = true
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
}

lv_obj_t* createBackBufferObj() {
    // 创建 LVGL 图像对象
    lv_obj_t * imgObj = lv_img_create(lv_scr_act());
    // 使用自定义缓冲区创建图像
    buffer_desc.header.w = CUBICAT.lcd.width();
    buffer_desc.header.h = CUBICAT.lcd.height();
    buffer_desc.header.cf = LV_IMG_CF_TRUE_COLOR;
    buffer_desc.data_size = buffer_desc.header.w*buffer_desc.header.h*sizeof(uint16_t);
    buffer_desc.data = (const uint8_t *)CUBICAT.lcd.getRenderBuffer().data;
    lv_img_set_src(imgObj, &buffer_desc); 
    lv_obj_align(imgObj, LV_ALIGN_CENTER, 0, 0);
    return imgObj;
}

void create_keyboard(lv_obj_t *parent) {
    lv_obj_t *keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(keyboard, LV_HOR_RES, LV_VER_RES / 2); // 设置键盘大小
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER); // 设置为文本模式
    
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, LV_HOR_RES, LV_VER_RES / 2); // 设置文本区域大小
    lv_textarea_set_placeholder_text(ta, "ssid..."); // 设置提示文本
    /* 关联键盘与输入框 */
    lv_keyboard_set_textarea(keyboard, ta);
}

std::string GetMacAddress() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    esp_rom_printf("!!! stack overflow !!! stack name: %s \n", pcTaskName);
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xTask);
    esp_rom_printf("stack high water mark: %u ( %u bytes left)\n", uxHighWaterMark, uxHighWaterMark * sizeof(StackType_t));
}

void lvObjAnimMove(lv_obj_t *obj, int16_t x, int16_t y, int timeMS) {
    // X move
    lv_anim_t anim_x;
    lv_anim_init(&anim_x);
    lv_anim_set_var(&anim_x, obj);
    lv_anim_set_exec_cb(&anim_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&anim_x, lv_obj_get_x(obj), x);
    lv_anim_set_time(&anim_x, timeMS);
    lv_anim_start(&anim_x);

    // Y move
    lv_anim_t anim_y;
    lv_anim_init(&anim_y);
    lv_anim_set_var(&anim_y, obj);
    lv_anim_set_exec_cb(&anim_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&anim_y, lv_obj_get_y(obj), y);
    lv_anim_set_time(&anim_y, timeMS);
    lv_anim_start(&anim_y);
}

void connectToXiaoZhi(Socket* socket, XiaoZhiAI* xiaozhi) {
    const char* uri = "wss://api.tenclass.net/xiaozhi/v1/"; int port = 443;
    // const char* uri = "ws://192.168.0.12:8000/xiaozhi/v1/"; int port = 8000;
    CustomHeaders headers;
    headers["Authorization"] = "Bearer " + std::string("test-token");
    headers["Protocol-Version"] = "1";
    headers["Device-Id"] = GetMacAddress();
    headers["Client-Id"] = xiaozhi->getUUID();
    socket->connect(uri, port, &headers);
}

void connectToMyServer(Socket* socket) {
    const char* uri = "192.168.0.12"; int port = 8201;
    socket->connect(uri, port, nullptr);
}

#define CHAT_BG_UP lvObjAnimMove(textBG, 0, CUBICAT.lcd.height() - textBGHeight, 200);
#define CHAT_BG_DOWN lvObjAnimMove(textBG, 0, CUBICAT.lcd.height(), 200);

extern "C" void app_main(void)
{
    CUBICAT.begin(true, true, true, false);
    initLvglEnv();
    auto backBufferObj = createBackBufferObj(); 
    // 创建lvgl对话框
    const int textBGHeight = 40;
    lv_obj_t * textBG = lv_obj_create(lv_layer_top());
    lv_obj_set_size(textBG, CUBICAT.lcd.width(), textBGHeight); 
    lv_obj_set_pos(textBG, 0, CUBICAT.lcd.height());
    static lv_style_t textBGStyle;
    lv_style_init(&textBGStyle);
    lv_style_set_bg_color(&textBGStyle, lv_color_make(128,128,128));
    lv_style_set_bg_opa(&textBGStyle, LV_OPA_COVER);
    lv_style_set_border_width(&textBGStyle, 0);  // 无边框
    lv_obj_add_style(textBG, &textBGStyle, LV_PART_MAIN);
    lv_obj_t * chat_text = lv_label_create(textBG);  
    lv_obj_set_pos(chat_text, 0, 0);
    // 设置样式（字体、颜色等）
    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, lv_color_make(255, 255, 255));
    lv_style_set_text_font(&style_label, &yuanti_18);
    lv_obj_add_style(chat_text, &style_label, LV_PART_MAIN);
    lv_label_set_text(chat_text, "");

    WebSocket* socket = new WebSocket();
    XiaoZhiAI* xiaozhi = new XiaoZhiAI();
    socket->setSocketListener(xiaozhi);
    CUBICAT.wifi.connectAsync("", "", [=](bool success,const char* ip) {
        if (success) {
            connectToXiaoZhi(socket, xiaozhi);
        } else {
            CUBICAT.wifi.smartConnect([=](bool success,const char* ip) {
                if (success) {
                    connectToXiaoZhi(socket, xiaozhi);
                }
            }, [=](SmartConfigState status) {
                std::string text = "";
                if (status == StartFinding) {
                    text = "正在查找设备...";
                } else if (status == StartConnecting) {
                    text = "正在连接设备...";
                } else {
                    text = "连接失败";
                }
                lv_label_set_text(chat_text, text.c_str());
                CHAT_BG_UP
            });
        }
    });
    CUBICAT.lcd.fillScreen(GRAY);
    CubicatSpineExtension::init();
    CubicatTextureLoader::init(SPIFFS);   
#if CONFIG_JAVASCRIPT_ENABLE
    JSBindingInit("/spiffs", [](){
        Register_SPINE_API();
    });
#else
    // ===========创建角色===========
    auto sceneMgr = CUBICAT.engine.getSceneManager();
    auto resMgr = CUBICAT.engine.getResourceManager();
    auto spineNode = SpineNode::create();
    sceneMgr->addNode(spineNode);
    spineNode->loadWithBinaryFile("/spiffs/c131/c131_00.skel", "/spiffs/c131/c131_00.atlas", 0.135);
    spineNode->setAnimation(0, "idle", true);
    spineNode->setPosition({CUBICAT.lcd.width() / 2.0f, -40});
    // ===========创建背景===========
    auto bg = resMgr->loadTexture("/spiffs/room.png", true);
    auto bgNode = sceneMgr->createSpriteNode(bg, {0.0, 0.0}, Layer2D::BACKGROUND);
    // ===========创建离线标识===========
    auto offlineImg = resMgr->loadTexture("/spiffs/offline.png", true);
    auto offline = sceneMgr->createSpriteNode(offlineImg, {0.5, 0.5}, Layer2D::FOREGROUND);
    offline->setPosition(CUBICAT.lcd.width() / 2.0f, CUBICAT.lcd.height() / 2.0f);
    // ===========创建计时器==========
    // 创建计时器背景
    auto clockBGImg = resMgr->loadTexture("/spiffs/header.png", true);
    auto clockBG = sceneMgr->createSpriteNode(clockBGImg, {0.5, 1}, Layer2D::FOREGROUND);
    clockBG->setPosition(CUBICAT.lcd.width() / 2.0f, (float)CUBICAT.lcd.height());
    // 创建 7段显示 H0
    auto yOffset = -22;
    auto h0Img = resMgr->loadTexture("/spiffs/seg7.png", true);
    // 把图片转位sprite sheet
    h0Img->setAsSpriteSheet(2, 5);
    auto h0 = sceneMgr->createSpriteNode(h0Img, {0.5f, 0.5f}, Layer2D::FOREGROUND);
    h0->setParent(clockBG);
    h0->setPosition(-60, yOffset);
    // 浅拷贝一个实例出来给时钟个位使用
    auto h1Img = TexturePtr(h0Img->shallowCopy());
    auto h1 = sceneMgr->createSpriteNode(h1Img, {0.5f, 0.5f}, Layer2D::FOREGROUND);
    h1->setParent(clockBG);
    h1->setPosition(-24, yOffset);
    // 浅拷贝一个实例出来给分针十位使用
    auto m0Img = TexturePtr(h0Img->shallowCopy());
    auto m0 = sceneMgr->createSpriteNode(m0Img, {0.5, 0.5}, Layer2D::FOREGROUND);
    m0->setParent(clockBG);
    m0->setPosition(24, yOffset);
    // 浅拷贝一个实例出来给分针个位使用
    auto m1Img = TexturePtr(h0Img->shallowCopy());
    auto m1 = sceneMgr->createSpriteNode(m1Img, {0.5, 0.5}, Layer2D::FOREGROUND);
    m1->setParent(clockBG);
    m1->setPosition(60, yOffset);
#endif
    xiaozhi->setTTSCallback([chat_text, textBG](const std::string& text){
        lv_label_set_text(chat_text, text.c_str());
        CHAT_BG_UP
    });
#if CONFIG_JAVASCRIPT_ENABLE
    xiaozhi->setLLMCallback([](Emotion emo){
        MJS_CALL("onXiaoZhiEmotion", 1, (double)emo);
#else
    xiaozhi->setLLMCallback([spineNode](Emotion emo){
        if (emo == Happy) {
            spineNode->setAnimation(0, "action", false);
        } else if (emo == Surprise) {
            spineNode->setAnimation(0, "suprise", false);
        } else if (emo == Sad || emo == Angry) {
            spineNode->setAnimation(0, "angry", false);
        } else {
            printf("emo not implemented: %d\n", emo);
        }
        spineNode->addAnimation(0, "idle", true, 0);
#endif
    });

#if CONFIG_JAVASCRIPT_ENABLE
    xiaozhi->setStateCallback([textBG, chat_text](DeviceState state){
        MJS_CALL("onXiaoZhiState", 1, (double)state);
#else
    xiaozhi->setStateCallback([textBG, chat_text, spineNode](DeviceState state){
        if (state == Speaking) {
            spineNode->setAnimation(1, "talk_start", true);
        } else if (state == Listening) {
            spineNode->setAnimation(0, "idle", true);
            spineNode->setAnimation(1, "talk_end", false);
        }
#endif
        if (state == Speaking) {
            lv_label_set_text(chat_text, "");
        } else if (state == Listening) {
            CHAT_BG_DOWN
        }
    });
#if CONFIG_JAVASCRIPT_ENABLE
    xiaozhi->setConnectionCallback([chat_text, textBG](bool connected){
        MJS_CALL("onXiaoZhiConnected", 1, connected);
#else
    xiaozhi->setConnectionCallback([chat_text, textBG, offline](bool connected){
        offline->setVisible(!connected);
#endif  
        if (connected) {
            lv_label_set_text(chat_text, "连接成功");
            CHAT_BG_DOWN
        }
    });

    while (1)
    {
        xiaozhi->loop();
        CUBICAT.loop(false);
#if !CONFIG_JAVASCRIPT_ENABLE
        auto now = timeNow(8);
        int min = (now % 3600) / 60.0;
        int hour = (now % 86400) / 3600.0;
        h0Img->setFrame(hour / 10);
        h1Img->setFrame(hour % 10);
        m0Img->setFrame(min / 10);
        m1Img->setFrame(min % 10);
#endif
        lv_timer_handler();
        lv_obj_invalidate(backBufferObj);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}