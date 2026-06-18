#shader vertex
precision highp float;

// Proxy geometry is a unit cube positioned at the droplet centre and scaled to enclose the metaball.
// We pass the world-space position (to reconstruct the view ray) and the clip-space position (for
// screen-space sampling of the scene behind the droplet). The surface itself comes from the SDF raymarch.

uniform mat4 MVP;
uniform mat4 modelMatrix;

attribute vec3 vPosition;

varying vec3 worldPos;
varying vec4 clipPos;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  clipPos     = gl_Position;
  worldPos    = (modelMatrix * vec4(vPosition, 1.0)).xyz;
}

#shader pixel
precision highp float;

// Metaball droplet: a sum-of-spheres SDF, sphere-traced inside the proxy box. The hit point's analytic
// gradient is the surface normal. Shading:
//   - REFRACTION: screen-space — sample the real scene behind the droplet (the leaf), rendered without
//     droplets by the water pass into refractionTexture, warped by the surface tilt. This makes the
//     droplet read as clear water showing the magnified/distorted leaf, instead of the flat opaque blob
//     the per-droplet cubemap gave (the cubemap, shot from the droplet centre on a leaf, is just flat green).
//   - REFLECTION: the per-droplet environment cubemap (sky/surroundings on the grazing edges).

varying vec3 worldPos;
varying vec4 clipPos;

#define MAX_DROPLET_PARTICLES 64   // must match MAX_DROPLET_RAYMARCH_PARTICLES in world.cpp
#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 2

uniform vec3       worldViewPosition;
uniform mat4       viewMatrix;        // world -> view: expresses the surface tilt in screen space
uniform samplerCube environmentMap;   // per-droplet cubemap (reflection)
uniform sampler2D  refractionTexture; // scene minus droplets (the leaf behind), from the water pass

// per-droplet (set via the mesh node's PropertyMap user-data, see world.cpp)
uniform vec4  particles[MAX_DROPLET_PARTICLES]; // .xyz world-space centre, .w radius
uniform int   particleCount;
uniform vec3  dropletCenter;
uniform float influenceRadius;        // smooth-min blend k (blobbiness / merge)
uniform float isoThreshold;           // surface iso level (inflate / thin)
uniform float boxHalfExtent;          // world half-size of the proxy box (for ray clipping)

// lights: same names + model as fresnel.glsl, populated by the forward pass frame properties
uniform vec3  pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3  pointLightColors[MAX_POINT_LIGHTS];
uniform vec3  pointLightAttenuations[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];

uniform vec3  spotLightPositions[MAX_SPOT_LIGHTS];
uniform vec3  spotLightDirections[MAX_SPOT_LIGHTS];
uniform vec3  spotLightColors[MAX_SPOT_LIGHTS];
uniform vec3  spotLightAttenuations[MAX_SPOT_LIGHTS];
uniform float spotLightRanges[MAX_SPOT_LIGHTS];
uniform float spotLightAngles[MAX_SPOT_LIGHTS];
uniform float spotLightExponents[MAX_SPOT_LIGHTS];

// tuning
const float fresnelPower            = 4.0;
const float F                       = 0.04;  // base reflectance (water ~0.02-0.05)
const float REFR_STRENGTH           = 0.045; // screen-space lens warp of the scene behind (tuning)
const vec3  REFR_TINT               = vec3(0.92, 1.0, 0.95); // faint cool tint of the transmitted light
const float envFactor               = 0.92;  // optical (refraction/reflection) vs direct diffuse
const float MIN_DIFFUSE             = 0.08;
const float DIFFUSE_AMOUNT          = 0.5;
const vec3  DROPLET_COLOR           = vec3(1.0);
const float DROPLET_SHININESS       = 200.0; // tight, crisp glint -> reads as a wet bead
const float DROPLET_SPECULAR_AMOUNT = 0.8;

const int   MAX_STEPS  = 64;
const float SURF_EPS   = 0.0015;
const float NORMAL_EPS = 0.0015;

// IQ polynomial smooth-min: blends the sphere fields so neighbours neck/merge instead of intersecting
float smin(float a, float b, float k)
{
  if (k <= 0.0)
    return min(a, b);

  float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) - k * h * (1.0 - h);
}

float map(vec3 p)
{
  float d = 1.0e9;

  for (int i = 0; i < MAX_DROPLET_PARTICLES; ++i)
  {
    if (i >= particleCount)
      break;

    float sphere = length(p - particles[i].xyz) - particles[i].w;
    d = smin(d, sphere, influenceRadius);
  }

  return d - isoThreshold;
}

