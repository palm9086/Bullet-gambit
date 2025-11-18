#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

uniform vec2 offset; // Position (bottom-left corner of the button)
uniform vec2 scale;  // Width and Height of the button

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    
    // 1. Transform input quad coordinates from [-1, 1] to [0, 1] (normalized screen space)
    vec2 pos = (aPos.xy * 0.5) + 0.5;
    
    // 2. Apply scale and offset (scale the quad, then move its bottom-left to 'offset')
    pos = pos * scale + offset;
    
    // 3. Transform back to clip space [-1, 1] for OpenGL drawing
    pos = pos * 2.0 - 1.0;

    gl_Position = vec4(pos, aPos.z, 1.0);
}