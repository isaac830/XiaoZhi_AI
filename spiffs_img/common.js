let Layer2D = {
    BACKGROUND: 0,
    FOREGROUND: 1,
};
let BlendMode = {
    Normal: 0,
    Additive: 1,
    Multiply: 2
};
let LightType = {
    Directional: 0,
    Point: 1,
};
function Vector2(x, y) {
    return {
        x: x,
        y: y,
        add: function(other) {
            return Vector2(this.x + other.x, this.y + other.y);
        },
        mul_scalar: function(s) {
            return Vector2(this.x * s, this.y * s);
        },
        sub: function(other) {
            return Vector2(this.x - other.x, this.y - other.y);
        },
        equals: function(other) {
            return this.x === other.x && this.y === other.y;
        }
    };
}
function Vector3(x, y, z) {
    return {
        x: x,
        y: y,
        z: z,
        // 定义加法方法
        add: function(other) {
            return Vector3(
                this.x + other.x,
                this.y + other.y,
                this.z + other.z
            );
        },
        mul_scalar: function (s) {
            return Vector3(
                this.x * s,
                this.y * s,
                this.z * s
            );
        },
        sub: function (other) {
            return Vector3(
                this.x - other.x,
                this.y - other.y,
                this.z - other.z
            );
        },
        equals: function (other) {
            return this.x === other.x && this.y === other.y && this.x === other.z;
        }
    };
}