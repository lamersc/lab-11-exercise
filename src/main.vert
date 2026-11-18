#version 120

uniform sampler1D transforms;
uniform int numTransforms;

attribute vec2 position;
attribute vec2 texCoords;
attribute float spriteNumber;

varying vec2 texCoordsOut;

// Given a world position (for this lab, bottom-left is (0, 0), top-right of window is (1280, 720)),
// convert it to be in [-1, 1]^2, where y = -1 is the bottom and x = -1 is the left side.
vec2 cameraPos(vec2 worldPos) {
    return 2.0 * worldPos / vec2(1280.0, 720.0) - 1.0;
}

void main() {
    // set gl_Position and texCoordsOut...
    vec4 transform = texture1D(transforms, (spriteNumber + 0.5) / float(numTransforms));
    vec2 worldPos = position * transform.xy + transform.zw;
    gl_Position = vec4(cameraPos(worldPos), 0.0, 1.0);
    texCoordsOut = vec2(texCoords.x, 1.0 - texCoords.y);
}
