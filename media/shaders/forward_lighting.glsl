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

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = modelMatrix * vec4(vPosition, 1.0);
  eyeDirection = vec4(worldViewPosition - position.xyz, 0.0);
  normal = modelMatrix * vec4 (vNormal, 0.0);
  color = vColor;
  texCoord = vTexCoord;
}

#shader pixel

precision mediump float;
varying vec4 position;
varying vec4 eyeDirection;
varying vec4 normal;
varying vec4 color;
varying vec2 texCoord;

#define outColor gl_FragColor
#define texture texture2D

#define DEBUG 0

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D specularTexture;
uniform sampler2D shadowTexture;

const float MIN_DIFFUSE_AMOUNT = 0.04; // low ambient -> darker, more mysterious night shadows
const float DIFFUSE_AMOUNT = 1.0; // diffuse light multiplier
const float SPECULAR_AMOUNT = 0.06; // leaves are matte, not metallic -> almost no specular
const float NORMAL_RELIEF = 2.2; // amplify the normal-map bumps for more visible surface relief
const float SHININESS_NORMALIZER = 1000.0; // workaround for RGBA8 precision for shininess

#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 2

uniform vec3 worldViewPosition;
uniform vec2 shadowMapPixelSize;

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

vec3 ComputeSpecularColor(const in vec3 normal, const in vec3 lightDir, const in vec3 eyeDir, const in vec3 texSpecularColor, const in float shininess)
{
  float specularFactor = pow(clamp(dot(reflect(-lightDir, normal), eyeDir), 0.00001, 1.0), shininess * SHININESS_NORMALIZER);

  return texSpecularColor * specularFactor;
}

float OffsetLookup(vec4 shadowTexCoord, vec2 offset)
{
  float shadowDepth = texture(shadowTexture, shadowTexCoord.xy + offset * shadowMapPixelSize * shadowTexCoord.w).x + 0.0001;

  if (shadowDepth < shadowTexCoord.z)
  {
    return shadowTexCoord.z - shadowDepth;
  }
 
  return 1.0;
}

float PCF(in vec4 shadowTexCoord)
{
  float sum = 0.0;
  float y = -1.5;

  #define STEPS_COUNT 3

  for (int i=0; i<STEPS_COUNT; i++)
  { 
    float x = -1.5;

    for (int j=0; j<STEPS_COUNT; j++)
    {
      sum += OffsetLookup(shadowTexCoord, vec2(x, y));
      x += 1.5;
    }

    y += 1.5;
  }

  return sum / float (STEPS_COUNT * STEPS_COUNT);
}

// Tangent frame without screen-space derivatives (unavailable for ES 1.00 shaders here):
// build a stable basis from the geometric normal and world up. Not UV-aligned, but it gives
// consistent surface relief from the normal map.
mat3 TangentFrame(const in vec3 N)
{
  vec3 T = cross(vec3(0.0, 1.0, 0.0), N);
  T = dot(T, T) > 0.001 ? normalize(T) : vec3(1.0, 0.0, 0.0);
  vec3 B = cross(N, T);
  return mat3(T, B, N);
}

void main()
{
  vec3 position = position.xyz;
  vec4 diffuseColor = texture(diffuseTexture, texCoord);

  if (diffuseColor.a < 0.01)
    discard;

  vec3 normalizedEyeDirection = normalize(eyeDirection.xyz);

  mat3 tbn = TangentFrame(normalize(normal.xyz));

  vec3 mappedNormal = texture(normalTexture, texCoord).xyz * 2.0 - 1.0;
  mappedNormal.xy *= NORMAL_RELIEF; // stronger relief

  mappedNormal = normalize(tbn * mappedNormal);

  vec3 normal = mappedNormal;
  vec4 specular = texture(specularTexture, texCoord);
  vec3 eyeDirection = normalize(worldViewPosition - position);
  
  vec3 color = vec3(0);

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
    vec3 specularColor = ComputeSpecularColor(normal, lightDirection, eyeDirection, specular.xyz, specular.w) * SPECULAR_AMOUNT;

    color += lightColor * attenuation * (diffuseColor + specularColor);
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

      mat4 shadowMatrix = spotLightShadowMatrices[i];
      vec4 shadowTexCoord = shadowMatrix * vec4(position, 1.0);
      float shadowAttenuation = 1.0;

      if (shadowTexCoord.w > 0.0)
      {
        shadowTexCoord /= shadowTexCoord.w;

        shadowTexCoord = shadowTexCoord * 0.5 + 0.5;

        if (shadowTexCoord.x >= 0.0 &&
            shadowTexCoord.x <= 1.0 &&
            shadowTexCoord.y >= 0.0 &&
            shadowTexCoord.y <= 1.0)
        {
          shadowAttenuation = PCF(shadowTexCoord);
        }
      }
     
      vec3 diffuseColor = ComputeDiffuseColor(normal, lightDirection, diffuseColor.xyz) * DIFFUSE_AMOUNT;
      vec3 specularColor = ComputeSpecularColor(normal, lightDirection, eyeDirection, specular.xyz, specular.w) * SPECULAR_AMOUNT;

      color += lightColor * attenuation * shadowAttenuation * (diffuseColor + specularColor);
    }
  }
  
  outColor = vec4(color, 1.0);
  
#if DEBUG
  // debug output
  if (texCoord.x < 0.5 && texCoord.y < 0.5)
  {
    vec4 albedo = texture(albedoTexture, texCoord * 2.0);

    //outColor = vec4(albedo.xyz, 1.f);
  }
  else if (texCoord.y < 0.5)
  {
    //vec4 specular = texture(specularTexture, vec2((texCoord.x - 0.5) * 2.0, texCoord.y * 2.0));

    //outColor = vec4(specular.xyz, 1.f);
    vec4 shadow = texture(shadowTexture, vec2((texCoord.x - 0.5) * 2.0, texCoord.y * 2.0));

    outColor = vec4(shadow.x) * 0.5;
  }
  else if (texCoord.x < 0.5)
  {
    vec4 position = texture(positionTexture, vec2(texCoord.x * 2.0, (texCoord.y - 0.5) * 2.0));

    outColor = vec4(position.xyz, 1.f);
  }
  else
  {
    vec4 normal = texture(normalTexture, vec2((texCoord.x - 0.5) * 2.0, (texCoord.y - 0.5) * 2.0));

    outColor = vec4(normal.xyz, 1.f);
  }
#endif
}
