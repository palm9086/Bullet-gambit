#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;

uniform vec2 offset; // Position (bottom-left corner of the button)
uniform vec2 scale;	 // Width and Height of the button
uniform float vStart; // New: normalized V start coordinate (0.0 to 1.0)
uniform float vEnd;   // New: normalized V end coordinate (0.0 to 1.0)

out vec2 TexCoords;

void main()
{
	// Map the input V coordinate (0.0 to 1.0) to the specified range [vStart, vEnd]
	float mappedV = aTexCoords.y * (vEnd - vStart) + vStart;
	
	TexCoords = vec2(aTexCoords.x, mappedV); // Only modify V coord

	// 1. Transform input quad coordinates from [-1, 1] to [0, 1] (normalized screen space)
	vec2 pos = (aPos.xy * 0.5) + 0.5;
	
	// 2. Apply scale and offset (scale the quad, then move its bottom-left to 'offset')
	pos = pos * scale + offset;
	
	// 3. Transform back to clip space [-1, 1] for OpenGL drawing
	pos = pos * 2.0 - 1.0;

	gl_Position = vec4(pos, aPos.z, 1.0);
}