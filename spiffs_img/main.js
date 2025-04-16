load('cubicat_api.js');
load('common.js');
load('spine_api.js');

// api.connect(YOUR_SSID, YOUR_PASSWORD);

let lastTouchPt = null;
let screen_width = 320;
let screen_height = 240;

let sceneMgr = api.getSceneManager();
let resMgr = api.getResourceManager();
let lcd = api.getLCD();
api.Display_fillScreen(lcd, 0x0);

let spineNode = spine.createSpine();
api.SceneManager_addNode(sceneMgr, spineNode);
spine.SpineNode_loadWithBinaryFile(spineNode, "/spiffs/c131/c131_00.skel", "/spiffs/c131/c131_00.atlas", 0.135);
spine.SpineNode_setSkinByName(spineNode, "00");
spine.SpineNode_setAnimation(spineNode, 0, "idle", true);
api.Node2D_setPosition(spineNode, screen_width / 2, -40);

let now = api.timeNow(8);
// 创建计时器背景
let headerImg = api.ResourceManager_loadTexture(resMgr, '/spiffs/header.png', true);
let header = api.SceneManager_createSpriteNode(sceneMgr, headerImg, 0.5, 1, Layer2D.FOREGROUND);
api.Node2D_setPosition(header, screen_width / 2, screen_height);
// create 7-seg H0 display
let yOffset = -22;
let h0Img = api.ResourceManager_loadTexture(resMgr, '/spiffs/seg7.png', true);
let h0 = api.SceneManager_createSpriteNode(sceneMgr, h0Img, 0.5, 0.5,Layer2D.FOREGROUND);
let drawable = api.Node_getDrawable(h0, 0);
let mat = api.Drawable_materialPtr(drawable);
let texSeg7 = api.Material_texturePtr(mat);
api.Texture_setAsSpriteSheet(texSeg7, 2, 5);
api.Node_setParent(h0, header);
api.Node2D_setPosition(h0, -60, yOffset);
// create 7-seg H1 display
let h1Img = api.Texture_shallowCopy(h0Img);
let h1 = api.SceneManager_createSpriteNode(sceneMgr, h1Img, 0.5, 0.5,Layer2D.FOREGROUND);
api.Node_setParent(h1, header);
api.Node2D_setPosition(h1, -24, yOffset);
// create 7-seg M0 display
let m0Img = api.Texture_shallowCopy(h0Img);
let m0 = api.SceneManager_createSpriteNode(sceneMgr, m0Img, 0.5, 0.5,Layer2D.FOREGROUND);
api.Node_setParent(m0, header);
api.Node2D_setPosition(m0, 24, yOffset);
// create 7-seg M1 display
let m1Img = api.Texture_shallowCopy(h0Img);
let m1 = api.SceneManager_createSpriteNode(sceneMgr, m1Img, 0.5, 0.5,Layer2D.FOREGROUND);
api.Node_setParent(m1, header);
api.Node2D_setPosition(m1, 60, yOffset);
// create background
let bg = api.ResourceManager_loadTexture(resMgr, '/spiffs/room.png', true);
let bgNode = api.SceneManager_createSpriteNode(sceneMgr, bg, 0.5, 0.5, Layer2D.BACKGROUND);
api.Node2D_setPosition(bgNode, 160, 120);
// create offline signal
let offlineImg = api.ResourceManager_loadTexture(resMgr, '/spiffs/offline.png', true);
let offline = api.SceneManager_createSpriteNode(sceneMgr, offlineImg, 0.5, 0.5, Layer2D.FOREGROUND);
api.Node2D_setPosition(offline, 160, 120);

function onXiaoZhiEmotion(emo) {
    if (emo === 1) {
        spine.SpineNode_setAnimation(spineNode, 0, "action", false);
    } else if (emo === 4) {
        spine.SpineNode_setAnimation(spineNode, 0, "suprise", false);
    } else if (emo === 7) {
        spine.SpineNode_setAnimation(spineNode, 0, "delight", false);
    } else {
        print("emo not implemented:");
        print(emo)
    }
    spine.SpineNode_addAnimation(spineNode, 0, "idle", true, 0);
}
function onXiaoZhiState(state) {
    if (state === 2) {
        spine.SpineNode_setAnimation(spineNode, 1, "talk_start", true);
    } else if (state === 3) {
        spine.SpineNode_setAnimation(spineNode, 0, "idle", true);
        spine.SpineNode_setAnimation(spineNode, 1, "talk_end", false);
    }
}
function onXiaoZhiConnected(connected) {
    api.Node_setVisible(offline, !connected);
}
function loop(t) {
    let now = api.timeNow(8);
    let min = (now % 3600) / 60.0;
    let hour = (now % 86400) / 3600.0;
    let minInt = min|0;
    let hourInt = hour|0;
    api.Texture_setFrame(h0Img, hourInt / 10);
    api.Texture_setFrame(h1Img, hourInt % 10);
    api.Texture_setFrame(m0Img, minInt / 10);
    api.Texture_setFrame(m1Img, minInt % 10);
}
function onTouch(x,y) {
    let pt = Vector2(x,y);
    if (lastTouchPt) {
        if (!pt.equals(lastTouchPt)) {
            let dir = pt.sub(lastTouchPt);
        }
    }
    lastTouchPt = pt;
}
function onRelease() {
    lastTouchPt = null;
}