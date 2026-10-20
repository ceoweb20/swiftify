#version 440
// a scrolling row that fades out at its left and right edges.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // how far in from each edge the fade reaches, as a fraction of the width
    float leftEdge;
    float rightEdge;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    float x = qt_TexCoord0.x;
    float leftFade = leftEdge > 0.001 ? clamp(x / leftEdge, 0.0, 1.0) : 1.0;
    float rightFade = rightEdge > 0.001 ? clamp((1.0 - x) / rightEdge, 0.0, 1.0) : 1.0;
    fragColor = texture(source, qt_TexCoord0) * (leftFade * rightFade * qt_Opacity);
}
