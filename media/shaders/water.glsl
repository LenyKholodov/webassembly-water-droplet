#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;
uniform vec3 worldViewPosition;

attribute vec3 vPosition;
attribute vec3 vNormal;

varying vec3 worldPos;
varying vec3 worldNormal;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  worldPos    = (modelMatrix * vec4(vPosition, 1.0)).xyz;
  worldNormal = (modelMatrix * vec4(vNormal, 0.0)).xyz; // the water node has uniform scale, so this is correct
}

#shader pixel
precision mediump float;

varying vec3 worldPos;
varying vec3 worldNormal;

uniform vec3 worldViewPosition;
uniform samplerCube skyTexture;     // the static sky cubemap (infinitely far -> correct flat-surface reflection)

#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 2
uniform vec3  pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3  pointLightColors[MAX_POINT_LIGHTS];
uniform vec3  pointLightAttenuations[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];
uniform vec3  spotLightPositions[MAX_SPOT_LIGHTS];
uniform vec3  spotLightColors[MAX_SPOT_LIGHTS];
uniform vec3  spotLightAttenuations[MAX_SPOT_LIGHTS];
uniform float spotLightRanges[MAX_SPOT_LIGHTS];

// Flat mirror: the sky is infinitely far, so reflecting it through the cubemap IS the correct
// planar reflection. Refraction = the actual bottom (the platform), which shows through the
// alpha-blended transparency below the water; fresnel blends reflection (grazing) vs the bottom (face-on).
const float F = 0.03;              // base reflectance (Schlick F0)
const float fresnelPower = 5.0;
const float REFLECT_FLOOR = 0.45;  // min opacity face-on -> reads as a mirror, with the bottom still visible
const vec3  WATER_TINT = vec3(0.012, 0.025, 0.045); // faint deep-water colour over the transmitted bottom
const float WATER_SHININESS = 140.0;       // tight, delicate glints
const float WATER_SPECULAR_AMOUNT = 0.30;
const float SKY_REFLECT = 1.0;

void pointSpecular(vec3 Lp, vec3 Lcolor, vec3 La, float Lrange, vec3 N, vec3 V, inout vec3 spec)
{
  float d   = length(Lp - worldPos);
  float att = min(1.0, Lrange / (La.x + La.y * d + La.z * d * d));
  vec3  H   = normalize(normalize(Lp - worldPos) + V);
  spec += Lcolor * att * pow(max(dot(N, H), 0.0), WATER_SHININESS);
}

void main()
{
  vec3 N = normalize(worldNormal);
  vec3 V = normalize(worldViewPosition - worldPos);  // surface -> eye

  // Schlick fresnel: grazing (cos~0) -> 1 (full mirror), face-on (cos~1) -> F (see the bottom)
  float cosTheta = max(dot(N, V), 0.0);
  float fresnel  = F + (1.0 - F) * pow(1.0 - cosTheta, fresnelPower);

  // flat-mirror reflection of the sky; the ripple-perturbed normal makes the reflection shimmer
  vec3 reflectColor = textureCube(skyTexture, reflect(-V, N)).xyz * SKY_REFLECT;

  // specular glints from the scene lights (moonlight + fireflies)
  vec3 specular = vec3(0.0);
  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    pointSpecular(pointLightPositions[i], pointLightColors[i], pointLightAttenuations[i], pointLightRanges[i], N, V, specular);
  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
    pointSpecular(spotLightPositions[i], spotLightColors[i], spotLightAttenuations[i], spotLightRanges[i], N, V, specular);

  // surface colour = the mirrored sky + glints + faint tint; the bottom shows via the alpha below.
  vec3 surface = reflectColor + WATER_TINT + specular * WATER_SPECULAR_AMOUNT;

  // opacity: full mirror at grazing, partly transparent (the bottom = refraction) when looking down
  float alpha = mix(REFLECT_FLOOR, 1.0, fresnel);
  alpha = clamp(alpha + dot(specular, vec3(0.3)) * WATER_SPECULAR_AMOUNT, 0.0, 1.0);

  gl_FragColor = vec4(surface, alpha);
}
