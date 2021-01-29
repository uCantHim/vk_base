#version 460

layout (location = 0) in vec2 vertexPosition;
layout (location = 1) in vec2 vertexUvCoord;

layout (location = 2) in vec2 basePos;

layout (location = 3) in vec2 glyphPos;
layout (location = 4) in vec2 glyphSize;
layout (location = 5) in vec2 texCoordLL;
layout (location = 6) in vec2 texCoordUR;
layout (location = 7) in float bearingY;

layout (location = 8) in uint fontIndex;
layout (location = 9) in vec4 glyphColor;

layout (push_constant) uniform PushConstants {
    vec2 resolution;
};

layout (location = 0) out Vertex {
    vec2 uv;
    flat uint fontIndex;
} vert;

void main()
{
    const vec2 sizeNorm = glyphSize / resolution;
    const vec2 posNorm = glyphPos / resolution;

    vec2 pos = sizeNorm * vertexPosition + posNorm - vec2(0, bearingY / resolution.y);
    pos += basePos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);

    vert.uv = texCoordLL + vertexUvCoord * (texCoordUR - texCoordLL);
    vert.fontIndex = fontIndex;
}
