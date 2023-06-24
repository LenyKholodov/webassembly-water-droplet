#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelViewMatrix;
uniform vec3 worldViewPosition;
attribute vec4 vColor;
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;
varying vec4 position;
varying vec4 eyeDirection;
varying vec4 normal;
varying vec4 color;
varying vec2 texCoord;
varying vec3 testTexCoord;

varying vec3 refractionDir;
varying vec3 reflectionDir;
varying float fresnel;

const float eta = 0.0;
//const float eta = 0.75;
//const float eta = 0.05;
const float fresnelPower = 5.0;
//const float F = 0.05;
const float F = 0.05;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = modelMatrix * vec4(vPosition, 1.0);
  eyeDirection = vec4(worldViewPosition - position.xyz, 0.0);
  normal = modelMatrix * vec4 (vNormal, 0.0);
  color = vColor;
  texCoord = vTexCoord;
  testTexCoord = vPosition;

  vec3 inVec = -eyeDirection.xyz;
  vec3 N = normal.xyz;
  reflectionDir = reflect(inVec, N);
  refractionDir = refract(inVec, N, eta);
  fresnel = clamp(F + (1.0 - F) * pow(1.0 + dot(inVec, normalize(N)), fresnelPower), 0.0, 1.0);
}

#shader pixel

precision mediump float;
varying vec4 position;
varying vec4 eyeDirection;
varying vec4 normal;
varying vec4 color;
varying vec2 texCoord;
varying vec3 testTexCoord;

varying vec3 refractionDir;
varying vec3 reflectionDir;
varying float fresnel;

#define outColor gl_FragColor
#define texture texture2D

#define DEBUG 0

uniform sampler2D diffuseTexture;
uniform samplerCube environmentMap;

const float MIN_DIFFUSE_AMOUNT = 0.1; // ambient light
const float DIFFUSE_AMOUNT = 1.0; // diffuse light multiplier

#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 2

uniform vec3 worldViewPosition;

uniform vec3 spotLightPositions[MAX_SPOT_LIGHTS];
uniform vec3 spotLightDirections[MAX_SPOT_LIGHTS];
uniform vec3 spotLightColors[MAX_SPOT_LIGHTS];
uniform vec3 spotLightAttenuations[MAX_SPOT_LIGHTS];
uniform float spotLightRanges[MAX_SPOT_LIGHTS];
uniform float spotLightAngles[MAX_SPOT_LIGHTS];
uniform float spotLightExponents[MAX_SPOT_LIGHTS];
uniform mat4 spotLightShadowMatrices[MAX_SPOT_LIGHTS];

uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform vec3 pointLightAttenuations[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];

vec3 ComputeDiffuseColor(const in vec3 normal, const in vec3 lightDir, const in vec3 texDiffuseColor)
{
  return texDiffuseColor * max(dot(lightDir, normal), MIN_DIFFUSE_AMOUNT);
}

const float envFactor = 0.85;
//const float envFactor = 0.15;

void main()
{
  vec3 position = position.xyz;
  vec4 diffuseColor = texture(diffuseTexture, texCoord);

  vec3 reflectDir   = normalize(reflectionDir);
  vec3 reflectColor = textureCube(environmentMap, reflectDir).xyz;
  vec3 refractDir   = normalize(refractionDir);
  vec3 refractColor = textureCube(environmentMap, refractDir).xyz;

  vec3 resultColor  = mix(refractColor, reflectColor, fresnel);

  vec3 normal = normalize(normal.xyz);
  vec3 eyeDirection = normalize(worldViewPosition - position);
  
  vec3 color = vec3(0.0);

  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
  {  
    vec3 lightPosition = pointLightPositions[i];
    vec3 lightColor = pointLightColors[i];
    vec3 lightAttenuation = pointLightAttenuations[i];
    float lightRange = pointLightRanges[i];

    float distance = length(lightPosition - position);
    float attenuation = min(1.0, lightRange / (lightAttenuation.x + lightAttenuation.y * distance + lightAttenuation.z * (distance * distance))); 
    vec3 lightDirection = normalize(lightPosition - position);
    
    vec3 diffuseColor = ComputeDiffuseColor(normal, lightDirection, diffuseColor.xyz) * DIFFUSE_AMOUNT;

    color += lightColor * attenuation * diffuseColor;
  }

  for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
  {
    float lightAngle = spotLightAngles[i];    
    vec3 lightPosition = spotLightPositions[i];
    vec3 lightSelfDirection = -normalize(spotLightDirections[i].xyz);
    vec3 lightColor = spotLightColors[i];      
    vec3 lightAttenuation = spotLightAttenuations[i];
    float lightExponent = spotLightExponents[i];
    float lightRange = spotLightRanges[i];

    vec3 lightDirection = normalize(lightPosition - position);
    float theta = acos(dot(lightSelfDirection, lightDirection));

    if (theta < lightAngle)
    {
      float distance = length(lightPosition - position);
      float attenuation = min(1.0, lightRange / (lightAttenuation.x + lightAttenuation.y * distance + lightAttenuation.z * (distance * distance))); 

      attenuation *= pow(max(0.0, 1.0 - theta / lightAngle), lightExponent);

      vec3 diffuseColor = ComputeDiffuseColor(normal, lightDirection, diffuseColor.xyz) * DIFFUSE_AMOUNT;

      color += lightColor * attenuation * diffuseColor;
    }
  }
  
  outColor = vec4(mix(color, resultColor, envFactor), 1.0);
  //outColor = vec4(normalize(normal), 1.0);
}
