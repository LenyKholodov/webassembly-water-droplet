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

// Same reflection/refraction settings as the droplet (fresnel.glsl):
const float eta = 0.0;             // refract() with eta 0 returns -N -> the droplet's lens trick
const float fresnelPower = 5.0;
const float F = 0.05;              // base reflectance

const vec3  WATER_AMBIENT = vec3(0.02, 0.035, 0.06);
const float WATER_SHININESS = 120.0;       // tighter, more delicate glints
const float WATER_SPECULAR_AMOUNT = 0.35;  // soft light glow on the water
const float MIN_ALPHA = 0.22;      // most-transparent (face-on) alpha
const float SKY_REFLECT = 0.85;    // brightness of the reflected/refracted sky

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
  vec3 inVec = -V;                                    // eye -> surface

  // reflection + refraction of the sky cubemap, mixed by fresnel (the droplet's settings)
  vec3 reflectDir   = reflect(inVec, N);
  vec3 refractDir   = refract(inVec, N, eta);         // eta 0 -> -N
  vec3 reflectColor = textureCube(skyTexture, reflectDir).xyz;
  vec3 refractColor = textureCube(skyTexture, refractDir).xyz;
  float fresnel     = clamp(F + (1.0 - F) * pow(1.0 + dot(inVec, N), fresnelPower), 0.0, 1.0);
  vec3 resultColor  = mix(refractColor, reflectColor, fresnel) * SKY_REFLECT;

  // specular glints from the scene lights; ripples break them into moving highlights
  vec3 specular = vec3(0.0);
  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    pointSpecular(pointLightPositions[i], pointLightColors[i], pointLightAttenuations[i], pointLightRanges[i], N, V, specular);
  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
    pointSpecular(spotLightPositions[i], spotLightColors[i], spotLightAttenuations[i], spotLightRanges[i], N, V, specular);

  vec3  surface = WATER_AMBIENT + resultColor + specular * WATER_SPECULAR_AMOUNT;
  // transparency from fresnel (no depth factor): face-on transparent, grazing reflective
  float alpha   = mix(MIN_ALPHA, 1.0, fresnel);
  alpha = clamp(alpha + dot(specular, vec3(0.2)) * WATER_SPECULAR_AMOUNT, 0.0, 1.0); // glints read as slightly opaque

  gl_FragColor = vec4(surface, alpha);
}
