#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;

attribute vec3 vPosition;
attribute vec3 vNormal;

varying vec3 worldPos;
varying vec3 worldNormal;
varying vec4 clipPos;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  clipPos     = gl_Position;                               // for screen-space sampling
  worldPos    = (modelMatrix * vec4(vPosition, 1.0)).xyz;
  worldNormal = (modelMatrix * vec4(vNormal, 0.0)).xyz;    // uniform node scale -> correct
}

#shader pixel
precision mediump float;

varying vec3 worldPos;
varying vec3 worldNormal;
varying vec4 clipPos;

uniform vec3 worldViewPosition;
uniform mat4 viewMatrix;              // world -> view, used to express the ripple distortion in screen space
uniform sampler2D reflectionTexture;  // scene rendered from the mirrored camera
uniform sampler2D refractionTexture;  // scene below the water from the main camera

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

const float F = 0.25;                  // base reflectance floor: keeps the tree/sky reflection visible even face-on (artistic, > physical 0.02)
const float fresnelPower = 4.0;        // ramps reflection up toward grazing angles
const float DISTORT = 0.035;           // ripple-driven screen-space distortion of both reflection and refraction
const float WATER_SHININESS = 140.0;
const float WATER_SPECULAR_AMOUNT = 0.30;
const vec3  WATER_TINT = vec3(0.6, 0.78, 0.9); // cool tint multiplied into the refracted (underwater) colour

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
  vec3 V = normalize(worldViewPosition - worldPos);

  // screen-space UV of this fragment
  vec2 screenUV = clipPos.xy / clipPos.w * 0.5 + 0.5;

  // ripples tilt the normal away from flat (world up). Express that deviation in VIEW space so its
  // x/y line up with screen right/up -> the reflection/refraction distorts the way the waves actually
  // look on screen, regardless of camera angle. (Using world N.xz here distorted in the wrong direction.)
  vec3 deviation = N - vec3(0.0, 1.0, 0.0);
  vec2 distort   = (viewMatrix * vec4(deviation, 0.0)).xy * DISTORT;

  vec3 reflectColor = texture2D(reflectionTexture, clamp(screenUV + distort, 0.0, 1.0)).rgb;
  vec3 refractColor = texture2D(refractionTexture, clamp(screenUV + distort, 0.0, 1.0)).rgb * WATER_TINT;

  // Schlick fresnel: face-on -> refraction (see the bottom), grazing -> reflection (the tree/sky)
  float cosTheta = max(dot(N, V), 0.0);
  float fresnel  = F + (1.0 - F) * pow(1.0 - cosTheta, fresnelPower);

  // specular glints from the scene lights (moonlight + fireflies)
  vec3 specular = vec3(0.0);
  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    pointSpecular(pointLightPositions[i], pointLightColors[i], pointLightAttenuations[i], pointLightRanges[i], N, V, specular);
  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
    pointSpecular(spotLightPositions[i], spotLightColors[i], spotLightAttenuations[i], spotLightRanges[i], N, V, specular);

  vec3 surface = mix(refractColor, reflectColor, fresnel) + specular * WATER_SPECULAR_AMOUNT;

  gl_FragColor = vec4(surface, 1.0); // opaque: the bottom comes from the refraction texture, not transparency
}
