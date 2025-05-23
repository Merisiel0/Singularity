#version 450

layout(location = 0) in vec2 fragCoord;

layout (location = 0) out vec4 fragColor;

layout( push_constant ) uniform PushConstants {
  vec3 iResolution;           // viewport resolution (in pixels)
  float iTime;                // shader playback time (in seconds)
  float iTimeDelta;           // render time (in seconds)
  float iFrameRate;           // shader frame rate
  int iFrame;                 // shader playback frame
  vec4 iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
} pushConstants;

#define iResolution pushConstants.iResolution
#define iTime       pushConstants.iTime
#define iTimeDelta  pushConstants.iTimeDelta
#define iFrameRate  pushConstants.iFrameRate
#define iFrame      pushConstants.iFrame
#define iMouse      pushConstants.iMouse

void main() {
  fragColor = vec4(0, 0.5, 0.5, 1.0);
}

