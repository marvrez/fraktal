#version 150

in vec3 ray_origin;
in vec3 ray_exit;
in vec2 texel;
uniform float time;
uniform sampler2D tex_sky;
out vec4 out_color;

#define EPSILON 0.001
#define STEPS 256
#define Z_NEAR 0.1
#define Z_FAR 100.0
#define PI_HALF 1.57079632679
#define PI 3.14159265359
#define TWO_PI 6.28318530718

vec3 SampleSky(vec3 dir)
{
    float u = atan(dir.x, dir.z);
    float v = asin(dir.y);
    u /= TWO_PI;
    v /= PI;
    v *= -1.0;
    v += 0.5;
    u += 0.5;
    return texture(tex_sky, vec2(u, v)).rgb;
}

// http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
float udRoundBox(vec3 p, vec3 b, float r)
{
    return length(max(abs(p)-b,0.0))-r;
}

float sdSphere(vec3 p, float r)
{
    return length(p) - r;
}

float opIntersect(float d1, float d2)
{
    return max(d1, d2);
}

float opSubtract(float d1, float d2)
{
    return max(d1, -d2);
}

float sdBox( vec3 p, vec3 b )
{
    vec3 d = abs(p) - b;
    return min(max(d.x,max(d.y,d.z)),0.0) +
         length(max(d,0.0));
}

vec3 RotateY(vec3 v, float t)
{
    float cost = cos(t); float sint = sin(t);
    return vec3(v.x * cost + v.z * sint, v.y, -v.x * sint + v.z * cost);
}

vec3 RotateX(vec3 v, float t)
{
    float cost = cos(t); float sint = sin(t);
    return vec3(v.x, v.y * cost - v.z * sint, v.y * sint + v.z * cost);
}

float Mystery(vec3 p)
{
    // float db = sdSphere(p, 1.5);
    // p.xyz = mod(p.xyz, vec3(0.5)) - vec3(0.25);
    // float d1 = opSubtract(db, sdBox(p, vec3(0.2)));
    // return d1;

    p = RotateY(p, 0.01 * time);
    float d1 = sdSphere(p, 0.8);
    float d2 = sdBox(p, vec3(1.0, 0.4, 0.4));
    float d3 = sdBox(p, vec3(0.1, 0.1, 2.0));
    return min(d3, opSubtract(d1, d2));
}

float Scene(vec3 p)
{
    vec3 q = p;
    vec2 modrad = vec2(1.0);
    q.xz = mod(p.xz + modrad, 2.0 * modrad) - modrad;
    float b = sdSphere(p - vec3(0.0, -1000.0, 0.0), 999.5);

    vec3 size = vec3(0.1, 0.4, 0.1);

    q = RotateY(q, 0.2 * time);
    float c = udRoundBox(q, size, 0.05);
    float d = Mystery(p - vec3(0.0, 0.5, 0.0));
    return min(b, min(c, d));
}

vec3 Normal(vec3 p)
{
    vec2 e = vec2(EPSILON, 0.0);
    return normalize(vec3(
                     Scene(p + e.xyy) - Scene(p - e.xyy),
                     Scene(p + e.yxy) - Scene(p - e.yxy),
                     Scene(p + e.yyx) - Scene(p - e.yyx))
    );
}

void 
Trace(vec3 origin, vec3 dir, 
      out vec3 hit_point, out float travel, out float nearest)
{
    hit_point = origin;
    travel = 0.0;
    nearest = Z_FAR;
    for (int i = 0; i < STEPS; i++)
    {
        float s = Scene(hit_point);
        travel += s;
        hit_point += s * dir;
        nearest = min(nearest, s);
        if (s < EPSILON)
            break;
        if (travel > Z_FAR)
            break;
    }
}

vec2 seed = (texel) * (time + 1.0);

vec2 Noise2f() {
    seed += vec2(-1, 1);
    // implementation based on: lumina.sourceforge.net/Tutorials/Noise.html
    return vec2(fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453),
        fract(cos(dot(seed.xy, vec2(4.898, 7.23))) * 23421.631));
}

// See http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts
vec3 Ortho(vec3 v)
{
    return abs(v.x) > abs(v.z) ? vec3(-v.y, v.x, 0.0)
                               : vec3(0.0, -v.z, v.y);
}

vec3 CosineWeightedSample(vec3 normal)
{
    vec3 tangent = normalize(Ortho(normal));
    vec3 bitangent = normalize(cross(normal, tangent));
    vec2 r = Noise2f();
    r.x *= TWO_PI;
    r.y = pow(r.y, 1.0 / 2.0);
    float oneminus = sqrt(1.0 - r.y * r.y);
    return cos(r.x) * oneminus * tangent + 
           sin(r.x) * oneminus * bitangent + 
           r.y * normal;
}

vec3 ConeSample(vec3 dir, float extent)
{
    vec3 tangent = normalize(Ortho(dir));
    vec3 bitangent = normalize(cross(dir, tangent));
    vec2 r = Noise2f();
    r.x *= TWO_PI;
    r.y *= 1.0 - r.y * extent;
    float oneminus = sqrt(1.0 - r.y * r.y);
    return cos(r.x) * oneminus * tangent + 
           sin(r.x) * oneminus * bitangent + 
           r.y * dir;
}

vec3 ComputeLight(vec3 hit, vec3 from)
{
    vec3 normal = Normal(hit);
    vec3 dir = CosineWeightedSample(normal);
    vec3 origin = hit + normal * 2.0 * EPSILON;

    float travel, nearest;
    Trace(origin, dir, hit, travel, nearest);

    vec3 luminance = vec3(1.0);
    if (nearest > EPSILON)
        return SampleSky(dir);

    return vec3(0.0);
}

void main()
{
    vec3 dir = normalize(ray_exit - ray_origin);
    vec3 origin = ray_origin;
    vec3 hit_point;
    float travel;
    float nearest;
    Trace(origin, dir, hit_point, travel, nearest);

    if (nearest < EPSILON)
    {
        out_color.rgb = ComputeLight(hit_point, dir);
    }
    else
    {
        out_color.rgb = SampleSky(dir);
        // out_color.rgb = vec3(1.0);
    }
    out_color.a = 1.0;
}
