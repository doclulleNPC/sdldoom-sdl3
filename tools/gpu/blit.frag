#version 450
layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 oColor;
// SDL_GPU SPIR-V binding model: fragment sampled textures live in set 2.
layout(set = 2, binding = 0) uniform sampler2D uTex;
void main()
{
    oColor = texture(uTex, vUV);
}
