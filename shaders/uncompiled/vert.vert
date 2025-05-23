#version 450

layout(location = 0) out vec2 F;

layout( push_constant ) uniform PushConstants {
  vec3 iResolution;           // viewport resolution (in pixels)
} pushConstants;

vec2 positions[6] = vec2[](
    vec2(-1, -1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(1, -1),
    vec2(1, 1),
    vec2(-1, 1)
);

vec2 UVs[6] = vec2[](
  vec2(0, 1),
  vec2(1, 1),
  vec2(0, 0),
  vec2(1, 1),
  vec2(1, 0),
  vec2(0, 0)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

  vec2 uv = UVs[gl_VertexIndex];
  F = uv * pushConstants.iResolution.xy;
}