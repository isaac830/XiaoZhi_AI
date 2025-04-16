let api = {
sendGlobalMessage: function(id, msg) {
    let f = ffi("void sendGlobalMessage(int, void*)");
    return f(id, msg);
},
sendLocalMessage: function(tube, id, msg) {
    let f = ffi("void sendLocalMessage(void*, int, void*)");
    return f(tube, id, msg);
},
timeNow: function(timeZone) {
    let f = ffi("uint32_t timeNow(int)");
    return f(timeZone);
},
getSceneManager: function() {
    let f = ffi("void* getSceneManager()");
    return f();
},
getResourceManager: function() {
    let f = ffi("void* getResourceManager()");
    return f();
},
getLCD: function() {
    let f = ffi("void* getLCD()");
    return f();
},
Node2D_rotate: function(ptr, angle) {
    let f = ffi("void Node2D_rotate(void*, float)");
    return f(ptr, angle);
},
Node2D_setPosition: function(ptr, x, y) {
    let f = ffi("void Node2D_setPosition(void*, float, float)");
    return f(ptr, x, y);
},
Node2D_setScale: function(ptr, vec_x, vec_y) {
    let f = ffi("void Node2D_setScale(void*, float, float)");
    return f(ptr, vec_x, vec_y);
},
SceneManager_createSpriteNode: function(ptr, texture, vec_x, vec_y, layer) {
    let f = ffi("void* SceneManager_createSpriteNode(void*, void*, float, float, int)");
    return f(ptr, texture, vec_x, vec_y, layer);
},
SceneManager_createQuad: function(ptr, texture, vec_x, vec_y, layer) {
    let f = ffi("void* SceneManager_createQuad(void*, void*, float, float, int)");
    return f(ptr, texture, vec_x, vec_y, layer);
},
SceneManager_createNode2D: function(ptr, layer) {
    let f = ffi("void* SceneManager_createNode2D(void*, int)");
    return f(ptr, layer);
},
SceneManager_getObjectById: function(ptr, id) {
    let f = ffi("void* SceneManager_getObjectById(void*, int)");
    return f(ptr, id);
},
SceneManager_getObjectByName: function(ptr, name) {
    let f = ffi("void* SceneManager_getObjectByName(void*, const string&)");
    return f(ptr, name);
},
SceneManager_deleteObject: function(ptr, id) {
    let f = ffi("void SceneManager_deleteObject(void*, int)");
    return f(ptr, id);
},
SceneManager_addNode: function(ptr, node) {
    let f = ffi("void SceneManager_addNode(void*, void*)");
    return f(ptr, node);
},
Node_setParent: function(ptr, parent) {
    let f = ffi("void Node_setParent(void*, void*)");
    return f(ptr, parent);
},
Node_setName: function(ptr, name) {
    let f = ffi("void Node_setName(void*, const string&)");
    return f(ptr, name);
},
Node_setVisible: function(ptr, visible) {
    let f = ffi("void Node_setVisible(void*, bool)");
    return f(ptr, visible);
},
Node_setScale: function(ptr, s) {
    let f = ffi("void Node_setScale(void*, float)");
    return f(ptr, s);
},
Node_getDrawable: function(ptr, index) {
    let f = ffi("void* Node_getDrawable(void*, int)");
    return f(ptr, index);
},
ResourceManager_loadTexture: function(ptr, name, fromFlash) {
    let f = ffi("void* ResourceManager_loadTexture(void*, char*, bool)");
    return f(ptr, name, fromFlash);
},
ResourceManager_removeTexture: function(ptr, name) {
    let f = ffi("void ResourceManager_removeTexture(void*, char*)");
    return f(ptr, name);
},
ResourceManager_getTexture: function(ptr, name) {
    let f = ffi("void* ResourceManager_getTexture(void*, char*)");
    return f(ptr, name);
},
Drawable_materialPtr: function(ptr) {
    let f = ffi("void* Drawable_materialPtr(void*)");
    return f(ptr);
},
Material_setTexture: function(ptr, texture) {
    let f = ffi("void Material_setTexture(void*, void*)");
    return f(ptr, texture);
},
Material_texturePtr: function(ptr) {
    let f = ffi("void* Material_texturePtr(void*)");
    return f(ptr);
},
Material_setColor: function(ptr, color) {
    let f = ffi("void Material_setColor(void*, int)");
    return f(ptr, color);
},
Material_setMask: function(ptr, hasMask) {
    let f = ffi("void Material_setMask(void*, bool)");
    return f(ptr, hasMask);
},
Material_setMaskColor: function(ptr, color) {
    let f = ffi("void Material_setMaskColor(void*, int)");
    return f(ptr, color);
},
Material_setEmissive: function(ptr, e) {
    let f = ffi("void Material_setEmissive(void*, float)");
    return f(ptr, e);
},
Material_setBlendMode: function(ptr, mode) {
    let f = ffi("void Material_setBlendMode(void*, int)");
    return f(ptr, mode);
},
Material_setBilinearFilter: function(ptr, b) {
    let f = ffi("void Material_setBilinearFilter(void*, bool)");
    return f(ptr, b);
},
Material_setTransparent: function(ptr, t) {
    let f = ffi("void Material_setTransparent(void*, bool)");
    return f(ptr, t);
},
Texture_setAsSpriteSheet: function(ptr, row, col) {
    let f = ffi("void Texture_setAsSpriteSheet(void*, int, int)");
    return f(ptr, row, col);
},
Texture_setFrame: function(ptr, nth) {
    let f = ffi("void Texture_setFrame(void*, int)");
    return f(ptr, nth);
},
Texture_shallowCopy: function(ptr) {
    let f = ffi("void* Texture_shallowCopy(void*)");
    return f(ptr);
},
connect: function(ssid, password) {
    let f = ffi("bool connect(char*, char*)");
    return f(ssid, password);
},
Display_drawLine: function(ptr, x1, y1, x2, y2, color, thickness) {
    let f = ffi("void Display_drawLine(void*, int, int, int, int, int, int)");
    return f(ptr, x1, y1, x2, y2, color, thickness);
},
Display_drawRect: function(ptr, x, y, w, h, color, thickness) {
    let f = ffi("void Display_drawRect(void*, int, int, int, int, int, int)");
    return f(ptr, x, y, w, h, color, thickness);
},
Display_fillRect: function(ptr, x, y, w, h, color) {
    let f = ffi("void Display_fillRect(void*, int, int, int, int, int)");
    return f(ptr, x, y, w, h, color);
},
Display_fillRoundRect: function(ptr, x, y, w, h, color, cornerRadius) {
    let f = ffi("void Display_fillRoundRect(void*, int, int, int, int, int, int)");
    return f(ptr, x, y, w, h, color, cornerRadius);
},
Display_drawCircle: function(ptr, x, y, radius, color, thickness) {
    let f = ffi("void Display_drawCircle(void*, int, int, int, int, int)");
    return f(ptr, x, y, radius, color, thickness);
},
Display_fillCircle: function(ptr, x, y, radius, color) {
    let f = ffi("void Display_fillCircle(void*, int, int, int, int)");
    return f(ptr, x, y, radius, color);
},
Display_fillScreen: function(ptr, color) {
    let f = ffi("void Display_fillScreen(void*, int)");
    return f(ptr, color);
},
};
