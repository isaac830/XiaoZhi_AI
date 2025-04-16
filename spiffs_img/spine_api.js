let spine = {
createSpine: function() {
    let f = ffi("void* createSpine()");
    return f();
},
SpineNode_loadWithBinaryFile: function(ptr, skeletonBinaryFile, atlasFile, scale) {
    let f = ffi("void SpineNode_loadWithBinaryFile(void*, char*, char*, float)");
    return f(ptr, skeletonBinaryFile, atlasFile, scale);
},
SpineNode_setSkinByName: function(ptr, skinName) {
    let f = ffi("void SpineNode_setSkinByName(void*, char*)");
    return f(ptr, skinName);
},
SpineNode_setSkinByIndex: function(ptr, idx) {
    let f = ffi("void SpineNode_setSkinByIndex(void*, int)");
    return f(ptr, idx);
},
SpineNode_setAnimation: function(ptr, trackIndex, name, loop) {
    let f = ffi("void* SpineNode_setAnimation(void*, int, char*, bool)");
    return f(ptr, trackIndex, name, loop);
},
SpineNode_addAnimation: function(ptr, trackIndex, name, loop, delay) {
    let f = ffi("void* SpineNode_addAnimation(void*, int, char*, bool, float)");
    return f(ptr, trackIndex, name, loop, delay);
},
SpineNode_clearTrack: function(ptr, trackIndex) {
    let f = ffi("void SpineNode_clearTrack(void*, int)");
    return f(ptr, trackIndex);
},
SpineNode_setScale: function(ptr, vec_x, vec_y) {
    let f = ffi("void SpineNode_setScale(void*, float, float)");
    return f(ptr, vec_x, vec_y);
},
SpineNode_setPosition: function(ptr, vec_x, vec_y) {
    let f = ffi("void SpineNode_setPosition(void*, float, float)");
    return f(ptr, vec_x, vec_y);
},
SpineNode_useBilinearFilter: function(ptr, b) {
    let f = ffi("void SpineNode_useBilinearFilter(void*, bool)");
    return f(ptr, b);
},
};
