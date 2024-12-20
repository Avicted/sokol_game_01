/* quad vertex shader */
@vs vs
in vec4 position;
in vec2 texcoord0;
in vec4 color0;

out vec4 color;
out vec2 uv;

void main() {
    gl_Position = position;
    color = color0;
    uv = texcoord0;
}
@end

/* quad fragment shader */
@fs fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec4 color;
in vec2 uv;

out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv) * color;
}
@end

/* quad shader program */
@program quad vs fs
