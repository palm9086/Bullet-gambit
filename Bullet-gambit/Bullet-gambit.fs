#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D paletteTex; // optional palette overlay
uniform bool usePalette;
uniform bool flipPaletteHoriz;
uniform float alphaCutoff; // discard fragments below this

void main()
{
    vec2 uv = TexCoords;
    if (flipPaletteHoriz) uv.x =1.0 - uv.x;

    vec4 tex = texture(texture_diffuse1, TexCoords);

    if (tex.a < alphaCutoff)
        discard;

    if (usePalette) {
        vec4 pal = texture(paletteTex, uv);
        // Combine: multiply base diffuse by palette RGB, preserve alpha from base texture
        FragColor = vec4(tex.rgb * pal.rgb, tex.a);
    } else {
        FragColor = tex;
    }
}