vec3 calcNormal(vec3 p)
{
  vec2 e = vec2(NORMAL_EPS, 0.0);
  return normalize(vec3(
    map(p + e.xyy) - map(p - e.xyy),
    map(p + e.yxy) - map(p - e.yxy),
    map(p + e.yyx) - map(p - e.yyx)));
}

// ray vs the (axis-aligned, unrotated) proxy box -> [entry, exit] distances
vec2 boxInterval(vec3 ro, vec3 rd)
{
  vec3 invD = 1.0 / rd;
  vec3 ta   = (dropletCenter - vec3(boxHalfExtent) - ro) * invD;
  vec3 tb   = (dropletCenter + vec3(boxHalfExtent) - ro) * invD;
  vec3 tmin = min(ta, tb);
  vec3 tmax = max(ta, tb);
  return vec2(max(max(tmin.x, tmin.y), tmin.z),
              min(min(tmax.x, tmax.y), tmax.z));
}

vec3 shade(vec3 p, vec3 n, vec2 screenUV)
{
  vec3 I = normalize(p - worldViewPosition);   // incident view direction
  vec3 V = -I;                                 // surface -> eye

  // --- reflection: per-droplet cubemap ---
  vec3 reflectCol = textureCube(environmentMap, reflect(I, n)).xyz;

  // --- refraction: the real scene behind the droplet, warped by the surface tilt (screen space) ---
  // the view-space normal xy is the surface slope on screen; offsetting the lookup by it bends the
  // background like a lens (centre ~undistorted, edges warp), showing the magnified leaf through the drop.
  vec2 viewN     = (viewMatrix * vec4(n, 0.0)).xy;
  vec2 uv        = clamp(screenUV - viewN * REFR_STRENGTH, 0.0, 1.0);
  vec3 refractCol = texture2D(refractionTexture, uv).xyz * REFR_TINT;

  float fresnel  = clamp(F + (1.0 - F) * pow(1.0 + dot(I, n), fresnelPower), 0.0, 1.0);
  vec3  envColor = mix(refractCol, reflectCol, fresnel);

  // --- direct diffuse + specular from scene lights (specular as in water.glsl) ---
  vec3 lit  = vec3(0.0);
  vec3 spec = vec3(0.0);

  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
  {
    vec3  La   = pointLightAttenuations[i];
    float dist = length(pointLightPositions[i] - p);
    float att  = min(1.0, pointLightRanges[i] / (La.x + La.y * dist + La.z * dist * dist));
    vec3  Ld   = normalize(pointLightPositions[i] - p);
    lit  += pointLightColors[i] * att * DROPLET_COLOR * max(dot(Ld, n), MIN_DIFFUSE) * DIFFUSE_AMOUNT;
    vec3  H    = normalize(Ld + V);
    spec += pointLightColors[i] * att * pow(max(dot(n, H), 0.0), DROPLET_SHININESS);
  }

  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
  {
    float ang   = spotLightAngles[i];
    vec3  Lself = -normalize(spotLightDirections[i]);
    vec3  Ld    = normalize(spotLightPositions[i] - p);
    float theta = acos(dot(Lself, Ld));

    if (theta < ang)
    {
      vec3  La   = spotLightAttenuations[i];
      float dist = length(spotLightPositions[i] - p);
      float att  = min(1.0, spotLightRanges[i] / (La.x + La.y * dist + La.z * dist * dist));
      att *= pow(max(0.0, 1.0 - theta / ang), spotLightExponents[i]);
      lit  += spotLightColors[i] * att * DROPLET_COLOR * max(dot(Ld, n), MIN_DIFFUSE) * DIFFUSE_AMOUNT;
      vec3 H = normalize(Ld + V);
      spec += spotLightColors[i] * att * pow(max(dot(n, H), 0.0), DROPLET_SHININESS);
    }
  }

  return mix(lit, envColor, envFactor) + spec * DROPLET_SPECULAR_AMOUNT;
}

void main()
{
  vec3 ro = worldViewPosition;
  vec3 rd = normalize(worldPos - ro);

  vec2  iv = boxInterval(ro, rd);
  float t  = max(iv.x, 0.0);
  float tf = iv.y;

  if (t > tf)
    discard;

  bool hit = false;

  for (int i = 0; i < MAX_STEPS; ++i)
  {
    float d = map(ro + rd * t);

    if (d < SURF_EPS) { hit = true; break; }

    t += d;

    if (t > tf)
      break;
  }

  if (!hit)
    discard;

  vec3 p = ro + rd * t;
  vec3 n = calcNormal(p);

  // the hit point lies on the ray through this pixel, so its screen position IS this fragment's
  vec2 screenUV = clipPos.xy / clipPos.w * 0.5 + 0.5;

  gl_FragColor = vec4(shade(p, n, screenUV), 1.0);
}
