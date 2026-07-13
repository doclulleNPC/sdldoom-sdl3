#version 450
// Fullscreen triangle (no vertex buffer). DOOM's framebuffer has row 0 at the
// top; SDL_GPU's clip space puts +Y up, so negate Y to keep the image upright.
layout(location = 0) out vec2 vUV;
void main()
{
    vec2 uv = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vUV = uv;
    gl_Position = vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
}
