#shader vertex
#version 410 core

uniform mat4 MVP;
in vec4 vColor;
in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;
out vec3 eyeDirection;
out vec3 normal;
out vec4 color;
out vec2 texCoord;

const vec3 VIEW_POS = vec3(0.0, 0.0, -10.0); // camera_position

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  eyeDirection = VIEW_POS - gl_Position.xyz;
  normal = normalize (MVP * vec4 (vNormal, 0.0)).xyz;
  color = vColor;
  texCoord = vTexCoord;
}

#shader pixel
#version 410 core

in vec3 eyeDirection;
in vec3 normal;
in vec4 color;
in vec2 texCoord;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D specularTexture;
uniform float shininess;

out vec4 outColor;

const float MIN_DIFFUSE_AMOUNT = 0.1; // ambient light
const float DIFFUSE_AMOUNT = 1.0; // diffuse light multiplier
const float SPECULAR_AMOUNT = 1.0; // specular light multiplier
const vec3 LIGHT_DIR = vec3(0.0, 0.0, -1.0); // light direction

mat3 CotangentFrame(const in vec3 N, const in vec3 p, const in vec2 uv)
{
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx(p);
  vec3 dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);

  // solve the linear system
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    
  return mat3(T * invmax, B * invmax, N);
}

vec3 ComputeDiffuseColor(const in vec3 normal, const in vec3 lightDir, const in vec3 texDiffuseColor)
{
  return texDiffuseColor * max(dot(lightDir, normal), MIN_DIFFUSE_AMOUNT);
}

vec3 ComputeSpecularColor(const in vec3 normal, const in vec3 lightDir, const in vec3 eyeDir, in vec3 texSpecularColor)
{
  float specularFactor = pow(clamp(dot(reflect(-lightDir, normal), eyeDir), 0.00001, 1.0), shininess);

  return texSpecularColor * specularFactor;
}

void main()
{
  vec3 texDiffuseColor  = texture(diffuseTexture, texCoord).xyz;
  vec3 texSpecularColor = texture(specularTexture, texCoord).xyz;

  vec3 normalizedEyeDirection = normalize(eyeDirection);
  vec3 normalizedLightDirection = normalize(LIGHT_DIR);

  mat3 tbn = CotangentFrame(normalize(normal), -normalizedEyeDirection, texCoord);

  vec3 mappedNormal = texture(normalTexture, texCoord).xyz * 2.0 - 1.0;

  mappedNormal = normalize(tbn * mappedNormal);

  vec3 resultColor = vec3(0.0);

  resultColor += ComputeDiffuseColor(mappedNormal, normalizedLightDirection, texDiffuseColor) * DIFFUSE_AMOUNT;
  resultColor += ComputeSpecularColor(mappedNormal, normalizedLightDirection, normalizedEyeDirection, texSpecularColor) * SPECULAR_AMOUNT;

  outColor = vec4(resultColor, 1.0) * color;
}
