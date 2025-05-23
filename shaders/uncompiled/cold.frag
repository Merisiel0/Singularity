#version 450

layout(location = 0) in vec2 F;

layout (location = 0) out vec4 o;

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

/*
    -4 chars from iapafoto 333->329 (p-p+ instead of vec3())
*/

void main() {
    vec2 u = F;
    float i,d,s,t=iTime;
    vec3  q,p = iResolution;
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<1e2;
        d += s = .01 + min(.03+abs(9.- q.y)*.1,
                           .01+abs( 1.+ p.y)*.2),
        o += 1. /  s)
        for (q = p = vec3(u*d,d+t),s = .01; s < 2.;
             p += abs(dot(sin(p * s *24.), p-p+.01)) / s,
             q += abs(dot(sin(.3*t+q * s * 16.), p-p+.005)) / s,
             s += s);
    o = tanh(o * vec4(1,2,4,0) / 1e4 / length(u-.2));
}

/*

    Playing with turbulence and translucency from
    @Xor's recent shaders, e.g.
        https://www.shadertoy.com/view/wXjSRt
        https://www.shadertoy.com/view/wXSXzV
*/