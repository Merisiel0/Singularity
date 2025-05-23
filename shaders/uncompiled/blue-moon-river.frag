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
    Golfed from 399 to 377 by
    
        @FabriceNeyret2     -14
        @AleDev             -8
        
        
        Thanks! :D
*/

#define A(x,r) abs(dot(sin(x*p*a), r +p-p )) / a

void main() 
{
    vec2 u = F;

    float i=0,r,d,s,a,t=iTime*5e1;
    vec3  p = iResolution; 
    
    u = (u-p.xy/2.)/p.y;
    
    for (o*=i ;i<1e2; i++){
        d += s = .02 + abs(min(s,r))*.2;
        o += vec4(1,2,4,0) / s;
         
        for(p = vec3(u * d, d + t),
            a = .01,
            r = 15. + p.y,
            s = r + 7. - sin(p.z*.01) * 32. -
            abs(p.x += sin(p.z*.03)*8.)*.72;

            a < .1;

            r += A(.06 * p.z + .04 * t + 12., .015),
            s += A(1.,.5),

            a *= 1.4);
    }

    o = tanh(o / 2e3 / dot(u-=.45,u));
}




/*

original:

void mainImage(out vec4 o, vec2 u) {
    float i,r,d,s,t=iTime*5e1;
    vec3  p = iResolution;    
    u = (u-p.xy/2.)/p.y;
    for(o*=i; i++<1e2; ) {
        p = vec3(u * d,d+t);
        p.x += sin(p.z*.03)*8.;
        s = 22. + p.y - sin(p.z*.01) * 32. - abs(p.x*.08)*9.;
        r = 15. + p.y;
        for (float a = .01; a < .1;
            r += abs(dot(sin(.06*p.z+.04*t+p * 12. * a ), vec3(.015))) / a,
            s += abs(dot(sin(p * 1. * a ), vec3(.5))) / a,
            a *= 1.4);
        d += s = .02 + abs(min(s,r))*.2;
        o += vec4(1,2,4,0) / s;
    }
    o = tanh(o / 2e3 / dot(u-=.45,u));
}

*/