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

const float PLATFORM_Y   = -7.0;   // matches GROUND_OFFSET
const float DEPTH_SCALE  = 7.0;    // view-ray length through water at which it becomes fully reflective/opaque
const vec3  WATER_AMBIENT = vec3(0.02, 0.035, 0.06);
const float WATER_SHININESS = 120.0;       // tighter, more delicate glints
const float WATER_SPECULAR_AMOUNT = 0.35;  // soft light glow on the water (was too bright)
const float MIN_ALPHA = 0.22;      // most-transparent (shallow, face-on) alpha
const float SKY_REFLECT = 0.75;    // brightness of the reflected sky

float pointSpecular(vec3 Lp, vec3 Lcolor, vec3 La, float Lrange, vec3 N, vec3 V, out vec3 outSpec)
{
  float d   = length(Lp - worldPos);
  float att = min(1.0, Lrange / (La.x + La.y * d + La.z * d * d));
  vec3  H   = normalize(normalize(Lp - worldPos) + V);
  outSpec   = Lcolor * att * pow(max(dot(N, H), 0.0), WATER_SHININESS);
  return 0.0;
}

void main()
{
  vec3 N = normalize(worldNormal);
  vec3 V = normalize(worldViewPosition - worldPos);  // surface -> eye

  // Depth-based water: how far the view ray travels through water before hitting the platform plane.
  // Deeper / grazing -> more reflection, less transparent. Shallow / face-on -> see the bottom.
  float viewDepth    = (worldPos.y - PLATFORM_Y) / max(abs(V.y), 0.06);
  float depthFactor  = clamp(viewDepth / DEPTH_SCALE, 0.0, 1.0);
  float fres         = pow(1.0 - max(dot(N, V), 0.0), 5.0);
  float reflectivity = clamp(max(fres, depthFactor), 0.0, 1.0);

  // reflect the sky cubemap (no parallax -> correct on a large flat surface)
  vec3 R = reflect(-V, N);
  vec3 reflectColor = textureCube(skyTexture, R).xyz * SKY_REFLECT;

  // specular glints from the scene lights; ripples break them into moving highlights
  vec3 specular = vec3(0.0), s;
  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
  {
    pointSpecular(pointLightPositions[i], pointLightColors[i], pointLightAttenuations[i], pointLightRanges[i], N, V, s);
    specular += s;
  }
  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
  {
    pointSpecular(spotLightPositions[i], spotLightColors[i], spotLightAttenuations[i], spotLightRanges[i], N, V, s);
    specular += s;
  }

  vec3  surface = WATER_AMBIENT + reflectColor * reflectivity + specular * WATER_SPECULAR_AMOUNT;
  float alpha   = mix(MIN_ALPHA, 1.0, reflectivity);
  alpha = clamp(alpha + dot(specular, vec3(0.2)) * WATER_SPECULAR_AMOUNT, 0.0, 1.0); // glints read as slightly opaque

  gl_FragColor = vec4(surface, alpha);
}
